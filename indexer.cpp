#include <iostream>
#include "indexer.h"
#include <memory>
#include <algorithm>
#include <iterator>

#include <time.h>

const int get_c_string(PyObject *str, char* &ref);
const unsigned long hash_of_string(const char *c_str, const unsigned int len);

void
n_gramm::add_line(PyObject *index, PyObject *str)
{
	if (!PyString_Check(str)) {
		throw std::exception("Expected a string");
	}

	if (storage_.find(index) != storage_.end())
		return;

	storage_.insert({ index, str });
	add_del_index(index, str, true);
}

void
n_gramm::del_line(PyObject *index) 
{
	auto f = storage_.find(index);
	if (f == storage_.end())
		return;

	add_del_index(index, f->second, false);
	storage_.erase(f);
}


PyObject *
n_gramm::search(PyObject *pattern)
{
	char *c_str;
	const int size = get_c_string(pattern, c_str);

	CSTRList substrs;
	CSTRList n_gramms;
	select_substrs(n_gramms, substrs, c_str, size);

	PyObject *result = PyList_New(0);
	if (n_gramms.empty()) {
		return result;
	}

	std::sort(n_gramms.begin(), n_gramms.end(), [](const CSTR &lhs, const CSTR &rhs) {
		return lhs.second < rhs.second;
	});


	IndexValueSet intersection;
	bool is_init_intersection = true;

	for (auto &p : n_gramms) {
		const char *c = p.first;
		const unsigned int len = p.second;
		NIndex &hash_map = indexes_[len - 1];
		auto f = hash_map.find(hash_of_string(c, len));
		if (f == hash_map.end()) {
			return result;
		}

		IndexValueSet &resultSet = f->second;
		if (is_init_intersection) {
			intersection.swap(resultSet);
			is_init_intersection = false;
			continue;
		}

		IndexValueSet set_intersection_result;
		std::set_intersection(
			intersection.begin(), intersection.end(),
			resultSet.begin(), resultSet.end(),
			std::inserter(set_intersection_result, set_intersection_result.begin())
		);
		set_intersection_result.swap(intersection);

		if (intersection.empty()) {
			return result;
		}
	}

	unsigned int i = 0;
	for (auto &p : intersection) {
		PyObject *str = p.second;
		char * c_str;
		const unsigned int size = get_c_string(str, c_str);
		if (!is_real_substrs(substrs, c_str, size))
			continue;

		PyObject * tuple = PyTuple_New(2);
		PyTuple_SetItem(tuple, 0, p.first);
		PyTuple_SetItem(tuple, 1, str);
		PyList_Insert(result, i++, tuple);
	}

	return result;
}


bool n_gramm::is_real_substrs(CSTRList &substrs, char * c_str, const unsigned int size)
{
	bool is_there = true;
	if (substrs.size() > 1) {
		is_there = true;
		unsigned int offset = 0;
		for (const auto &p : substrs) {
			is_there = false;
			char *c_sub = p.first;
			unsigned int len = p.second;
			while (offset + len <= size) {
				if (std::equal(c_str + offset, c_str + offset + len, c_sub)) {
					is_there = true;
					offset += len;
					break;
				}
				offset++;
			}
			if (!is_there)
				break;
		}
	}

	return is_there;
}

void 
n_gramm::add_del_index(PyObject *index, PyObject *str, bool is_add)
{
	using namespace std;
	char *c_str;
	IndexValue value = std::make_pair(index, str);
	const unsigned int size = get_c_string(str, c_str);
	for (unsigned int pos = 0; pos < size; pos++)
		for (unsigned int len = 1; pos + len <= size && len <= n_count_; len++) {
			NIndex &hash_map = indexes_[len - 1];
			IndexKey hash = hash_of_string(&c_str[pos], len);
			auto f = hash_map.find(hash);
			if (f == hash_map.end()) {
				if (!is_add)
					continue;

				IndexValueSet new_value_set = { value };
				hash_map.insert({ hash, new_value_set });
				continue;
			}
			IndexValueSet &old_value_set = f->second;
			if (is_add)
				old_value_set.insert(value);
			else
				old_value_set.erase(value);
		}
}

void
n_gramm::create_n_gramms(CSTRList &n_gramms, char *c_str, const unsigned int size)
{
	const unsigned int len = std::min(size, n_count_);
	unsigned int pos = 0;
	while (true) {
		n_gramms.push_back(std::make_pair(&c_str[pos], len));
		const unsigned int end = pos + len;
		if (end == size)
			break;
		pos = std::min(end, size - len);
	}
}

void 
n_gramm::select_substrs(CSTRList &n_gramms, CSTRList &substr, char *c_str, const unsigned int size)
{
	bool prev_is_start = true;
	char *cur_c = nullptr;
	int len = 0;
	for (unsigned int i = 0; i < size; i++) {
		if (c_str[i] == '*') {
			if (!prev_is_start) {
				substr.push_back(std::make_pair(cur_c, len));
				create_n_gramms(n_gramms, cur_c, len);
			}
			prev_is_start = true;
		}
		else {
			if (prev_is_start) {
				cur_c = &c_str[i];
				len = 0;
			}
			prev_is_start = false;
			len++;
		}
	}
	if (!prev_is_start) {
		substr.push_back(std::make_pair(cur_c, len));
		create_n_gramms(n_gramms, cur_c, len);
	}
}
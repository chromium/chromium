// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_VALUE_COUNTER_H_
#define EXTENSIONS_COMMON_VALUE_COUNTER_H_

#include <stddef.h>

#include <vector>

namespace base {
class Value;
}

namespace extensions {

// Keeps a running count of Values, like map<Value, int>. Adding/removing
// values increments/decrements the count associated with a given Value.
//
// Add() and Remove() are linear in the number of Values in the ValueCounter,
// because there is no operator<() defined on Value, so we must iterate to find
// whether a Value is equal to an existing one.
class ValueCounter {
 public:
  ValueCounter();

  ValueCounter(const ValueCounter&) = delete;
  ValueCounter& operator=(const ValueCounter&) = delete;

  ~ValueCounter();

  // Adds `value` to the set. In the case where a Value equal to `value`
  // doesn't already exist in this map, this function makes a copy of `value`
  // and returns true. Otherwise, it returns false.
  bool Add(const base::Value& value);

  // Removes `value` from the set, and returns true if it removed the last
  // value equal to `value`. If there are more equal values, or if there
  // weren't any in the first place, returns false.
  bool Remove(const base::Value& value);

  // Returns true if there are no values of any type being counted.
  bool is_empty() const { return entries_.empty(); }

  // Returns the number of values being counted, including duplicates.
  size_t total_count() const { return total_count_; }

  // Returns the number of distinct values being counted.
  size_t distinct_count() const { return entries_.size(); }

 private:
  struct Entry;
  std::vector<Entry> entries_;
  size_t total_count_ = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VALUE_COUNTER_H_

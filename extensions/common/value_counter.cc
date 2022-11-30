// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/value_counter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/values.h"

namespace extensions {

struct ValueCounter::Entry {
  explicit Entry(base::Value value) : value(std::move(value)), count(1) {}

  Entry(Entry&) = delete;
  Entry& operator=(Entry&) = delete;

  Entry(Entry&&) = default;
  Entry& operator=(Entry&&) = default;

  base::Value value;
  int count;
};

ValueCounter::ValueCounter() {
}

ValueCounter::~ValueCounter() {
}

bool ValueCounter::Add(const base::Value& value) {
  for (auto& entry : entries_) {
    if (entry.value == value) {
      ++entry.count;
      return false;
    }
  }
  entries_.emplace_back(value.Clone());
  return true;
}

bool ValueCounter::Remove(const base::Value& value) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (it->value == value) {
      if (--it->count == 0) {
        std::swap(*it, entries_.back());
        entries_.pop_back();
        return true;  // Removed the last entry.
      }
      return false;  // Removed, but no the last entry.
    }
  }
  return false;  // Nothing to remove.
}

}  // namespace extensions

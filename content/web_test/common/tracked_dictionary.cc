// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/tracked_dictionary.h"

#include <utility>

namespace content {

TrackedDictionary::TrackedDictionary() {}

void TrackedDictionary::ResetChangeTracking() {
  changed_values_.clear();
}

void TrackedDictionary::ApplyUntrackedChanges(
    const base::Value::Dict& new_changes) {
  current_values_.Merge(new_changes.Clone());

  for (const auto [key, value] : new_changes) {
    changed_values_.Remove(key);
  }
}

void TrackedDictionary::Set(const std::string& path, base::Value new_value) {
  // Is this truly a *new* value?
  if (const base::Value* old_value = current_values_.FindByDottedPath(path)) {
    if (*old_value == new_value)
      return;
  }

  changed_values_.SetByDottedPath(path, new_value.Clone());
  current_values_.SetByDottedPath(path, std::move(new_value));
}

void TrackedDictionary::SetBoolean(const std::string& path, bool new_value) {
  Set(path, base::Value(new_value));
}

void TrackedDictionary::SetInteger(const std::string& path, int new_value) {
  Set(path, base::Value(new_value));
}

void TrackedDictionary::SetString(const std::string& path,
                                  const std::string& new_value) {
  Set(path, base::Value(new_value));
}

}  // namespace content

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/tracked_dictionary.h"

#include <string_view>
#include <utility>

namespace content {

TrackedDictionary::TrackedDictionary() {}

void TrackedDictionary::ResetChangeTracking() {
  changed_values_.clear();
}

void TrackedDictionary::ApplyUntrackedChanges(
    const base::DictValue& new_changes) {
  ApplyUntrackedChanges(new_changes.Clone());
}

void TrackedDictionary::ApplyUntrackedChanges(base::DictValue&& new_changes) {
  for (const auto [key, value] : new_changes) {
    changed_values_.Remove(key);
  }
  current_values_.Merge(std::move(new_changes));
}

void TrackedDictionary::Set(std::string_view path, base::Value new_value) {
  // Is this truly a *new* value?
  if (const base::Value* old_value = current_values_.FindByDottedPath(path)) {
    if (*old_value == new_value)
      return;
  }

  changed_values_.SetByDottedPath(path, new_value.Clone());
  current_values_.SetByDottedPath(path, std::move(new_value));
}

void TrackedDictionary::SetBoolean(std::string_view path, bool new_value) {
  Set(path, base::Value(new_value));
}

void TrackedDictionary::SetInteger(std::string_view path, int new_value) {
  Set(path, base::Value(new_value));
}

void TrackedDictionary::SetString(std::string_view path,
                                  std::string_view new_value) {
  Set(path, base::Value(new_value));
}

}  // namespace content

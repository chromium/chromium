// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/tracked_dictionary.h"

#include <utility>

namespace content {

TrackedDictionary::TrackedDictionary() {}

void TrackedDictionary::ResetChangeTracking() {
  changed_values_.DictClear();
}

void TrackedDictionary::ApplyUntrackedChanges(
    const base::DictionaryValue& new_changes) {
  current_values_.MergeDictionary(&new_changes);

  for (base::DictionaryValue::Iterator it(new_changes); !it.IsAtEnd();
       it.Advance()) {
    changed_values_.RemoveKey(it.key());
  }
}

void TrackedDictionary::Set(const std::string& path,
                            std::unique_ptr<base::Value> new_value) {
  // Is this truly a *new* value?
  const base::Value* old_value;
  if (current_values_.Get(path, &old_value)) {
    if (*old_value == *new_value)
      return;
  }

  changed_values_.SetKey(path, new_value->Clone());
  current_values_.SetKey(path,
                         base::Value::FromUniquePtrValue(std::move(new_value)));
}

void TrackedDictionary::SetBoolean(const std::string& path, bool new_value) {
  Set(path, std::make_unique<base::Value>(new_value));
}

void TrackedDictionary::SetString(const std::string& path,
                                  const std::string& new_value) {
  Set(path, std::make_unique<base::Value>(new_value));
}

}  // namespace content

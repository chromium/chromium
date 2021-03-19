// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_change.h"

#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"

base::Value ValueStoreChange::ToValue(ValueStoreChangeList changes) {
  base::Value changes_value(base::Value::Type::DICTIONARY);
  for (auto& change : changes) {
    base::Value change_value(base::Value::Type::DICTIONARY);
    if (change.old_value()) {
      change_value.SetKey("oldValue", std::move(*change.old_value_));
    }
    if (change.new_value()) {
      change_value.SetKey("newValue", std::move(*change.new_value_));
    }
    changes_value.SetKey(change.key(), std::move(change_value));
  }
  return changes_value;
}

ValueStoreChange::ValueStoreChange(const std::string& key,
                                   base::Optional<base::Value> old_value,
                                   base::Optional<base::Value> new_value)
    : key_(key),
      old_value_(std::move(old_value)),
      new_value_(std::move(new_value)) {}

ValueStoreChange::~ValueStoreChange() = default;

ValueStoreChange::ValueStoreChange(ValueStoreChange&& other) = default;
ValueStoreChange& ValueStoreChange::operator=(ValueStoreChange&& other) =
    default;

const base::Value* ValueStoreChange::old_value() const {
  return base::OptionalOrNullptr(old_value_);
}

const base::Value* ValueStoreChange::new_value() const {
  return base::OptionalOrNullptr(new_value_);
}



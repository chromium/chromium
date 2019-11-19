// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_change.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"

// static
std::string ValueStoreChange::ToJson(
    const ValueStoreChangeList& changes) {
  base::Value changes_value(base::Value::Type::DICTIONARY);
  for (const auto& change : changes) {
    base::Value change_value(base::Value::Type::DICTIONARY);
    if (change.old_value()) {
      change_value.SetKey("oldValue", change.old_value()->Clone());
    }
    if (change.new_value()) {
      change_value.SetKey("newValue", change.new_value()->Clone());
    }
    changes_value.SetKey(change.key(), std::move(change_value));
  }
  std::string json;
  bool success = base::JSONWriter::Write(changes_value, &json);
  DCHECK(success);
  return json;
}

ValueStoreChange::ValueStoreChange(const std::string& key,
                                   base::Optional<base::Value> old_value,
                                   base::Optional<base::Value> new_value)
    : inner_(new Inner(key, std::move(old_value), std::move(new_value))) {}

ValueStoreChange::ValueStoreChange(const ValueStoreChange& other) = default;

ValueStoreChange::~ValueStoreChange() {}

const std::string& ValueStoreChange::key() const {
  DCHECK(inner_.get());
  return inner_->key_;
}

const base::Value* ValueStoreChange::old_value() const {
  DCHECK(inner_.get());
  return inner_->old_value_ ? &*inner_->old_value_ : nullptr;
}

const base::Value* ValueStoreChange::new_value() const {
  DCHECK(inner_.get());
  return inner_->new_value_ ? &*inner_->new_value_ : nullptr;
}

ValueStoreChange::Inner::Inner(const std::string& key,
                               base::Optional<base::Value> old_value,
                               base::Optional<base::Value> new_value)
    : key_(key),
      old_value_(std::move(old_value)),
      new_value_(std::move(new_value)) {}

ValueStoreChange::Inner::~Inner() {}

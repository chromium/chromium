// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/value.h"

#include "base/hash.h"

namespace media {
namespace learning {

Value::Value(int x) : value_(x) {}
Value::Value(const char* x) {
  // std::hash would be nice, but it can (and does) change values between
  // different instances of the class.  In other words, Value("foo") !=
  // Value("foo") necessarily.
  if (x)
    value_ = base::PersistentHash(x, strlen(x));
}

Value::Value(const std::string& x) : value_(base::PersistentHash(x)) {}

Value::Value(const Value& other) : value_(other.value_) {}

bool Value::operator==(const Value& rhs) const {
  return value_ == rhs.value_;
}

bool Value::operator!=(const Value& rhs) const {
  return value_ != rhs.value_;
}

bool Value::operator<(const Value& rhs) const {
  return value_ < rhs.value_;
}

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return out << value.value_;
}

}  // namespace learning
}  // namespace media

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_COMMON_VALUE_H_
#define MEDIA_LEARNING_COMMON_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>

#include "base/component_export.h"

namespace media {
namespace learning {

// Lightweight, copyable value for features and labels.
// Strings are hashed into ints in an implementation-specific way, so don't
// count on ordering between them.  See LearningTask for more info about nominal
// versus numerical values.
//
// For numeric values, ==, !=, > operators behave as one would expect.
//
// For strings, only == and != are guaranteed to be meaningful.
class COMPONENT_EXPORT(LEARNING_COMMON) Value {
 public:
  Value();
  template <typename T>
  explicit Value(const T& x) : value_(x) {
    // We want to rule out mostly pointers, since they wouldn't make much sense.
    // Note that the implicit cast would likely fail anyway.
    static_assert(std::is_arithmetic<T>::value || std::is_enum<T>::value,
                  "media::learning::Value works only with arithmetic types");
  }

  explicit Value(const char* x);
  explicit Value(const std::string& x);
  explicit Value(bool x);

  Value(const Value& other);
  Value(Value&&) noexcept;

  Value& operator=(const Value&);
  Value& operator=(Value&&) noexcept;

  bool operator==(const Value& rhs) const;
  bool operator!=(const Value& rhs) const;
  bool operator<(const Value& rhs) const;
  bool operator>(const Value& rhs) const;

  double value() const { return value_; }

 private:
  double value_ = 0;

  friend COMPONENT_EXPORT(LEARNING_COMMON) std::ostream& operator<<(
      std::ostream& out,
      const Value& value);

  // Copy and assign are fine.
};

// Just to make it clearer what type of value we mean in context.
using FeatureValue = Value;
using TargetValue = Value;

COMPONENT_EXPORT(LEARNING_COMMON)
std::ostream& operator<<(std::ostream& out, const Value& value);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_COMMON_VALUE_H_

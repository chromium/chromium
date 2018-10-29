// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_LEARNING_VALUE_H_
#define MEDIA_LEARNING_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>

#include "media/base/media_export.h"

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
class MEDIA_EXPORT Value {
 public:
  explicit Value(int x);
  explicit Value(const char* x);
  explicit Value(const std::string& x);

  Value(const Value& other);

  bool operator==(const Value& rhs) const;
  bool operator!=(const Value& rhs) const;
  bool operator<(const Value& rhs) const;

 private:
  int64_t value_ = 0;

  friend MEDIA_EXPORT std::ostream& operator<<(std::ostream& out,
                                               const Value& value);

  // Copy and assign are fine.
};

// Just to make it clearer what type of value we mean in context.
using FeatureValue = Value;
using TargetValue = Value;

MEDIA_EXPORT std::ostream& operator<<(std::ostream& out, const Value& value);

}  // namespace learning
}  // namespace media

#endif  // MEDIA_LEARNING_VALUE_H_

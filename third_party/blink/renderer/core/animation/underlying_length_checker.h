// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_UNDERLYING_LENGTH_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_UNDERLYING_LENGTH_CHECKER_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/animation/interpolation_type.h"

namespace blink {

class UnderlyingLengthChecker : public InterpolationType::ConversionChecker {
 public:
  explicit UnderlyingLengthChecker(wtf_size_t underlying_length)
      : underlying_length_(underlying_length) {}

  static wtf_size_t GetUnderlyingLength(const InterpolationValue& underlying) {
    if (!underlying)
      return 0;
    return ToInterpolableList(*underlying.interpolable_value).length();
  }

  bool IsValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return underlying_length_ == GetUnderlyingLength(underlying);
  }

 private:
  wtf_size_t underlying_length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_UNDERLYING_LENGTH_CHECKER_H_

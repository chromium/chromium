// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/successful_position_fallback.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/style/position_try_fallbacks.h"

namespace blink {

bool SuccessfulPositionFallback::operator==(
    const SuccessfulPositionFallback& other) const {
  return base::ValuesEquivalent(position_try_fallbacks_,
                                other.position_try_fallbacks_) &&
         try_set_ == other.try_set_ && try_tactics_ == other.try_tactics_ &&
         index_ == other.index_;
}

void SuccessfulPositionFallback::Clear() {
  position_try_fallbacks_.Clear();
  try_set_.Clear();
  try_tactics_ = kNoTryTactics;
  index_ = std::nullopt;
}

void SuccessfulPositionFallback::Trace(Visitor* visitor) const {
  visitor->Trace(position_try_fallbacks_);
  visitor->Trace(try_set_);
}

}  // namespace blink

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/successful_position_option.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/style/position_try_options.h"

namespace blink {

bool SuccessfulPositionOption::operator==(
    const SuccessfulPositionOption& other) const {
  return base::ValuesEquivalent(position_try_options_,
                                other.position_try_options_) &&
         try_set_ == other.try_set_ && try_tactics_ == other.try_tactics_;
}

void SuccessfulPositionOption::Clear() {
  position_try_options_.Clear();
  try_set_.Clear();
  try_tactics_ = kNoTryTactics;
}

void SuccessfulPositionOption::Trace(Visitor* visitor) const {
  visitor->Trace(position_try_options_);
  visitor->Trace(try_set_);
}

}  // namespace blink

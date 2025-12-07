// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_grid_integer_repeat_value.h"

#include "third_party/blink/renderer/core/style/grid_area.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

String CSSGridIntegerRepeatValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("repeat(");
  result.Append(repetitions_->CssText());
  result.Append(", ");
  result.Append(CSSValueList::CustomCSSText());
  result.Append(')');
  return result.ReleaseString();
}

bool CSSGridIntegerRepeatValue::Equals(
    const CSSGridIntegerRepeatValue& other) const {
  return base::ValuesEquivalent(repetitions_, other.repetitions_) &&
         CSSValueList::Equals(other);
}

std::optional<wtf_size_t> CSSGridIntegerRepeatValue::GetRepetitionsIfKnown()
    const {
  std::optional<double> repetitions = repetitions_->GetValueIfKnown();
  if (!repetitions.has_value()) {
    return std::nullopt;
  }
  return ClampRepetitions(ClampTo<wtf_size_t>(*repetitions));
}

wtf_size_t CSSGridIntegerRepeatValue::ComputeRepetitions(
    const CSSLengthResolver& resolver) const {
  return ClampRepetitions(repetitions_->ComputeInteger(resolver));
}

wtf_size_t CSSGridIntegerRepeatValue::ClampRepetitions(
    wtf_size_t repetitions) const {
  repetitions = ClampTo<wtf_size_t>(repetitions, 0, kGridMaxTracks);
  if (extra_clamp_) {
    repetitions = std::min(repetitions, *extra_clamp_);
  }
  return repetitions;
}

}  // namespace cssvalue
}  // namespace blink

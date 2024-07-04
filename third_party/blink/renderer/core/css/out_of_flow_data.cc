// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/out_of_flow_data.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

void OutOfFlowData::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);
  visitor->Trace(last_successful_position_fallback_);
  visitor->Trace(new_successful_position_fallback_);
}

bool OutOfFlowData::SetPendingSuccessfulPositionFallback(
    const PositionTryFallbacks* fallbacks,
    const CSSPropertyValueSet* try_set,
    const TryTacticList& try_tactics,
    std::optional<size_t> index) {
  new_successful_position_fallback_.position_try_fallbacks_ = fallbacks;
  new_successful_position_fallback_.try_set_ = try_set;
  new_successful_position_fallback_.try_tactics_ = try_tactics;
  new_successful_position_fallback_.index_ = index;
  return last_successful_position_fallback_ !=
         new_successful_position_fallback_;
}

bool OutOfFlowData::ApplyPendingSuccessfulPositionFallback(
    LayoutObject* layout_object) {
  if (!new_successful_position_fallback_.IsEmpty()) {
    last_successful_position_fallback_ = new_successful_position_fallback_;
    new_successful_position_fallback_.Clear();
    // Last attempt resulted in new successful fallback, which means the
    // anchored element already has the correct layout.
    return false;
  }
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    // Element no longer renders as an OOF positioned. Clear last successful
    // position fallback, but no need for another layout since the previous
    // lifecycle update would not have applied a successful fallback.
    last_successful_position_fallback_.Clear();
    return false;
  }
  if (!last_successful_position_fallback_.IsEmpty() &&
      !base::ValuesEquivalent(
          last_successful_position_fallback_.position_try_fallbacks_.Get(),
          layout_object->StyleRef().GetPositionTryFallbacks().Get())) {
    // position-try-fallbacks changed which means the last successful fallback
    // is no longer valid. Clear and return true for a re-layout.
    last_successful_position_fallback_.Clear();
    return true;
  }
  return false;
}

void OutOfFlowData::ClearLastSuccessfulPositionFallback() {
  last_successful_position_fallback_.Clear();
  new_successful_position_fallback_.Clear();
}

bool OutOfFlowData::InvalidatePositionTryNames(
    const HashSet<AtomicString>& try_names) {
  if (HasLastSuccessfulPositionFallback()) {
    if (last_successful_position_fallback_.position_try_fallbacks_
            ->HasPositionTryName(try_names)) {
      ClearLastSuccessfulPositionFallback();
      return true;
    }
  }
  return false;
}

}  // namespace blink

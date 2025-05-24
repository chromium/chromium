// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/out_of_flow_data.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

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

bool OutOfFlowData::ApplyPendingSuccessfulPositionFallbackAndAnchorScrollShift(
    LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    // Element no longer renders as an OOF positioned. Clear last successful
    // position fallback, but no need for another layout since the previous
    // lifecycle update would not have applied a successful fallback.
    ResetAnchorData();
    return false;
  }

  if (!new_successful_position_fallback_.IsEmpty()) {
    // This is an anchor recalculation point, either because the box just got
    // displayed, or because we switched to a different position option.
    default_anchor_scroll_shift_ =
        PotentialNextDefaultAnchorScrollShift(*To<LayoutBox>(layout_object));

    last_successful_position_fallback_ = new_successful_position_fallback_;
    new_successful_position_fallback_.Clear();
    // Last attempt resulted in new successful fallback, which means the
    // anchored element already has the correct layout.
    return false;
  }

  if (!last_successful_position_fallback_.IsEmpty() &&
      !base::ValuesEquivalent(
          last_successful_position_fallback_.position_try_fallbacks_.Get(),
          layout_object->StyleRef().GetPositionTryFallbacks().Get())) {
    // position-try-fallbacks changed which means the last successful fallback
    // is no longer valid. Clear and return true for a re-layout.
    ResetAnchorData();
    return true;
  }
  return false;
}

void OutOfFlowData::ResetAnchorData() {
  last_successful_position_fallback_.Clear();
  new_successful_position_fallback_.Clear();
  default_anchor_scroll_shift_ = PhysicalOffset();
}

bool OutOfFlowData::InvalidatePositionTryNames(
    const HashSet<AtomicString>& try_names) {
  if (HasLastSuccessfulPositionFallback()) {
    if (last_successful_position_fallback_.position_try_fallbacks_
            ->HasPositionTryName(try_names)) {
      ResetAnchorData();
      return true;
    }
  }
  return false;
}

PhysicalOffset OutOfFlowData::PotentialNextDefaultAnchorScrollShift(
    const LayoutBox& layout_box) const {
  if (const AnchorPositionScrollData* scroll_data =
          layout_box.GetAnchorPositionScrollData()) {
    return scroll_data->TotalOffset();
  }
  return PhysicalOffset();
}

bool OutOfFlowData::HasStaleFallbackData(const LayoutBox& box) const {
  return !last_successful_position_fallback_.IsEmpty() &&
         !base::ValuesEquivalent(
             last_successful_position_fallback_.position_try_fallbacks_.Get(),
             box.StyleRef().GetPositionTryFallbacks().Get());
}

}  // namespace blink

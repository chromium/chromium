// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/out_of_flow_data.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

void OutOfFlowData::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);
  visitor->Trace(last_successful_position_fallback_);
  visitor->Trace(new_successful_position_fallback_);
  visitor->Trace(remembered_scroll_offsets_);
  visitor->Trace(pending_remembered_scroll_offsets_);
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

  if (pending_remembered_scroll_offsets_) {
    // TODO(vmpstr): Pending offsets were calculated at layout time. There may
    // have been adjustments made since (e.g. clamping). Should we check whether
    // the pending offsets match the current offsets for the same anchors and if
    // so, invalidate?
    remembered_scroll_offsets_ = pending_remembered_scroll_offsets_;
    pending_remembered_scroll_offsets_ = nullptr;
  }

  if (!new_successful_position_fallback_.IsEmpty()) {
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
  remembered_scroll_offsets_ = nullptr;
  pending_remembered_scroll_offsets_ = nullptr;
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

bool OutOfFlowData::HasStaleFallbackData(const LayoutBox& box) const {
  return !last_successful_position_fallback_.IsEmpty() &&
         !base::ValuesEquivalent(
             last_successful_position_fallback_.position_try_fallbacks_.Get(),
             box.StyleRef().GetPositionTryFallbacks().Get());
}

const OutOfFlowData::RememberedScrollOffsets*
OutOfFlowData::GetRememberedScrollOffsets() const {
  return remembered_scroll_offsets_;
}

const OutOfFlowData::RememberedScrollOffsets*
OutOfFlowData::GetSpeculativeRememberedScrollOffsets() const {
  return pending_remembered_scroll_offsets_ ? pending_remembered_scroll_offsets_
                                            : remembered_scroll_offsets_;
}

bool OutOfFlowData::SetPendingRememberedScrollOffsets(
    const RememberedScrollOffsets* offsets) {
  // TODO(vmpstr): Investigate the cases when offsets are nullptr.
  if (!offsets) {
    pending_remembered_scroll_offsets_ = nullptr;
    // If the current remembered offsets are not null, then we need an update.
    return !remembered_scroll_offsets_;
  }

  // If the offsets are already the remembered ones, then we don't need to do
  // anything.
  if (remembered_scroll_offsets_ && *remembered_scroll_offsets_ == *offsets) {
    return false;
  }
  pending_remembered_scroll_offsets_ = offsets;
  return true;
}

String OutOfFlowData::RememberedScrollOffsets::ToString() const {
  StringBuilder builder;
  builder.Append("RememberedScrollOffsets = [\n");
  for (const auto& offset : offsets_) {
    builder.Append("  ");
    builder.Append(offset.key->DebugName());
    builder.Append(": {for_layout: ");
    builder.Append(offset.value.scroll_offset_for_layout.ToString());
    builder.Append(", for_range_adjustment: ");
    builder.Append(offset.value.scroll_offset_for_range_adjustment.ToString());
    builder.Append("},\n");
  }
  builder.Append("]\n");
  return builder.ReleaseString();
}

}  // namespace blink

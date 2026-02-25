// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/line_clamp_data.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsLineClampData {
  int lines_until_clamp;
  LayoutUnit clamp_bfc_offset;
  UntracedMember<const LayoutObject> clamp_after_layout_object;
  int state;
};

ASSERT_SIZE(LineClampData, SameSizeAsLineClampData);

}  // namespace

CORE_EXPORT LineClampData::LineClampData(const LineClampData& o)
    : state(o.state) {
  switch (state) {
    case kDisabled:
      break;
    case kClampByLines:
      lines_until_clamp = o.lines_until_clamp;
      break;
    case kClampAfterLayoutObject:
      clamp_after_layout_object = o.clamp_after_layout_object;
      break;
    case kMeasureLinesUntilBfcOffset:
    case kClampByLinesWithBfcOffset:
      lines_until_clamp = o.lines_until_clamp;
      clamp_bfc_offset = o.clamp_bfc_offset;
      break;
  }
}

CORE_EXPORT LineClampData& LineClampData::operator=(const LineClampData& o) {
  if (state == kClampAfterLayoutObject && o.state != kClampAfterLayoutObject) {
    clamp_after_layout_object.Clear();
  }
  state = o.state;
  switch (state) {
    case kDisabled:
      break;
    case kClampByLines:
      lines_until_clamp = o.lines_until_clamp;
      break;
    case kClampAfterLayoutObject:
      clamp_after_layout_object = o.clamp_after_layout_object;
      break;
    case kMeasureLinesUntilBfcOffset:
    case kClampByLinesWithBfcOffset:
      lines_until_clamp = o.lines_until_clamp;
      clamp_bfc_offset = o.clamp_bfc_offset;
      break;
  }
  return *this;
}

LayoutUnit LineClampAncestorChain::InnerFinalLineClampBlockSize(
    LayoutUnit bfc_offset_override,
    LayoutUnit inflow_block_offset,
    MarginStrut margin_strut) const {
  LayoutUnit block_size = inflow_block_offset;
  if (end_border_padding_ || !parent_) {
    block_size += margin_strut.Sum() + end_border_padding_;
    margin_strut = MarginStrut();
  }

  if (parent_) {
    margin_strut.Append(end_margin_, /* is_quirky */ false);

    LayoutUnit bfc_offset = bfc_offset_.value_or(bfc_offset_override);
    LayoutUnit parent_bfc_offset =
        parent_->bfc_offset_.value_or(bfc_offset_override);
    return parent_->InnerFinalLineClampBlockSize(
        bfc_offset, bfc_offset + block_size - parent_bfc_offset, margin_strut);
  } else {
    DCHECK(bfc_offset_.has_value());
    DCHECK_EQ(*bfc_offset_, LayoutUnit());
    DCHECK(margin_strut.IsEmpty());
    return block_size;
  }
}

void LineClampAncestorChain::Trace(Visitor* visitor) const {
  visitor->Trace(parent_);
}

}  // namespace blink

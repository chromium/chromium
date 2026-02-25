// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/margin_strut.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
class LayoutObject;

struct LineClampData {
  DISALLOW_NEW();

  LineClampData() = default;

  CORE_EXPORT LineClampData(const LineClampData&);

  CORE_EXPORT LineClampData& operator=(const LineClampData&);

  enum State {
    kDisabled,
    kClampByLines,
    kClampAfterLayoutObject,
    kMeasureLinesUntilBfcOffset,
    kClampByLinesWithBfcOffset,
  };

  bool IsLineClampContext() const { return state != kDisabled; }

  bool IsClampByLines() const {
    return state == kClampByLines || state == kClampByLinesWithBfcOffset;
  }
  bool IsMeasureUntilBfcOffset() const {
    return state == kMeasureLinesUntilBfcOffset ||
           state == kClampByLinesWithBfcOffset;
  }

  std::optional<int> LinesUntilClamp(bool show_measured_lines = false) const {
    if (IsClampByLines() ||
        (show_measured_lines && state == kMeasureLinesUntilBfcOffset)) {
      return lines_until_clamp;
    }
    return std::optional<int>();
  }

  bool IsAtClampPoint() const {
    return IsClampByLines() && lines_until_clamp == 1;
  }

  bool IsPastClampPoint() const {
    return IsClampByLines() && lines_until_clamp <= 0;
  }

  bool ShouldHideForPaint() const {
    return RuntimeEnabledFeatures::CSSLineClampEnabled() && IsPastClampPoint();
  }

  bool operator==(const LineClampData& other) const {
    if (state != other.state) {
      return false;
    }
    switch (state) {
      case kClampByLines:
        return lines_until_clamp == other.lines_until_clamp;
      case kClampAfterLayoutObject:
        return clamp_after_layout_object == other.clamp_after_layout_object;
      case kMeasureLinesUntilBfcOffset:
      case kClampByLinesWithBfcOffset:
        return lines_until_clamp == other.lines_until_clamp &&
               clamp_bfc_offset == other.clamp_bfc_offset;
      default:
        return true;
    }
  }

  // If state == kClampByLines or kClampByLinesWithBfcOffset, the number of
  // lines until the clamp point. A value of 1 indicates the current line should
  // be clamped. May go negative.
  // With state == kMeasureLinesUntilBfcOffset, the number of lines found in the
  // BFC so far.
  int lines_until_clamp = 0;

  // The BFC offset where the current block container should clamp.
  // (Might not be the same BFC offset as other block containers in the same
  // BFC, depending on the bottom bmp).
  // Only valid if state == kMeasureLinesUntilBfcOffset or
  // kClampByLinesWithBfcOffset.
  LayoutUnit clamp_bfc_offset;

  // A LayoutObject immediately after which the container should clamp.
  // This is used to clamp after a lineless block when clamping by a height.
  //
  // This UntracedMember should not be dereferenced, it should only ever be used
  // to compare pointer equality.
  //
  // Even though it should not be dereferenced, we don't expect LineClampData
  // objects to live past the end of the layout phase; and the LayoutObject is
  // part of the input to that phase. So we can be somewhat confident that the
  // LayoutObject won't be GC'd and therefore that its address won't be reused
  // for a different LayoutObject during the LineClampData's lifetime. So using
  // it for pointer equality should not run into false positives.
  //
  // Only valid if state == kClampAfterLayoutObject.
  UntracedMember<const LayoutObject> clamp_after_layout_object;

  State state = kDisabled;
};

// This class is a linked list containing the data to compute the block size the
// line-clamp container would have if we clamped at a particular clamp point.
//
// If a `LineClampAncestorChain` instance has `parent` set to null, it
// represents the data for the line-clamp container. Otherwise, it represents
// the data for one of its descendent block boxes, and `parent` points to its
// parent's data.
//
// An instance of this class might be created when the corresponding block box's
// BFC offset hasn't been resolved yet. To deal with this, the `bfc_offset`
// field is mutable. If the BFC offset isn't known, calling `ResolveBfcOffset`
// will update that field with the resolved value for this node and its
// ancestors in the chain. (If it *is* known, calling `ResolveBfcOffset` will
// DCHECK that the passed offset is correct.)
class CORE_EXPORT LineClampAncestorChain final
    : public GarbageCollected<LineClampAncestorChain> {
 public:
  explicit LineClampAncestorChain(LayoutUnit end_border_padding)
      : bfc_offset_(LayoutUnit()), end_border_padding_(end_border_padding) {}
  LineClampAncestorChain(std::optional<LayoutUnit> bfc_offset,
                         LayoutUnit end_border_padding,
                         LayoutUnit end_margin,
                         const LineClampAncestorChain* parent)
      : bfc_offset_(bfc_offset),
        end_border_padding_(end_border_padding),
        end_margin_(end_margin),
        parent_(parent) {
    DCHECK(parent);
  }

  bool HasBfcOffset() const { return bfc_offset_.has_value(); }

  const LineClampAncestorChain* WithResolvedBfcOffset(
      LayoutUnit new_bfc_offset) const {
    if (bfc_offset_.has_value()) {
      DCHECK_EQ(*bfc_offset_, new_bfc_offset);
      return this;
    } else {
      return MakeGarbageCollected<LineClampAncestorChain>(
          new_bfc_offset, end_border_padding_, end_margin_, parent_);
    }
  }

  // Computes the block size that the line-clamp container would have for a
  // clamp point directly contained in the block box corresponding to this node,
  // with the passed inflow block offset and margin strut.
  LayoutUnit FinalLineClampBlockSize(LayoutUnit inflow_block_offset,
                                     MarginStrut margin_strut) const {
    DCHECK(bfc_offset_);
    return InnerFinalLineClampBlockSize(*bfc_offset_, inflow_block_offset,
                                        margin_strut);
  }

  void Trace(Visitor*) const;

  bool operator==(const LineClampAncestorChain& other) const {
    return bfc_offset_ == other.bfc_offset_ &&
           end_border_padding_ == other.end_border_padding_ &&
           end_margin_ == other.end_margin_ && parent_ == other.parent_;
  }

 private:
  LayoutUnit InnerFinalLineClampBlockSize(LayoutUnit bfc_offset_override,
                                          LayoutUnit inflow_block_offset,
                                          MarginStrut margin_strut) const;

  const std::optional<LayoutUnit> bfc_offset_;
  const LayoutUnit end_border_padding_;
  const LayoutUnit end_margin_;
  const Member<const LineClampAncestorChain> parent_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_

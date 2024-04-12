// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_INSET_AREA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_INSET_AREA_H_

#include <optional>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/anchor_query.h"
#include "third_party/blink/renderer/core/css/css_anchor_query_enums.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class WritingDirectionMode;

// Possible region end points for a computed <inset-area-span>
enum class InsetAreaRegion : uint8_t {
  kNone,
  kAll,
  kCenter,
  kStart,
  kEnd,
  kSelfStart,
  kSelfEnd,
  kInlineStart,
  kInlineEnd,
  kSelfInlineStart,
  kSelfInlineEnd,
  kBlockStart,
  kBlockEnd,
  kSelfBlockStart,
  kSelfBlockEnd,
  kTop,
  kBottom,
  kLeft,
  kRight,
  kXStart,
  kXEnd,
  kYStart,
  kYEnd,
  kXSelfStart,
  kXSelfEnd,
  kYSelfStart,
  kYSelfEnd,
};

// Represents the computed value for the inset-area property. Each span is
// represented by two end points. That is:
//
//   "span-all" -> (kAll, kAll)
//   "center" -> (kCenter, kCenter)
//   "span-right" -> (kCenter, kRight)
//   "span-left" -> (kLeft, kCenter)
//   "top" -> (kTop, kTop)
//
// The axes are not ordered in a particular block/inline or vertical/
// horizontal order because the axes will be resolved at layout time (see
// ToPhysical() below).
class CORE_EXPORT InsetArea {
  DISALLOW_NEW();

 public:
  InsetArea() = default;
  InsetArea(InsetAreaRegion span1_start,
            InsetAreaRegion span1_end,
            InsetAreaRegion span2_start,
            InsetAreaRegion span2_end)
      : span1_start_(span1_start),
        span1_end_(span1_end),
        span2_start_(span2_start),
        span2_end_(span2_end) {}

  InsetAreaRegion FirstStart() const { return span1_start_; }
  InsetAreaRegion FirstEnd() const { return span1_end_; }
  InsetAreaRegion SecondStart() const { return span2_start_; }
  InsetAreaRegion SecondEnd() const { return span2_end_; }

  bool operator==(const InsetArea& other) const {
    return span1_start_ == other.span1_start_ &&
           span1_end_ == other.span1_end_ &&
           span2_start_ == other.span2_start_ && span2_end_ == other.span2_end_;
  }
  bool operator!=(const InsetArea& other) const { return !(*this == other); }
  bool IsNone() const { return span1_start_ == InsetAreaRegion::kNone; }

  // Convert the computed inset-area into a physical representation where the
  // first span is always a top/center/bottom span, and the second is a
  // left/center/right span. If the inset-area is not valid, all regions will be
  // InsetAreaRegion::kNone.
  InsetArea ToPhysical(
      const WritingDirectionMode& container_writing_direction,
      const WritingDirectionMode& self_writing_direction) const;

  // Return anchor() functions to override auto inset values according to the
  // resolved inset-area. May only be called on InsetAreas returned from
  // ToPhysical() which ensures physical vertical / horizontal areas.
  // A return value of nullopt represents 0px rather than an anchor() function.
  std::optional<AnchorQuery> UsedTop() const;
  std::optional<AnchorQuery> UsedBottom() const;
  std::optional<AnchorQuery> UsedLeft() const;
  std::optional<AnchorQuery> UsedRight() const;

  // Anchored elements using inset area align towards the unused area through
  // different 'normal' behavior for align-self and justify-self. Compute the
  // alignments to be passed into ResolvedAlignSelf()/ResolvedJustifySelf().
  // Return value is an <align-self, justify-self> pair.
  std::pair<ItemPosition, ItemPosition> AlignJustifySelfFromPhysical(
      WritingDirectionMode container_writing_direction) const;

  // Made public because they are used in unit test expectations.
  static AnchorQuery AnchorTop();
  static AnchorQuery AnchorBottom();
  static AnchorQuery AnchorLeft();
  static AnchorQuery AnchorRight();

 private:
  InsetAreaRegion span1_start_ = InsetAreaRegion::kNone;
  InsetAreaRegion span1_end_ = InsetAreaRegion::kNone;
  InsetAreaRegion span2_start_ = InsetAreaRegion::kNone;
  InsetAreaRegion span2_end_ = InsetAreaRegion::kNone;
};

// Used to store inset offsets on ComputedStyle for adjusting the
// containing-block rectangle. All zeros means a span-all inset-area is applied.
// Non-zero values refer to an anchor edge offset relative to the containing
// block rectangle.
struct InsetAreaOffsets {
  std::optional<LayoutUnit> top;
  std::optional<LayoutUnit> bottom;
  std::optional<LayoutUnit> left;
  std::optional<LayoutUnit> right;

  bool operator==(const InsetAreaOffsets& other) const {
    return top == other.top && bottom == other.bottom && left == other.left &&
           right == other.right;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_INSET_AREA_H_

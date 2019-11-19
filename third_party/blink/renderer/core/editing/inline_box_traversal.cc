// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"

#include <unicode/ubidi.h>

#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/line/inline_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

// TODO(xiaochengh): Rename this file to |bidi_adjustment.cc|

namespace blink {

namespace {

// |AbstractInlineBox| provides abstraction of leaf nodes (text and atomic
// inlines) in both legacy and NG inline layout, so that the same bidi
// adjustment algorithm can be applied on both types of inline layout.
class AbstractInlineBox {
  STACK_ALLOCATED();

 public:
  AbstractInlineBox() : type_(InstanceType::kNull) {}

  explicit AbstractInlineBox(const InlineBox& box)
      : type_(InstanceType::kOldLayout), inline_box_(&box) {}
  explicit AbstractInlineBox(const NGInlineCursor& cursor)
      : type_(InstanceType::kNG),
        line_cursor_(CreateLineRootedCursor(cursor)) {}

  bool IsNotNull() const { return type_ != InstanceType::kNull; }
  bool IsNull() const { return !IsNotNull(); }
  bool IsOldLayout() const { return type_ == InstanceType::kOldLayout; }
  bool IsNG() const { return type_ == InstanceType::kNG; }

  bool operator==(const AbstractInlineBox& other) const {
    if (type_ != other.type_)
      return false;
    switch (type_) {
      case InstanceType::kNull:
        return true;
      case InstanceType::kOldLayout:
        return inline_box_ == other.inline_box_;
      case InstanceType::kNG:
        return line_cursor_ == other.line_cursor_;
    }
    NOTREACHED();
    return false;
  }

  // Returns containing block rooted cursor instead of line rooted cursor for
  // ease of handling, e.g. equiality check, move to next/previous line, etc.
  NGInlineCursor GetCursor() const {
    NGInlineCursor cursor;
    cursor.MoveTo(line_cursor_);
    return cursor;
  }

  const InlineBox& GetInlineBox() const {
    DCHECK(IsOldLayout());
    DCHECK(inline_box_);
    return *inline_box_;
  }

  UBiDiLevel BidiLevel() const {
    DCHECK(IsNotNull());
    return IsOldLayout() ? GetInlineBox().BidiLevel()
                         : line_cursor_.CurrentBidiLevel();
  }

  TextDirection Direction() const {
    DCHECK(IsNotNull());
    return IsOldLayout() ? GetInlineBox().Direction()
                         : line_cursor_.CurrentResolvedDirection();
  }

  AbstractInlineBox PrevLeafChild() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      const InlineBox* result = GetInlineBox().PrevLeafChild();
      return result ? AbstractInlineBox(*result) : AbstractInlineBox();
    }
    NGInlineCursor cursor(line_cursor_);
    cursor.MoveToPreviousInlineLeaf();
    return cursor ? AbstractInlineBox(cursor) : AbstractInlineBox();
  }

  AbstractInlineBox PrevLeafChildIgnoringLineBreak() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      const InlineBox* result = GetInlineBox().PrevLeafChildIgnoringLineBreak();
      return result ? AbstractInlineBox(*result) : AbstractInlineBox();
    }
    NGInlineCursor cursor(line_cursor_);
    cursor.MoveToPreviousInlineLeafIgnoringLineBreak();
    return cursor ? AbstractInlineBox(cursor) : AbstractInlineBox();
  }

  AbstractInlineBox NextLeafChild() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      const InlineBox* result = GetInlineBox().NextLeafChild();
      return result ? AbstractInlineBox(*result) : AbstractInlineBox();
    }
    NGInlineCursor cursor(line_cursor_);
    cursor.MoveToNextInlineLeaf();
    return cursor ? AbstractInlineBox(cursor) : AbstractInlineBox();
  }

  AbstractInlineBox NextLeafChildIgnoringLineBreak() const {
    DCHECK(IsNotNull());
    if (IsOldLayout()) {
      const InlineBox* result = GetInlineBox().NextLeafChildIgnoringLineBreak();
      return result ? AbstractInlineBox(*result) : AbstractInlineBox();
    }
    NGInlineCursor cursor(line_cursor_);
    cursor.MoveToNextInlineLeafIgnoringLineBreak();
    return cursor ? AbstractInlineBox(cursor) : AbstractInlineBox();
  }

  TextDirection ParagraphDirection() const {
    DCHECK(IsNotNull());
    if (IsOldLayout())
      return ParagraphDirectionOf(GetInlineBox());
    return GetLineBox(line_cursor_).CurrentBaseDirection();
  }

 private:
  static NGInlineCursor CreateLineRootedCursor(const NGInlineCursor& cursor) {
    NGInlineCursor line_cursor = GetLineBox(cursor).CursorForDescendants();
    line_cursor.MoveTo(cursor);
    return line_cursor;
  }

  // Returns containing line box of |cursor| even if |cursor| is scoped inside
  // line.
  static NGInlineCursor GetLineBox(const NGInlineCursor& cursor) {
    NGInlineCursor line_box;
    line_box.MoveTo(cursor);
    line_box.MoveToContainingLine();
    return line_box;
  }

  enum class InstanceType { kNull, kOldLayout, kNG };
  InstanceType type_;

  // Only one of |inline_box_| or |line_cursor_| is used, but we cannot make
  // them union because of non-trivial destructor.
  const InlineBox* inline_box_;

  // Because of |MoveToContainingLine()| isn't cheap and we avoid to call each
  // |MoveTo{Next,Previous}InlineLeaf()|, we hold containing line rooted cursor
  // instead of containing block rooted cursor.
  NGInlineCursor line_cursor_;
};

// |SideAffinity| represents the left or right side of a leaf inline
// box/fragment. For example, with text box/fragment "abc", "|abc" is the left
// side, and "abc|" is the right side.
enum SideAffinity { kLeft, kRight };

// Returns whether |box_position| is at the left or right side of its InlineBox.
SideAffinity GetSideAffinity(const InlineBoxPosition& box_position) {
  DCHECK(box_position.inline_box);
  const InlineBox* box = box_position.inline_box;
  const int offset = box_position.offset_in_box;
  DCHECK(offset == box->CaretLeftmostOffset() ||
         offset == box->CaretRightmostOffset());
  return offset == box->CaretLeftmostOffset() ? SideAffinity::kLeft
                                              : SideAffinity::kRight;
}

// Returns whether |caret_position| is at the start of its fragment.
bool IsAtFragmentStart(const NGCaretPosition& caret_position) {
  switch (caret_position.position_type) {
    case NGCaretPositionType::kBeforeBox:
      return true;
    case NGCaretPositionType::kAfterBox:
      return false;
    case NGCaretPositionType::kAtTextOffset:
      DCHECK(caret_position.text_offset.has_value());
      return *caret_position.text_offset ==
             caret_position.cursor.CurrentTextStartOffset();
  }
  NOTREACHED();
  return false;
}

// Returns whether |caret_position| is at the end of its fragment.
bool IsAtFragmentEnd(const NGCaretPosition& caret_position) {
  switch (caret_position.position_type) {
    case NGCaretPositionType::kBeforeBox:
      return false;
    case NGCaretPositionType::kAfterBox:
      return true;
    case NGCaretPositionType::kAtTextOffset:
      DCHECK(caret_position.text_offset.has_value());
      return *caret_position.text_offset ==
             caret_position.cursor.CurrentTextEndOffset();
  }
  NOTREACHED();
  return false;
}

// Returns whether |caret_position| is at the left or right side of fragment.
SideAffinity GetSideAffinity(const NGCaretPosition& caret_position) {
  DCHECK(!caret_position.IsNull());
  DCHECK(IsAtFragmentStart(caret_position) || IsAtFragmentEnd(caret_position));
  const bool is_at_start = IsAtFragmentStart(caret_position);
  const bool is_at_left_side =
      is_at_start == IsLtr(caret_position.cursor.CurrentResolvedDirection());
  return is_at_left_side ? SideAffinity::kLeft : SideAffinity::kRight;
}

// An abstraction of a caret position that is at the left or right side of a
// leaf inline box/fragment. The abstraction allows the object to be used in
// bidi adjustment algorithm for both legacy and NG.
class AbstractInlineBoxAndSideAffinity {
  STACK_ALLOCATED();

 public:
  AbstractInlineBoxAndSideAffinity(const AbstractInlineBox& box,
                                   SideAffinity side)
      : box_(box), side_(side) {
    DCHECK(box_.IsNotNull());
  }

  explicit AbstractInlineBoxAndSideAffinity(
      const InlineBoxPosition& box_position)
      : box_(*box_position.inline_box), side_(GetSideAffinity(box_position)) {
    DCHECK(box_position.inline_box);
  }

  explicit AbstractInlineBoxAndSideAffinity(
      const NGCaretPosition& caret_position)
      : box_(caret_position.cursor), side_(GetSideAffinity(caret_position)) {
    DCHECK(!caret_position.IsNull());
  }

  InlineBoxPosition ToInlineBoxPosition() const {
    DCHECK(box_.IsOldLayout());
    const InlineBox& inline_box = box_.GetInlineBox();
    return InlineBoxPosition(&inline_box,
                             side_ == SideAffinity::kLeft
                                 ? inline_box.CaretLeftmostOffset()
                                 : inline_box.CaretRightmostOffset());
  }

  NGCaretPosition ToNGCaretPosition() const {
    DCHECK(box_.IsNG());
    const bool is_at_start = IsLtr(box_.Direction()) == AtLeftSide();
    NGInlineCursor cursor(box_.GetCursor());

    if (!cursor.IsText()) {
      return {cursor,
              is_at_start ? NGCaretPositionType::kBeforeBox
                          : NGCaretPositionType::kAfterBox,
              base::nullopt};
    }

    return {cursor, NGCaretPositionType::kAtTextOffset,
            is_at_start ? cursor.CurrentTextStartOffset()
                        : cursor.CurrentTextEndOffset()};
  }

  PositionInFlatTree GetPosition() const {
    DCHECK(box_.IsNotNull());
    if (box_.IsNG())
      return ToPositionInFlatTree(ToNGCaretPosition().ToPositionInDOMTree());

    const InlineBoxPosition inline_box_position = ToInlineBoxPosition();
    const LineLayoutItem item =
        inline_box_position.inline_box->GetLineLayoutItem();
    const int text_start_offset =
        item.IsText() ? LineLayoutText(item).TextStartOffset() : 0;
    const int offset_in_node =
        text_start_offset + inline_box_position.offset_in_box;
    return PositionInFlatTree::EditingPositionOf(item.GetNode(),
                                                 offset_in_node);
  }

  AbstractInlineBox GetBox() const { return box_; }
  bool AtLeftSide() const { return side_ == SideAffinity::kLeft; }
  bool AtRightSide() const { return side_ == SideAffinity::kRight; }

 private:
  AbstractInlineBox box_;
  SideAffinity side_;
};

struct TraverseRight;

// "Left" traversal strategy
struct TraverseLeft {
  STATIC_ONLY(TraverseLeft);

  using Backwards = TraverseRight;

  static AbstractInlineBox Forward(const AbstractInlineBox& box) {
    return box.PrevLeafChild();
  }

  static AbstractInlineBox ForwardIgnoringLineBreak(
      const AbstractInlineBox& box) {
    return box.PrevLeafChildIgnoringLineBreak();
  }

  static AbstractInlineBox Backward(const AbstractInlineBox& box);
  static AbstractInlineBox BackwardIgnoringLineBreak(
      const AbstractInlineBox& box);

  static SideAffinity ForwardSideAffinity() { return SideAffinity::kLeft; }
};

// "Left" traversal strategy
struct TraverseRight {
  STATIC_ONLY(TraverseRight);

  using Backwards = TraverseLeft;

  static AbstractInlineBox Forward(const AbstractInlineBox& box) {
    return box.NextLeafChild();
  }

  static AbstractInlineBox ForwardIgnoringLineBreak(
      const AbstractInlineBox& box) {
    return box.NextLeafChildIgnoringLineBreak();
  }

  static AbstractInlineBox Backward(const AbstractInlineBox& box) {
    return Backwards::Forward(box);
  }

  static AbstractInlineBox BackwardIgnoringLineBreak(
      const AbstractInlineBox& box) {
    return Backwards::ForwardIgnoringLineBreak(box);
  }

  static SideAffinity ForwardSideAffinity() { return SideAffinity::kRight; }
};

// static
AbstractInlineBox TraverseLeft::Backward(const AbstractInlineBox& box) {
  return Backwards::Forward(box);
}

// static
AbstractInlineBox TraverseLeft::BackwardIgnoringLineBreak(
    const AbstractInlineBox& box) {
  return Backwards::ForwardIgnoringLineBreak(box);
}

template <typename TraversalStrategy>
using Backwards = typename TraversalStrategy::Backwards;

template <typename TraversalStrategy>
AbstractInlineBoxAndSideAffinity AbstractInlineBoxAndForwardSideAffinity(
    const AbstractInlineBox& box) {
  return AbstractInlineBoxAndSideAffinity(
      box, TraversalStrategy::ForwardSideAffinity());
}

template <typename TraversalStrategy>
AbstractInlineBoxAndSideAffinity AbstractInlineBoxAndBackwardSideAffinity(
    const AbstractInlineBox& box) {
  return AbstractInlineBoxAndForwardSideAffinity<Backwards<TraversalStrategy>>(
      box);
}

// Template algorithms for traversing in bidi runs

// Traverses from |start|, and returns the first box with bidi level less than
// or equal to |bidi_level| (excluding |start| itself). Returns a null box when
// such a box doesn't exist.
template <typename TraversalStrategy>
AbstractInlineBox FindBidiRun(const AbstractInlineBox& start,
                              unsigned bidi_level) {
  DCHECK(start.IsNotNull());
  for (AbstractInlineBox runner = TraversalStrategy::Forward(start);
       runner.IsNotNull(); runner = TraversalStrategy::Forward(runner)) {
    if (runner.BidiLevel() <= bidi_level)
      return runner;
  }
  return AbstractInlineBox();
}

// Traverses from |start|, and returns the last non-linebreak box with bidi
// level greater than |bidi_level| (including |start| itself).
template <typename TraversalStrategy>
AbstractInlineBox FindBoundaryOfBidiRunIgnoringLineBreak(
    const AbstractInlineBox& start,
    unsigned bidi_level) {
  DCHECK(start.IsNotNull());
  AbstractInlineBox last_runner = start;
  for (AbstractInlineBox runner =
           TraversalStrategy::ForwardIgnoringLineBreak(start);
       runner.IsNotNull();
       runner = TraversalStrategy::ForwardIgnoringLineBreak(runner)) {
    if (runner.BidiLevel() <= bidi_level)
      return last_runner;
    last_runner = runner;
  }
  return last_runner;
}

// Traverses from |start|, and returns the last box with bidi level greater than
// or equal to |bidi_level| (including |start| itself). Line break boxes may or
// may not be ignored, depending of the passed |forward| function.
AbstractInlineBox FindBoundaryOfEntireBidiRunInternal(
    const AbstractInlineBox& start,
    unsigned bidi_level,
    AbstractInlineBox (*forward)(const AbstractInlineBox&)) {
  DCHECK(start.IsNotNull());
  AbstractInlineBox last_runner = start;
  for (AbstractInlineBox runner = forward(start); runner.IsNotNull();
       runner = forward(runner)) {
    if (runner.BidiLevel() < bidi_level)
      return last_runner;
    last_runner = runner;
  }
  return last_runner;
}

// Variant of |FindBoundaryOfEntireBidiRun| preserving line break boxes.
template <typename TraversalStrategy>
AbstractInlineBox FindBoundaryOfEntireBidiRun(const AbstractInlineBox& start,
                                              unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRunInternal(start, bidi_level,
                                             TraversalStrategy::Forward);
}

// Variant of |FindBoundaryOfEntireBidiRun| ignoring line break boxes.
template <typename TraversalStrategy>
AbstractInlineBox FindBoundaryOfEntireBidiRunIgnoringLineBreak(
    const AbstractInlineBox& start,
    unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRunInternal(
      start, bidi_level, TraversalStrategy::ForwardIgnoringLineBreak);
}

// Adjustment algorithm at the end of caret position resolution.
template <typename TraversalStrategy>
class CaretPositionResolutionAdjuster {
  STATIC_ONLY(CaretPositionResolutionAdjuster);

 public:
  static AbstractInlineBoxAndSideAffinity UnadjustedCaretPosition(
      const AbstractInlineBox& box) {
    return AbstractInlineBoxAndBackwardSideAffinity<TraversalStrategy>(box);
  }

  // Returns true if |box| starts different direction of embedded text run.
  // See [1] for details.
  // [1] UNICODE BIDIRECTIONAL ALGORITHM, http://unicode.org/reports/tr9/
  static bool IsStartOfDifferentDirection(const AbstractInlineBox&);

  static AbstractInlineBoxAndSideAffinity AdjustForPrimaryDirectionAlgorithm(
      const AbstractInlineBox& box) {
    if (IsStartOfDifferentDirection(box))
      return UnadjustedCaretPosition(box);

    const unsigned level = TraversalStrategy::Backward(box).BidiLevel();
    const AbstractInlineBox forward_box =
        FindBidiRun<TraversalStrategy>(box, level);

    // For example, abc FED 123 ^ CBA when adjusting right side of 123
    if (forward_box.IsNotNull() && forward_box.BidiLevel() == level)
      return UnadjustedCaretPosition(box);

    // For example, abc 123 ^ CBA when adjusting right side of 123
    const AbstractInlineBox result_box =
        FindBoundaryOfEntireBidiRun<Backwards<TraversalStrategy>>(box, level);
    return AbstractInlineBoxAndBackwardSideAffinity<TraversalStrategy>(
        result_box);
  }

  static AbstractInlineBoxAndSideAffinity AdjustFor(
      const AbstractInlineBox& box) {
    DCHECK(box.IsNotNull());

    const TextDirection primary_direction = box.ParagraphDirection();
    if (box.Direction() == primary_direction)
      return AdjustForPrimaryDirectionAlgorithm(box);

    const unsigned char level = box.BidiLevel();
    const AbstractInlineBox backward_box =
        TraversalStrategy::BackwardIgnoringLineBreak(box);
    if (backward_box.IsNull() || backward_box.BidiLevel() < level) {
      // Backward side of a secondary run. Set to the forward side of the entire
      // run.
      const AbstractInlineBox result_box =
          FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraversalStrategy>(
              box, level);
      return AbstractInlineBoxAndForwardSideAffinity<TraversalStrategy>(
          result_box);
    }

    if (backward_box.BidiLevel() <= level)
      return UnadjustedCaretPosition(box);

    // Forward side of a "tertiary" run. Set to the backward side of that run.
    const AbstractInlineBox result_box =
        FindBoundaryOfBidiRunIgnoringLineBreak<Backwards<TraversalStrategy>>(
            box, level);
    return AbstractInlineBoxAndBackwardSideAffinity<TraversalStrategy>(
        result_box);
  }
};

// TODO(editing-dev): Try to unify the algorithms for both directions.
template <>
bool CaretPositionResolutionAdjuster<TraverseLeft>::IsStartOfDifferentDirection(
    const AbstractInlineBox& box) {
  DCHECK(box.IsNotNull());
  const AbstractInlineBox backward_box = TraverseRight::Forward(box);
  if (backward_box.IsNull())
    return true;
  return backward_box.BidiLevel() >= box.BidiLevel();
}

template <>
bool CaretPositionResolutionAdjuster<
    TraverseRight>::IsStartOfDifferentDirection(const AbstractInlineBox& box) {
  DCHECK(box.IsNotNull());
  const AbstractInlineBox backward_box = TraverseLeft::Forward(box);
  if (backward_box.IsNull())
    return true;
  if (backward_box.Direction() == box.Direction())
    return true;
  return backward_box.BidiLevel() > box.BidiLevel();
}

// Adjustment algorithm at the end of hit tests.
template <typename TraversalStrategy>
class HitTestAdjuster {
  STATIC_ONLY(HitTestAdjuster);

 public:
  static AbstractInlineBoxAndSideAffinity UnadjustedHitTestPosition(
      const AbstractInlineBox& box) {
    return AbstractInlineBoxAndBackwardSideAffinity<TraversalStrategy>(box);
  }

  static AbstractInlineBoxAndSideAffinity AdjustFor(
      const AbstractInlineBox& box) {
    // TODO(editing-dev): Fix handling of left on 12CBA
    if (box.Direction() == box.ParagraphDirection())
      return UnadjustedHitTestPosition(box);

    const UBiDiLevel level = box.BidiLevel();

    const AbstractInlineBox backward_box =
        TraversalStrategy::BackwardIgnoringLineBreak(box);
    if (backward_box.IsNotNull() && backward_box.BidiLevel() == level)
      return UnadjustedHitTestPosition(box);

    if (backward_box.IsNotNull() && backward_box.BidiLevel() > level) {
      // e.g. left of B in aDC12BAb when adjusting left side
      const AbstractInlineBox backward_most_box =
          FindBoundaryOfBidiRunIgnoringLineBreak<Backwards<TraversalStrategy>>(
              backward_box, level);
      return AbstractInlineBoxAndForwardSideAffinity<TraversalStrategy>(
          backward_most_box);
    }

    // backward_box.IsNull() || backward_box.BidiLevel() < level
    // e.g. left of D in aDC12BAb when adjusting left side
    const AbstractInlineBox forward_most_box =
        FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraversalStrategy>(box,
                                                                        level);
    return box.Direction() == forward_most_box.Direction()
               ? AbstractInlineBoxAndForwardSideAffinity<TraversalStrategy>(
                     forward_most_box)
               : AbstractInlineBoxAndBackwardSideAffinity<TraversalStrategy>(
                     forward_most_box);
  }
};

// Adjustment algorithm at the end of creating range selection
class RangeSelectionAdjuster {
  STATIC_ONLY(RangeSelectionAdjuster);

 public:
  static SelectionInFlatTree AdjustFor(
      const PositionInFlatTreeWithAffinity& visible_base,
      const PositionInFlatTreeWithAffinity& visible_extent) {
    const SelectionInFlatTree& unchanged_selection =
        SelectionInFlatTree::Builder()
            .SetBaseAndExtent(visible_base.GetPosition(),
                              visible_extent.GetPosition())
            .Build();

    if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled()) {
      if (NGInlineFormattingContextOf(visible_base.GetPosition()) ||
          NGInlineFormattingContextOf(visible_extent.GetPosition()))
        return unchanged_selection;
    }

    RenderedPosition base = RenderedPosition::Create(visible_base);
    RenderedPosition extent = RenderedPosition::Create(visible_extent);

    if (base.IsNull() || extent.IsNull() || base == extent ||
        (!base.AtBidiBoundary() && !extent.AtBidiBoundary())) {
      return unchanged_selection;
    }

    if (base.AtBidiBoundary()) {
      if (ShouldAdjustBaseAtBidiBoundary(base, extent)) {
        const PositionInFlatTree adjusted_base =
            CreateVisiblePosition(base.GetPosition()).DeepEquivalent();
        return SelectionInFlatTree::Builder()
            .SetBaseAndExtent(adjusted_base, visible_extent.GetPosition())
            .Build();
      }
      return unchanged_selection;
    }

    if (ShouldAdjustExtentAtBidiBoundary(base, extent)) {
      const PositionInFlatTree adjusted_extent =
          CreateVisiblePosition(extent.GetPosition()).DeepEquivalent();
      return SelectionInFlatTree::Builder()
          .SetBaseAndExtent(visible_base.GetPosition(), adjusted_extent)
          .Build();
    }

    return unchanged_selection;
  }

 private:
  class RenderedPosition {
    STACK_ALLOCATED();

   public:
    RenderedPosition() = default;
    static RenderedPosition Create(const PositionInFlatTreeWithAffinity&);

    bool IsNull() const { return box_.IsNull(); }
    bool operator==(const RenderedPosition& other) const {
      return box_ == other.box_ &&
             bidi_boundary_type_ == other.bidi_boundary_type_;
    }

    bool AtBidiBoundary() const {
      return bidi_boundary_type_ != BidiBoundaryType::kNotBoundary;
    }

    // Given |other|, which is a boundary of a bidi run, returns true if |this|
    // can be the other boundary of that run by checking some conditions.
    bool IsPossiblyOtherBoundaryOf(const RenderedPosition& other) const {
      DCHECK(other.AtBidiBoundary());
      if (!AtBidiBoundary())
        return false;
      if (bidi_boundary_type_ == other.bidi_boundary_type_)
        return false;
      return box_.BidiLevel() >= other.box_.BidiLevel();
    }

    // Callable only when |this| is at boundary of a bidi run. Returns true if
    // |other| is in that bidi run.
    bool BidiRunContains(const RenderedPosition& other) const {
      DCHECK(AtBidiBoundary());
      DCHECK(!other.IsNull());
      UBiDiLevel level = box_.BidiLevel();
      if (level > other.box_.BidiLevel())
        return false;
      const AbstractInlineBox boundary_of_other =
          bidi_boundary_type_ == BidiBoundaryType::kLeftBoundary
              ? FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraverseLeft>(
                    other.box_, level)
              : FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraverseRight>(
                    other.box_, level);
      return box_ == boundary_of_other;
    }

    PositionInFlatTree GetPosition() const {
      DCHECK(AtBidiBoundary());
      DCHECK(box_.IsNotNull());
      const SideAffinity side =
          bidi_boundary_type_ == BidiBoundaryType::kLeftBoundary
              ? SideAffinity::kLeft
              : SideAffinity::kRight;
      return AbstractInlineBoxAndSideAffinity(box_, side).GetPosition();
    }

   private:
    enum class BidiBoundaryType { kNotBoundary, kLeftBoundary, kRightBoundary };
    RenderedPosition(const AbstractInlineBox& box, BidiBoundaryType type)
        : box_(box), bidi_boundary_type_(type) {}

    static BidiBoundaryType GetPotentialBidiBoundaryType(
        const InlineBoxPosition& box_position) {
      DCHECK(box_position.inline_box);
      const InlineBox& box = *box_position.inline_box;
      const int offset = box_position.offset_in_box;
      if (offset == box.CaretLeftmostOffset())
        return BidiBoundaryType::kLeftBoundary;
      if (offset == box.CaretRightmostOffset())
        return BidiBoundaryType::kRightBoundary;
      return BidiBoundaryType::kNotBoundary;
    }

    static BidiBoundaryType GetPotentialBidiBoundaryType(
        const NGCaretPosition& caret_position) {
      DCHECK(!caret_position.IsNull());
      DCHECK(!RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
      if (!IsAtFragmentStart(caret_position) &&
          !IsAtFragmentEnd(caret_position))
        return BidiBoundaryType::kNotBoundary;
      return GetSideAffinity(caret_position) == SideAffinity::kLeft
                 ? BidiBoundaryType::kLeftBoundary
                 : BidiBoundaryType::kRightBoundary;
    }

    // Helper function for Create().
    static RenderedPosition CreateUncanonicalized(
        const PositionInFlatTreeWithAffinity& position) {
      if (position.IsNull() || !position.AnchorNode()->GetLayoutObject())
        return RenderedPosition();
      const PositionInFlatTreeWithAffinity adjusted =
          ComputeInlineAdjustedPosition(position);
      if (adjusted.IsNull())
        return RenderedPosition();

      if (NGInlineFormattingContextOf(adjusted.GetPosition())) {
        const NGCaretPosition caret_position = ComputeNGCaretPosition(adjusted);
        if (caret_position.IsNull())
          return RenderedPosition();
        return RenderedPosition(AbstractInlineBox(caret_position.cursor),
                                GetPotentialBidiBoundaryType(caret_position));
      }

      const InlineBoxPosition box_position = ComputeInlineBoxPosition(adjusted);
      if (!box_position.inline_box)
        return RenderedPosition();
      return RenderedPosition(AbstractInlineBox(*box_position.inline_box),
                              GetPotentialBidiBoundaryType(box_position));
    }

    AbstractInlineBox box_;
    BidiBoundaryType bidi_boundary_type_ = BidiBoundaryType::kNotBoundary;
  };

  static bool ShouldAdjustBaseAtBidiBoundary(const RenderedPosition& base,
                                             const RenderedPosition& extent) {
    DCHECK(base.AtBidiBoundary());
    if (extent.IsPossiblyOtherBoundaryOf(base))
      return false;
    return base.BidiRunContains(extent);
  }

  static bool ShouldAdjustExtentAtBidiBoundary(const RenderedPosition& base,
                                               const RenderedPosition& extent) {
    if (!extent.AtBidiBoundary())
      return false;
    return extent.BidiRunContains(base);
  }
};

RangeSelectionAdjuster::RenderedPosition
RangeSelectionAdjuster::RenderedPosition::Create(
    const PositionInFlatTreeWithAffinity& position) {
  const RenderedPosition uncanonicalized = CreateUncanonicalized(position);
  const BidiBoundaryType potential_type = uncanonicalized.bidi_boundary_type_;
  if (potential_type == BidiBoundaryType::kNotBoundary)
    return uncanonicalized;
  const AbstractInlineBox& box = uncanonicalized.box_;
  DCHECK(box.IsNotNull());

  // When at bidi boundary, ensure that |box_| belongs to the higher-level bidi
  // run.

  // For example, abc FED |ghi should be changed into abc FED| ghi
  if (potential_type == BidiBoundaryType::kLeftBoundary) {
    const AbstractInlineBox prev_box = box.PrevLeafChildIgnoringLineBreak();
    if (prev_box.IsNotNull() && prev_box.BidiLevel() > box.BidiLevel())
      return RenderedPosition(prev_box, BidiBoundaryType::kRightBoundary);
    BidiBoundaryType type =
        prev_box.IsNotNull() && prev_box.BidiLevel() == box.BidiLevel()
            ? BidiBoundaryType::kNotBoundary
            : BidiBoundaryType::kLeftBoundary;
    return RenderedPosition(box, type);
  }

  // potential_type == BidiBoundaryType::kRightBoundary
  // For example, abc| FED ghi should be changed into abc |FED ghi
  const AbstractInlineBox next_box = box.NextLeafChildIgnoringLineBreak();
  if (next_box.IsNotNull() && next_box.BidiLevel() > box.BidiLevel())
    return RenderedPosition(next_box, BidiBoundaryType::kLeftBoundary);
  BidiBoundaryType type =
      next_box.IsNotNull() && next_box.BidiLevel() == box.BidiLevel()
          ? BidiBoundaryType::kNotBoundary
          : BidiBoundaryType::kRightBoundary;
  return RenderedPosition(box, type);
}

}  // namespace

InlineBoxPosition BidiAdjustment::AdjustForCaretPositionResolution(
    const InlineBoxPosition& caret_position) {
  const AbstractInlineBoxAndSideAffinity unadjusted(caret_position);
  const AbstractInlineBoxAndSideAffinity adjusted =
      unadjusted.AtLeftSide()
          ? CaretPositionResolutionAdjuster<TraverseRight>::AdjustFor(
                unadjusted.GetBox())
          : CaretPositionResolutionAdjuster<TraverseLeft>::AdjustFor(
                unadjusted.GetBox());
  return adjusted.ToInlineBoxPosition();
}

NGCaretPosition BidiAdjustment::AdjustForCaretPositionResolution(
    const NGCaretPosition& caret_position) {
  DCHECK(!RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
  const AbstractInlineBoxAndSideAffinity unadjusted(caret_position);
  const AbstractInlineBoxAndSideAffinity adjusted =
      unadjusted.AtLeftSide()
          ? CaretPositionResolutionAdjuster<TraverseRight>::AdjustFor(
                unadjusted.GetBox())
          : CaretPositionResolutionAdjuster<TraverseLeft>::AdjustFor(
                unadjusted.GetBox());
  return adjusted.ToNGCaretPosition();
}

InlineBoxPosition BidiAdjustment::AdjustForHitTest(
    const InlineBoxPosition& caret_position) {
  const AbstractInlineBoxAndSideAffinity unadjusted(caret_position);
  const AbstractInlineBoxAndSideAffinity adjusted =
      unadjusted.AtLeftSide()
          ? HitTestAdjuster<TraverseRight>::AdjustFor(unadjusted.GetBox())
          : HitTestAdjuster<TraverseLeft>::AdjustFor(unadjusted.GetBox());
  return adjusted.ToInlineBoxPosition();
}

NGCaretPosition BidiAdjustment::AdjustForHitTest(
    const NGCaretPosition& caret_position) {
  DCHECK(!RuntimeEnabledFeatures::BidiCaretAffinityEnabled());
  const AbstractInlineBoxAndSideAffinity unadjusted(caret_position);
  const AbstractInlineBoxAndSideAffinity adjusted =
      unadjusted.AtLeftSide()
          ? HitTestAdjuster<TraverseRight>::AdjustFor(unadjusted.GetBox())
          : HitTestAdjuster<TraverseLeft>::AdjustFor(unadjusted.GetBox());
  return adjusted.ToNGCaretPosition();
}

SelectionInFlatTree BidiAdjustment::AdjustForRangeSelection(
    const PositionInFlatTreeWithAffinity& base,
    const PositionInFlatTreeWithAffinity& extent) {
  return RangeSelectionAdjuster::AdjustFor(base, extent);
}

// TODO(xiaochengh): Move this function to a better place
TextDirection ParagraphDirectionOf(const InlineBox& box) {
  const ComputedStyle& block_style = *box.Root().Block().Style();
  if (block_style.GetUnicodeBidi() != UnicodeBidi::kPlaintext)
    return block_style.Direction();

  // There is no reliable way to get the paragraph direction in legacy
  // layout when 'unicode-bidi: plaintext' is set. Use the lowest-level
  // inline box's direction as a workaround.
  UBiDiLevel min_level = 128;
  for (const InlineBox* runner = box.Root().FirstLeafChild(); runner;
       runner = runner->NextLeafChild()) {
    min_level = std::min(min_level, runner->BidiLevel());
  }
  return DirectionFromLevel(min_level);
}

}  // namespace blink

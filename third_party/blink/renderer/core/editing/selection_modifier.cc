/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/ng_flat_tree_shorthands.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"
#include "third_party/blink/renderer/core/layout/inline/offset_mapping.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/physical_fragment.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// There are some cases where |SelectionModifier::ModifyWithPageGranularity()|
// enters an infinite loop. Work around it by hard-limiting the iteration.
const unsigned kMaxIterationForPageGranularityMovement = 1024;

VisiblePositionInFlatTree LeftBoundaryOfLine(const VisiblePositionInFlatTree& c,
                                             TextDirection direction) {
  DCHECK(c.IsValid()) << c;
  return direction == TextDirection::kLtr ? LogicalStartOfLine(c)
                                          : LogicalEndOfLine(c);
}

VisiblePositionInFlatTree RightBoundaryOfLine(
    const VisiblePositionInFlatTree& c,
    TextDirection direction) {
  DCHECK(c.IsValid()) << c;
  return direction == TextDirection::kLtr ? LogicalEndOfLine(c)
                                          : LogicalStartOfLine(c);
}

}  // namespace

static bool InSameParagraph(const VisiblePositionInFlatTree& a,
                            const VisiblePositionInFlatTree& b,
                            EditingBoundaryCrossingRule boundary_crossing_rule =
                                kCannotCrossEditingBoundary) {
  DCHECK(a.IsValid()) << a;
  DCHECK(b.IsValid()) << b;
  return a.IsNotNull() &&
         StartOfParagraph(a, boundary_crossing_rule).DeepEquivalent() ==
             StartOfParagraph(b, boundary_crossing_rule).DeepEquivalent();
}

// static
VisiblePositionInFlatTree SelectionModifier::PreviousParagraphPosition(
    const VisiblePositionInFlatTree& passed_position,
    LayoutUnit x_point) {
  VisiblePositionInFlatTree position = passed_position;
  do {
    DCHECK(position.IsValid()) << position;
    const VisiblePositionInFlatTree& new_position = CreateVisiblePosition(
        PreviousLinePosition(position.ToPositionWithAffinity(), x_point));
    if (new_position.IsNull() ||
        new_position.DeepEquivalent() == position.DeepEquivalent())
      break;
    position = new_position;
  } while (InSameParagraph(
      passed_position, position,
      RuntimeEnabledFeatures::ModifyParagraphCrossEditingoundaryEnabled()
          ? kCanCrossEditingBoundary
          : kCannotCrossEditingBoundary));
  return position;
}

// static
VisiblePositionInFlatTree SelectionModifier::NextParagraphPosition(
    const VisiblePositionInFlatTree& passed_position,
    LayoutUnit x_point) {
  VisiblePositionInFlatTree position = passed_position;
  do {
    DCHECK(position.IsValid()) << position;
    const VisiblePositionInFlatTree& new_position = CreateVisiblePosition(
        NextLinePosition(position.ToPositionWithAffinity(), x_point));
    if (new_position.IsNull() ||
        new_position.DeepEquivalent() == position.DeepEquivalent())
      break;
    position = new_position;
  } while (InSameParagraph(
      passed_position, position,
      RuntimeEnabledFeatures::ModifyParagraphCrossEditingoundaryEnabled()
          ? kCanCrossEditingBoundary
          : kCannotCrossEditingBoundary));
  return position;
}

LayoutUnit NoXPosForVerticalArrowNavigation() {
  return LayoutUnit::Min();
}

bool SelectionModifier::ShouldAlwaysUseDirectionalSelection(
    const LocalFrame& frame) {
  return frame.GetEditor().Behavior().ShouldConsiderSelectionAsDirectional();
}

SelectionModifier::SelectionModifier(
    const LocalFrame& frame,
    const SelectionInDOMTree& selection,
    LayoutUnit x_pos_for_vertical_arrow_navigation)
    : frame_(&frame),
      current_selection_(ConvertToSelectionInFlatTree(selection)),
      x_pos_for_vertical_arrow_navigation_(
          x_pos_for_vertical_arrow_navigation) {}

SelectionModifier::SelectionModifier(const LocalFrame& frame,
                                     const SelectionInDOMTree& selection)
    : SelectionModifier(frame, selection, NoXPosForVerticalArrowNavigation()) {}

VisibleSelection SelectionModifier::Selection() const {
  return CreateVisibleSelection(
      ConvertToSelectionInDOMTree(current_selection_));
}

static VisiblePositionInFlatTree ComputeVisibleFocus(
    const VisibleSelectionInFlatTree& visible_selection) {
  return CreateVisiblePosition(visible_selection.Focus(),
                               visible_selection.Affinity());
}

TextDirection SelectionModifier::DirectionOfEnclosingBlock() const {
  const PositionInFlatTree& selection_focus = selection_.Focus();

  // TODO(editing-dev): Check for PositionInFlatTree::IsNotNull is an easy fix
  // for few editing/ web tests, that didn't expect that (e.g.
  // editing/selection/extend-byline-withfloat.html).
  // That should be fixed in a more appropriate manner.
  // We should either have SelectionModifier aborted earlier for null selection,
  // or do not allow null selection in SelectionModifier at all.
  return selection_focus.IsNotNull()
             ? DirectionOfEnclosingBlockOf(selection_focus)
             : TextDirection::kLtr;
}

namespace {

std::optional<TextDirection> DirectionAt(
    const PositionInFlatTreeWithAffinity& position) {
  if (position.IsNull())
    return std::nullopt;
  const PositionInFlatTreeWithAffinity adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return std::nullopt;

  if (NGInlineFormattingContextOf(adjusted.GetPosition())) {
    const InlineCursor& cursor = ComputeInlineCaretPosition(adjusted).cursor;
    if (cursor)
      return cursor.Current().ResolvedDirection();
    return std::nullopt;
  }

  return std::nullopt;
}

// TODO(xiaochengh): Deduplicate code with |DirectionAt()|.
std::optional<TextDirection> LineDirectionAt(
    const PositionInFlatTreeWithAffinity& position) {
  if (position.IsNull())
    return std::nullopt;
  const PositionInFlatTreeWithAffinity adjusted =
      ComputeInlineAdjustedPosition(position);
  if (adjusted.IsNull())
    return std::nullopt;

  if (NGInlineFormattingContextOf(adjusted.GetPosition())) {
    InlineCursor line = ComputeInlineCaretPosition(adjusted).cursor;
    if (!line)
      return std::nullopt;
    line.MoveToContainingLine();
    return line.Current().BaseDirection();
  }

  return std::nullopt;
}

TextDirection DirectionOf(const VisibleSelectionInFlatTree& visible_selection) {
  std::optional<TextDirection> maybe_start_direction =
      DirectionAt(visible_selection.VisibleStart().ToPositionWithAffinity());
  std::optional<TextDirection> maybe_end_direction =
      DirectionAt(visible_selection.VisibleEnd().ToPositionWithAffinity());
  if (maybe_start_direction.has_value() && maybe_end_direction.has_value() &&
      maybe_start_direction.value() == maybe_end_direction.value())
    return maybe_start_direction.value();

  return DirectionOfEnclosingBlockOf(visible_selection.Focus());
}

}  // namespace

TextDirection SelectionModifier::DirectionOfSelection() const {
  return DirectionOf(selection_);
}

TextDirection SelectionModifier::LineDirectionOfFocus() const {
  return LineDirectionAt(selection_.VisibleFocus().ToPositionWithAffinity())
      .value_or(DirectionOfEnclosingBlockOf(selection_.Focus()));
}

static bool IsAnchorStart(const VisibleSelectionInFlatTree& visible_selection,
                          SelectionModifyDirection direction) {
  switch (direction) {
    case SelectionModifyDirection::kRight:
      return DirectionOf(visible_selection) == TextDirection::kLtr;
    case SelectionModifyDirection::kForward:
      return true;
    case SelectionModifyDirection::kLeft:
      return DirectionOf(visible_selection) != TextDirection::kLtr;
    case SelectionModifyDirection::kBackward:
      return false;
  }
  NOTREACHED_IN_MIGRATION()
      << "We should handle " << static_cast<int>(direction);
  return true;
}

// This function returns |VisibleSelectionInFlatTree| from start and end
// position of current_selection_'s |VisibleSelectionInFlatTree| with
// |direction| and ordering of anchor and focus to handle anchor/focus don't
// match to start/end, e.g. granularity
// != character, and start/end adjustment in
// |VisibleSelectionInFlatTree::validate()| for range selection.
VisibleSelectionInFlatTree SelectionModifier::PrepareToModifySelection(
    SelectionModifyAlteration alter,
    SelectionModifyDirection direction) const {
  const VisibleSelectionInFlatTree& visible_selection =
      CreateVisibleSelection(current_selection_);
  if (alter != SelectionModifyAlteration::kExtend)
    return visible_selection;
  if (visible_selection.IsNone())
    return visible_selection;

  const EphemeralRangeInFlatTree& range =
      visible_selection.AsSelection().ComputeRange();
  if (range.IsCollapsed())
    return visible_selection;
  SelectionInFlatTree::Builder builder;
  // Make anchor and focus match start and end so we extend the user-visible
  // selection. This only matters for cases where anchor and focus point to
  // different positions than start and end (e.g. after a double-click to
  // select a word).
  const bool anchor_is_start =
      selection_is_directional_ ? visible_selection.IsAnchorFirst()
                                : IsAnchorStart(visible_selection, direction);
  if (anchor_is_start) {
    builder.SetAsForwardSelection(range);
  } else {
    builder.SetAsBackwardSelection(range);
  }
  return CreateVisibleSelection(builder.Build());
}

VisiblePositionInFlatTree SelectionModifier::PositionForPlatform(
    bool is_get_start) const {
  Settings* settings = GetFrame().GetSettings();
  if (settings && settings->GetEditingBehaviorType() ==
                      mojom::blink::EditingBehavior::kEditingMacBehavior)
    return is_get_start ? selection_.VisibleStart() : selection_.VisibleEnd();
  // Linux and Windows always extend selections from the focus endpoint.
  // FIXME: VisibleSelectionInFlatTree should be fixed to ensure as an invariant
  // that anchor/focus always point to the same nodes as start/end, but which
  // points to which depends on the value of IsAnchorFirst. Then this can be
  // changed to just return selection_.Focus().
  return selection_.IsAnchorFirst() ? selection_.VisibleEnd()
                                    : selection_.VisibleStart();
}

VisiblePositionInFlatTree SelectionModifier::StartForPlatform() const {
  return PositionForPlatform(true);
}

VisiblePositionInFlatTree SelectionModifier::EndForPlatform() const {
  return PositionForPlatform(false);
}

PositionInFlatTree SelectionModifier::NextWordPositionForPlatform(
    const PositionInFlatTree& original_position) {
  const PlatformWordBehavior platform_word_behavior =
      GetFrame().GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight()
          ? PlatformWordBehavior::kWordSkipSpaces
          : PlatformWordBehavior::kWordDontSkipSpaces;
  // Next word position can't be upstream.
  const PositionInFlatTree position_after_current_word =
      NextWordPosition(original_position, platform_word_behavior).GetPosition();

  return position_after_current_word;
}

static VisiblePositionInFlatTree AdjustForwardPositionForUserSelectAll(
    const VisiblePositionInFlatTree& position) {
  Node* const root_user_select_all = EditingStrategy::RootUserSelectAllForNode(
      position.DeepEquivalent().AnchorNode());
  if (!root_user_select_all)
    return position;
  return CreateVisiblePosition(MostForwardCaretPosition(
      PositionInFlatTree::AfterNode(*root_user_select_all),
      kCanCrossEditingBoundary));
}

static VisiblePositionInFlatTree AdjustBackwardPositionForUserSelectAll(
    const VisiblePositionInFlatTree& position) {
  Node* const root_user_select_all = EditingStrategy::RootUserSelectAllForNode(
      position.DeepEquivalent().AnchorNode());
  if (!root_user_select_all)
    return position;
  return CreateVisiblePosition(MostBackwardCaretPosition(
      PositionInFlatTree::BeforeNode(*root_user_select_all),
      kCanCrossEditingBoundary));
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingRightInternal(
    TextGranularity granularity) {
  // The difference between modifyExtendingRight and modifyExtendingForward is:
  // modifyExtendingForward always extends forward logically.
  // modifyExtendingRight behaves the same as modifyExtendingForward except for
  // extending character or word, it extends forward logically if the enclosing
  // block is LTR direction, but it extends backward logically if the enclosing
  // block is RTL direction.
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return NextPositionOf(ComputeVisibleFocus(selection_),
                              kCanSkipOverEditingBoundary);
      }
      return PreviousPositionOf(ComputeVisibleFocus(selection_),
                                kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return CreateVisiblePosition(NextWordPositionForPlatform(
            ComputeVisibleFocus(selection_).DeepEquivalent()));
      }
      return CreateVisiblePosition(PreviousWordPosition(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
    case TextGranularity::kLineBoundary:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        return ModifyExtendingForwardInternal(granularity);
      return ModifyExtendingBackwardInternal(granularity);
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      // TODO(editing-dev): implement all of the above?
      return ModifyExtendingForwardInternal(granularity);
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingRight(
    TextGranularity granularity) {
  const VisiblePositionInFlatTree& pos =
      ModifyExtendingRightInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustForwardPositionForUserSelectAll(pos);
  return AdjustBackwardPositionForUserSelectAll(pos);
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingForwardInternal(
    TextGranularity granularity) {
  switch (granularity) {
    case TextGranularity::kCharacter:
      return NextPositionOf(ComputeVisibleFocus(selection_),
                            kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      return CreateVisiblePosition(NextWordPositionForPlatform(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
    case TextGranularity::kSentence:
      return CreateVisiblePosition(
          NextSentencePosition(
              ComputeVisibleFocus(selection_).DeepEquivalent()),
          TextAffinity::kUpstreamIfPossible);
    case TextGranularity::kLine: {
      const VisiblePositionInFlatTree& pos = ComputeVisibleFocus(selection_);
      DCHECK(pos.IsValid()) << pos;
      return CreateVisiblePosition(NextLinePosition(
          pos.ToPositionWithAffinity(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Focus())));
    }
    case TextGranularity::kParagraph:
      return NextParagraphPosition(
          ComputeVisibleFocus(selection_),
          LineDirectionPointForBlockDirectionNavigation(selection_.Focus()));
    case TextGranularity::kSentenceBoundary:
      return EndOfSentence(EndForPlatform());
    case TextGranularity::kLineBoundary:
      return LogicalEndOfLine(EndForPlatform());
    case TextGranularity::kParagraphBoundary:
      return EndOfParagraph(EndForPlatform());
    case TextGranularity::kDocumentBoundary: {
      const VisiblePositionInFlatTree& pos = EndForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent())) {
        DCHECK(pos.IsValid()) << pos;
        return CreateVisiblePosition(
            EndOfEditableContent(pos.DeepEquivalent()));
      }
      return EndOfDocument(pos);
    }
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingForward(
    TextGranularity granularity) {
  const VisiblePositionInFlatTree pos =
      ModifyExtendingForwardInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustForwardPositionForUserSelectAll(pos);
  return AdjustBackwardPositionForUserSelectAll(pos);
}

VisiblePositionInFlatTree SelectionModifier::ModifyMovingRight(
    TextGranularity granularity) {
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (!selection_.IsRange()) {
        if (LineDirectionOfFocus() == TextDirection::kLtr) {
          return ModifyMovingForward(granularity);
        }
        return ModifyMovingBackward(granularity);
      }
      if (DirectionOfSelection() == TextDirection::kLtr)
        return CreateVisiblePosition(selection_.End(), selection_.Affinity());
      return CreateVisiblePosition(selection_.Start(), selection_.Affinity());
    case TextGranularity::kWord:
      if (LineDirectionOfFocus() == TextDirection::kLtr) {
        return ModifyMovingForward(granularity);
      }
      return ModifyMovingBackward(granularity);
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      // TODO(editing-dev): Implement all of the above.
      return ModifyMovingForward(granularity);
    case TextGranularity::kLineBoundary:
      return RightBoundaryOfLine(StartForPlatform(),
                                 DirectionOfEnclosingBlock());
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyMovingForward(
    TextGranularity granularity) {
  // TODO(editing-dev): Stay in editable content for the less common
  // granularities.
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (selection_.IsRange())
        return CreateVisiblePosition(selection_.End(), selection_.Affinity());
      return NextPositionOf(ComputeVisibleFocus(selection_),
                            kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      return CreateVisiblePosition(NextWordPositionForPlatform(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
    case TextGranularity::kSentence:
      return CreateVisiblePosition(
          NextSentencePosition(
              ComputeVisibleFocus(selection_).DeepEquivalent()),
          TextAffinity::kUpstreamIfPossible);
    case TextGranularity::kLine: {
      // down-arrowing from a range selection that ends at the start of a line
      // needs to leave the selection at that line start (no need to call
      // nextLinePosition!)
      const VisiblePositionInFlatTree& pos = EndForPlatform();
      if (selection_.IsRange() && IsStartOfLine(pos))
        return pos;
      DCHECK(pos.IsValid()) << pos;
      return CreateVisiblePosition(NextLinePosition(
          pos.ToPositionWithAffinity(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start())));
    }
    case TextGranularity::kParagraph:
      return NextParagraphPosition(
          EndForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start()));
    case TextGranularity::kSentenceBoundary:
      return EndOfSentence(EndForPlatform());
    case TextGranularity::kLineBoundary:
      return LogicalEndOfLine(EndForPlatform());
    case TextGranularity::kParagraphBoundary:
      return EndOfParagraph(
          EndForPlatform(),
          RuntimeEnabledFeatures::
                      MoveToParagraphStartOrEndSkipsNonEditableEnabled() &&
                  IsEditablePosition(EndForPlatform().DeepEquivalent())
              ? EditingBoundaryCrossingRule::kCanSkipOverEditingBoundary
              : EditingBoundaryCrossingRule::kCannotCrossEditingBoundary);
    case TextGranularity::kDocumentBoundary: {
      const VisiblePositionInFlatTree& pos = EndForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent())) {
        DCHECK(pos.IsValid()) << pos;
        return CreateVisiblePosition(
            EndOfEditableContent(pos.DeepEquivalent()));
      }
      return EndOfDocument(pos);
    }
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingLeftInternal(
    TextGranularity granularity) {
  // The difference between modifyExtendingLeft and modifyExtendingBackward is:
  // modifyExtendingBackward always extends backward logically.
  // modifyExtendingLeft behaves the same as modifyExtendingBackward except for
  // extending character or word, it extends backward logically if the enclosing
  // block is LTR direction, but it extends forward logically if the enclosing
  // block is RTL direction.
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return PreviousPositionOf(ComputeVisibleFocus(selection_),
                                  kCanSkipOverEditingBoundary);
      }
      return NextPositionOf(ComputeVisibleFocus(selection_),
                            kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr) {
        return CreateVisiblePosition(PreviousWordPosition(
            ComputeVisibleFocus(selection_).DeepEquivalent()));
      }
      return CreateVisiblePosition(NextWordPositionForPlatform(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
    case TextGranularity::kLineBoundary:
      if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
        return ModifyExtendingBackwardInternal(granularity);
      return ModifyExtendingForwardInternal(granularity);
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      return ModifyExtendingBackwardInternal(granularity);
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingLeft(
    TextGranularity granularity) {
  const VisiblePositionInFlatTree& pos =
      ModifyExtendingLeftInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustBackwardPositionForUserSelectAll(pos);
  return AdjustForwardPositionForUserSelectAll(pos);
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingBackwardInternal(
    TextGranularity granularity) {
  // Extending a selection backward by word or character from just after a table
  // selects the table.  This "makes sense" from the user perspective, esp. when
  // deleting. It was done here instead of in VisiblePositionInFlatTree because
  // we want VPs to iterate over everything.
  switch (granularity) {
    case TextGranularity::kCharacter:
      return PreviousPositionOf(ComputeVisibleFocus(selection_),
                                kCanSkipOverEditingBoundary);
    case TextGranularity::kWord:
      return CreateVisiblePosition(PreviousWordPosition(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
    case TextGranularity::kSentence:
      return CreateVisiblePosition(PreviousSentencePosition(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
    case TextGranularity::kLine: {
      const VisiblePositionInFlatTree& pos = ComputeVisibleFocus(selection_);
      DCHECK(pos.IsValid()) << pos;
      return CreateVisiblePosition(PreviousLinePosition(
          pos.ToPositionWithAffinity(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Focus())));
    }
    case TextGranularity::kParagraph:
      return PreviousParagraphPosition(
          ComputeVisibleFocus(selection_),
          LineDirectionPointForBlockDirectionNavigation(selection_.Focus()));
    case TextGranularity::kSentenceBoundary:
      return CreateVisiblePosition(
          StartOfSentencePosition(StartForPlatform().DeepEquivalent()));
    case TextGranularity::kLineBoundary:
      return LogicalStartOfLine(StartForPlatform());
    case TextGranularity::kParagraphBoundary:
      return StartOfParagraph(StartForPlatform());
    case TextGranularity::kDocumentBoundary: {
      const VisiblePositionInFlatTree pos = StartForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent())) {
        DCHECK(pos.IsValid()) << pos;
        return CreateVisiblePosition(
            StartOfEditableContent(pos.DeepEquivalent()));
      }
      return CreateVisiblePosition(StartOfDocument(pos.DeepEquivalent()));
    }
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyExtendingBackward(
    TextGranularity granularity) {
  const VisiblePositionInFlatTree pos =
      ModifyExtendingBackwardInternal(granularity);
  if (DirectionOfEnclosingBlock() == TextDirection::kLtr)
    return AdjustBackwardPositionForUserSelectAll(pos);
  return AdjustForwardPositionForUserSelectAll(pos);
}

VisiblePositionInFlatTree SelectionModifier::ModifyMovingLeft(
    TextGranularity granularity) {
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (!selection_.IsRange()) {
        if (LineDirectionOfFocus() == TextDirection::kLtr) {
          return ModifyMovingBackward(granularity);
        }
        return ModifyMovingForward(granularity);
      }
      if (DirectionOfSelection() == TextDirection::kLtr)
        return CreateVisiblePosition(selection_.Start(), selection_.Affinity());
      return CreateVisiblePosition(selection_.End(), selection_.Affinity());
    case TextGranularity::kWord:
      if (LineDirectionOfFocus() == TextDirection::kLtr) {
        return ModifyMovingBackward(granularity);
      }
      return ModifyMovingForward(granularity);
    case TextGranularity::kSentence:
    case TextGranularity::kLine:
    case TextGranularity::kParagraph:
    case TextGranularity::kSentenceBoundary:
    case TextGranularity::kParagraphBoundary:
    case TextGranularity::kDocumentBoundary:
      // FIXME: Implement all of the above.
      return ModifyMovingBackward(granularity);
    case TextGranularity::kLineBoundary:
      return LeftBoundaryOfLine(StartForPlatform(),
                                DirectionOfEnclosingBlock());
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(granularity);
  return VisiblePositionInFlatTree();
}

VisiblePositionInFlatTree SelectionModifier::ModifyMovingBackward(
    TextGranularity granularity) {
  VisiblePositionInFlatTree pos;
  switch (granularity) {
    case TextGranularity::kCharacter:
      if (selection_.IsRange()) {
        pos = CreateVisiblePosition(selection_.Start(), selection_.Affinity());
      } else {
        pos = PreviousPositionOf(ComputeVisibleFocus(selection_),
                                 kCanSkipOverEditingBoundary);
      }
      break;
    case TextGranularity::kWord:
      pos = CreateVisiblePosition(PreviousWordPosition(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
      break;
    case TextGranularity::kSentence:
      pos = CreateVisiblePosition(PreviousSentencePosition(
          ComputeVisibleFocus(selection_).DeepEquivalent()));
      break;
    case TextGranularity::kLine: {
      const VisiblePositionInFlatTree& start = StartForPlatform();
      DCHECK(start.IsValid()) << start;
      pos = CreateVisiblePosition(PreviousLinePosition(
          start.ToPositionWithAffinity(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start())));
      break;
    }
    case TextGranularity::kParagraph:
      pos = PreviousParagraphPosition(
          StartForPlatform(),
          LineDirectionPointForBlockDirectionNavigation(selection_.Start()));
      break;
    case TextGranularity::kSentenceBoundary:
      pos = CreateVisiblePosition(
          StartOfSentencePosition(StartForPlatform().DeepEquivalent()));
      break;
    case TextGranularity::kLineBoundary:
      pos = LogicalStartOfLine(StartForPlatform());
      break;
    case TextGranularity::kParagraphBoundary:
      pos = StartOfParagraph(
          StartForPlatform(),
          RuntimeEnabledFeatures::
                      MoveToParagraphStartOrEndSkipsNonEditableEnabled() &&
                  IsEditablePosition(StartForPlatform().DeepEquivalent())
              ? EditingBoundaryCrossingRule::kCanSkipOverEditingBoundary
              : EditingBoundaryCrossingRule::kCannotCrossEditingBoundary);
      break;
    case TextGranularity::kDocumentBoundary:
      pos = StartForPlatform();
      if (IsEditablePosition(pos.DeepEquivalent())) {
        DCHECK(pos.IsValid()) << pos;
        pos =
            CreateVisiblePosition(StartOfEditableContent(pos.DeepEquivalent()));
      } else {
        pos = CreateVisiblePosition(StartOfDocument(pos.DeepEquivalent()));
      }
      break;
  }
  return pos;
}

static bool IsBoundary(TextGranularity granularity) {
  return granularity == TextGranularity::kLineBoundary ||
         granularity == TextGranularity::kParagraphBoundary ||
         granularity == TextGranularity::kDocumentBoundary;
}

VisiblePositionInFlatTree SelectionModifier::ComputeModifyPosition(
    SelectionModifyAlteration alter,
    SelectionModifyDirection direction,
    TextGranularity granularity) {
  switch (direction) {
    case SelectionModifyDirection::kRight:
      if (alter == SelectionModifyAlteration::kMove)
        return ModifyMovingRight(granularity);
      return ModifyExtendingRight(granularity);
    case SelectionModifyDirection::kForward:
      if (alter == SelectionModifyAlteration::kExtend)
        return ModifyExtendingForward(granularity);
      return ModifyMovingForward(granularity);
    case SelectionModifyDirection::kLeft:
      if (alter == SelectionModifyAlteration::kMove)
        return ModifyMovingLeft(granularity);
      return ModifyExtendingLeft(granularity);
    case SelectionModifyDirection::kBackward:
      if (alter == SelectionModifyAlteration::kExtend)
        return ModifyExtendingBackward(granularity);
      return ModifyMovingBackward(granularity);
  }
  NOTREACHED_IN_MIGRATION() << static_cast<int>(direction);
  return VisiblePositionInFlatTree();
}

bool SelectionModifier::Modify(SelectionModifyAlteration alter,
                               SelectionModifyDirection direction,
                               TextGranularity granularity) {
  DCHECK(!GetFrame().GetDocument()->NeedsLayoutTreeUpdate());
  if (granularity == TextGranularity::kLine ||
      granularity == TextGranularity::kParagraph)
    UpdateLifecycleToPrePaintClean();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame().GetDocument()->Lifecycle());

  selection_ = PrepareToModifySelection(alter, direction);
  if (selection_.IsNone())
    return false;

  bool was_range = selection_.IsRange();
  VisiblePositionInFlatTree original_start_position = selection_.VisibleStart();
  VisiblePositionInFlatTree position =
      ComputeModifyPosition(alter, direction, granularity);
  if (position.IsNull())
    return false;

  if (IsSpatialNavigationEnabled(&GetFrame())) {
    if (!was_range && alter == SelectionModifyAlteration::kMove &&
        position.DeepEquivalent() == original_start_position.DeepEquivalent())
      return false;
  }

  // Some of the above operations set an xPosForVerticalArrowNavigation.
  // Setting a selection will clear it, so save it to possibly restore later.
  // Note: the START position type is arbitrary because it is unused, it would
  // be the requested position type if there were no
  // xPosForVerticalArrowNavigation set.
  LayoutUnit x =
      LineDirectionPointForBlockDirectionNavigation(selection_.Start());

  switch (alter) {
    case SelectionModifyAlteration::kMove:
      current_selection_ = SelectionInFlatTree::Builder()
                               .Collapse(position.ToPositionWithAffinity())
                               .Build();
      break;
    case SelectionModifyAlteration::kExtend:

      if (!selection_.IsCaret() &&
          (granularity == TextGranularity::kWord ||
           granularity == TextGranularity::kParagraph ||
           granularity == TextGranularity::kLine) &&
          !GetFrame()
               .GetEditor()
               .Behavior()
               .ShouldExtendSelectionByWordOrLineAcrossCaret()) {
        // Don't let the selection go across the anchor position directly.
        // Needed to match mac behavior when, for instance, word-selecting
        // backwards starting with the caret in the middle of a word and then
        // word-selecting forward, leaving the caret in the same place where it
        // was, instead of directly selecting to the end of the word.
        const VisibleSelectionInFlatTree& new_selection =
            CreateVisibleSelection(
                SelectionInFlatTree::Builder(selection_.AsSelection())
                    .Extend(position.DeepEquivalent())
                    .Build());
        if (selection_.IsAnchorFirst() != new_selection.IsAnchorFirst()) {
          position = selection_.VisibleAnchor();
        }
      }

      // Standard Mac behavior when extending to a boundary is grow the
      // selection rather than leaving the anchor in place and moving the
      // focus. Matches NSTextView.
      if (!GetFrame()
               .GetEditor()
               .Behavior()
               .ShouldAlwaysGrowSelectionWhenExtendingToBoundary() ||
          selection_.IsCaret() || !IsBoundary(granularity)) {
        current_selection_ = SelectionInFlatTree::Builder()
                                 .Collapse(selection_.Anchor())
                                 .Extend(position.DeepEquivalent())
                                 .Build();
      } else {
        TextDirection text_direction = DirectionOfEnclosingBlock();
        if (direction == SelectionModifyDirection::kForward ||
            (text_direction == TextDirection::kLtr &&
             direction == SelectionModifyDirection::kRight) ||
            (text_direction == TextDirection::kRtl &&
             direction == SelectionModifyDirection::kLeft)) {
          current_selection_ =
              SelectionInFlatTree::Builder()
                  .Collapse(selection_.IsAnchorFirst()
                                ? selection_.Anchor()
                                : position.DeepEquivalent())
                  .Extend(selection_.IsAnchorFirst() ? position.DeepEquivalent()
                                                     : selection_.Focus())
                  .Build();
        } else {
          current_selection_ = SelectionInFlatTree::Builder()
                                   .Collapse(selection_.IsAnchorFirst()
                                                 ? position.DeepEquivalent()
                                                 : selection_.Anchor())
                                   .Extend(selection_.IsAnchorFirst()
                                               ? selection_.Focus()
                                               : position.DeepEquivalent())
                                   .Build();
        }
      }
      break;
  }

  if (granularity == TextGranularity::kLine ||
      granularity == TextGranularity::kParagraph)
    x_pos_for_vertical_arrow_navigation_ = x;

  return true;
}

// TODO(yosin): Maybe baseline would be better?
static bool AbsoluteCaretY(const PositionInFlatTreeWithAffinity& c, int& y) {
  gfx::Rect rect = AbsoluteCaretBoundsOf(c);
  if (rect.IsEmpty())
    return false;
  y = rect.y() + rect.height() / 2;
  return true;
}

bool SelectionModifier::ModifyWithPageGranularity(
    SelectionModifyAlteration alter,
    unsigned vertical_distance,
    SelectionModifyVerticalDirection direction) {
  if (!vertical_distance)
    return false;

  DCHECK(!GetFrame().GetDocument()->NeedsLayoutTreeUpdate());
  UpdateLifecycleToPrePaintClean();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      GetFrame().GetDocument()->Lifecycle());

  selection_ = PrepareToModifySelection(
      alter, direction == SelectionModifyVerticalDirection::kUp
                 ? SelectionModifyDirection::kBackward
                 : SelectionModifyDirection::kForward);

  VisiblePositionInFlatTree pos;
  LayoutUnit x_pos;
  switch (alter) {
    case SelectionModifyAlteration::kMove:
      pos = CreateVisiblePosition(
          direction == SelectionModifyVerticalDirection::kUp
              ? selection_.Start()
              : selection_.End(),
          selection_.Affinity());
      x_pos = LineDirectionPointForBlockDirectionNavigation(
          direction == SelectionModifyVerticalDirection::kUp
              ? selection_.Start()
              : selection_.End());
      break;
    case SelectionModifyAlteration::kExtend:
      pos = ComputeVisibleFocus(selection_);
      x_pos = LineDirectionPointForBlockDirectionNavigation(selection_.Focus());
      break;
  }

  int start_y;
  DCHECK(pos.IsValid()) << pos;
  if (!AbsoluteCaretY(pos.ToPositionWithAffinity(), start_y))
    return false;
  if (direction == SelectionModifyVerticalDirection::kUp)
    start_y = -start_y;
  int last_y = start_y;

  VisiblePositionInFlatTree result;
  VisiblePositionInFlatTree next;
  unsigned iteration_count = 0;
  for (VisiblePositionInFlatTree p = pos;
       iteration_count < kMaxIterationForPageGranularityMovement; p = next) {
    ++iteration_count;

    if (direction == SelectionModifyVerticalDirection::kUp) {
      next = CreateVisiblePosition(
          PreviousLinePosition(p.ToPositionWithAffinity(), x_pos));
    } else {
      next = CreateVisiblePosition(
          NextLinePosition(p.ToPositionWithAffinity(), x_pos));
    }

    if (next.IsNull() || next.DeepEquivalent() == p.DeepEquivalent())
      break;
    int next_y;
    DCHECK(next.IsValid()) << next;
    if (!AbsoluteCaretY(next.ToPositionWithAffinity(), next_y))
      break;
    if (direction == SelectionModifyVerticalDirection::kUp)
      next_y = -next_y;
    if (next_y - start_y > static_cast<int>(vertical_distance))
      break;
    if (next_y >= last_y) {
      last_y = next_y;
      result = next;
    }
  }

  if (result.IsNull())
    return false;

  switch (alter) {
    case SelectionModifyAlteration::kMove:
      current_selection_ =
          SelectionInFlatTree::Builder()
              .Collapse(result.ToPositionWithAffinity())
              .SetAffinity(direction == SelectionModifyVerticalDirection::kUp
                               ? TextAffinity::kUpstream
                               : TextAffinity::kDownstream)
              .Build();
      break;
    case SelectionModifyAlteration::kExtend: {
      current_selection_ = SelectionInFlatTree::Builder()
                               .Collapse(selection_.Anchor())
                               .Extend(result.DeepEquivalent())
                               .Build();
      break;
    }
  }

  return true;
}

// Abs x/y position of the caret ignoring transforms.
// TODO(yosin) navigation with transforms should be smarter.
static LayoutUnit LineDirectionPointForBlockDirectionNavigationOf(
    const VisiblePositionInFlatTree& visible_position) {
  if (visible_position.IsNull())
    return LayoutUnit();

  const LocalCaretRect& caret_rect =
      LocalCaretRectOfPosition(visible_position.ToPositionWithAffinity());
  if (caret_rect.IsEmpty())
    return LayoutUnit();

  // This ignores transforms on purpose, for now. Vertical navigation is done
  // without consulting transforms, so that 'up' in transformed text is 'up'
  // relative to the text, not absolute 'up'.
  PhysicalOffset caret_point;
  if (caret_rect.layout_object->HasFlippedBlocksWritingMode()) [[unlikely]] {
    caret_point = caret_rect.rect.MaxXMinYCorner();
  } else {
    caret_point = caret_rect.rect.MinXMinYCorner();
  }
  caret_point = caret_rect.layout_object->LocalToAbsolutePoint(
      caret_point, kIgnoreTransforms);
  return caret_rect.layout_object->IsHorizontalWritingMode() ? caret_point.left
                                                             : caret_point.top;
}

LayoutUnit SelectionModifier::LineDirectionPointForBlockDirectionNavigation(
    const PositionInFlatTree& pos) {
  LayoutUnit x;

  if (selection_.IsNone())
    return x;

  if (x_pos_for_vertical_arrow_navigation_ ==
      NoXPosForVerticalArrowNavigation()) {
    VisiblePositionInFlatTree visible_position =
        CreateVisiblePosition(pos, selection_.Affinity());
    // VisiblePositionInFlatTree creation can fail here if a node containing the
    // selection becomes visibility:hidden after the selection is created and
    // before this function is called.
    x = LineDirectionPointForBlockDirectionNavigationOf(visible_position);
    x_pos_for_vertical_arrow_navigation_ = x;
  } else {
    x = x_pos_for_vertical_arrow_navigation_;
  }

  return x;
}

void SelectionModifier::UpdateLifecycleToPrePaintClean() {
  LocalFrameView* const frame_view = frame_->View();
  if (!frame_view)
    return;
  frame_view->UpdateLifecycleToPrePaintClean(DocumentUpdateReason::kSelection);
}

}  // namespace blink

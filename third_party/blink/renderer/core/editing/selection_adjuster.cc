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

#include "third_party/blink/renderer/core/editing/selection_adjuster.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {

template <typename Strategy>
SelectionTemplate<Strategy> ComputeAdjustedSelection(
    const SelectionTemplate<Strategy> selection,
    const EphemeralRangeTemplate<Strategy>& range) {
  if (range.StartPosition().CompareTo(range.EndPosition()) == 0) {
    return typename SelectionTemplate<Strategy>::Builder()
        .Collapse(selection.IsAnchorFirst() ? range.StartPosition()
                                            : range.EndPosition())
        .Build();
  }
  if (selection.IsAnchorFirst()) {
    return typename SelectionTemplate<Strategy>::Builder()
        .SetAsForwardSelection(range)
        .Build();
  }
  return typename SelectionTemplate<Strategy>::Builder()
      .SetAsBackwardSelection(range)
      .Build();
}

bool IsEmptyTableCell(const Node* node) {
  // Returns true IFF the passed in node is one of:
  //   .) a table cell with no children,
  //   .) a table cell with a single BR child, and which has no other child
  //      layoutObject, including :before and :after layoutObject
  //   .) the BR child of such a table cell

  // Find rendered node
  while (node && !node->GetLayoutObject())
    node = node->parentNode();
  if (!node)
    return false;

  // Make sure the rendered node is a table cell or <br>.
  // If it's a <br>, then the parent node has to be a table cell.
  const LayoutObject* layout_object = node->GetLayoutObject();
  if (layout_object->IsBR()) {
    layout_object = layout_object->Parent();
    if (!layout_object)
      return false;
  }
  if (!layout_object->IsTableCell())
    return false;

  // Check that the table cell contains no child layoutObjects except for
  // perhaps a single <br>.
  const LayoutObject* const child_layout_object =
      layout_object->SlowFirstChild();
  if (!child_layout_object)
    return true;
  if (!child_layout_object->IsBR())
    return false;
  return !child_layout_object->NextSibling();
}

}  // anonymous namespace

class GranularityAdjuster final {
  STATIC_ONLY(GranularityAdjuster);

 public:
  template <typename Strategy>
  static PositionTemplate<Strategy> ComputeStartRespectingGranularityAlgorithm(
      const PositionWithAffinityTemplate<Strategy>& passed_start,
      TextGranularity granularity,
      WordInclusion inclusion = WordInclusion::kDefault) {
    DCHECK(passed_start.IsNotNull());

    switch (granularity) {
      case TextGranularity::kCharacter:
        // Don't do any expansion.
        return passed_start.GetPosition();
      case TextGranularity::kWord: {
        // General case: Select the word the caret is positioned inside of.
        // If the caret is on the word boundary, select the word according to
        // |wordSide|.
        // Edge case: If the caret is after the last word in a soft-wrapped line
        // or the last word in the document, select that last word
        // (kPreviousWordIfOnBoundary).
        // Edge case: If the caret is after the last word in a paragraph, select
        // from the the end of the last word to the line break (also
        // kNextWordIfOnBoundary);
        const VisiblePositionTemplate<Strategy> visible_start =
            CreateVisiblePosition(passed_start);
        const PositionTemplate<Strategy> word_start = StartOfWordPosition(
            passed_start.GetPosition(), ChooseWordSide(visible_start));
        if (inclusion == WordInclusion::kMiddle) {
          // Check if the middle of the word is within the passed selection.
          const PositionTemplate<Strategy> word_end = EndOfWordPosition(
              passed_start.GetPosition(), ChooseWordSide(visible_start));
          const PositionTemplate<Strategy> word_middle =
              MiddleOfWordPosition(word_start, word_end);
          if (passed_start.GetPosition() > word_middle) {
            return word_end;
          }
        }
        return CreateVisiblePosition(word_start).DeepEquivalent();
      }
      case TextGranularity::kSentence:
        return StartOfSentencePosition(passed_start.GetPosition());
      case TextGranularity::kLine:
        return StartOfLine(CreateVisiblePosition(passed_start))
            .DeepEquivalent();
      case TextGranularity::kLineBoundary:
        return StartOfLine(CreateVisiblePosition(passed_start))
            .DeepEquivalent();
      case TextGranularity::kParagraph: {
        const VisiblePositionTemplate<Strategy> pos =
            CreateVisiblePosition(passed_start);
        if (IsStartOfLine(pos) && IsEndOfEditableOrNonEditableContent(pos))
          return StartOfParagraph(PreviousPositionOf(pos)).DeepEquivalent();
        return StartOfParagraph(pos).DeepEquivalent();
      }
      case TextGranularity::kDocumentBoundary:
        return CreateVisiblePosition(
                   StartOfDocument(passed_start.GetPosition()))
            .DeepEquivalent();
      case TextGranularity::kParagraphBoundary:
        return StartOfParagraph(CreateVisiblePosition(passed_start))
            .DeepEquivalent();
      case TextGranularity::kSentenceBoundary:
        return StartOfSentencePosition(passed_start.GetPosition());
    }

    NOTREACHED_IN_MIGRATION();
    return passed_start.GetPosition();
  }

  template <typename Strategy>
  static PositionTemplate<Strategy> ComputeEndRespectingGranularityAlgorithm(
      const PositionTemplate<Strategy>& start,
      const PositionWithAffinityTemplate<Strategy>& passed_end,
      TextGranularity granularity,
      WordInclusion inclusion = WordInclusion::kDefault) {
    DCHECK(passed_end.IsNotNull());

    switch (granularity) {
      case TextGranularity::kCharacter:
        // Don't do any expansion.
        return passed_end.GetPosition();
      case TextGranularity::kWord: {
        // General case: Select the word the caret is positioned inside of.
        // If the caret is on the word boundary, select the word according to
        // |wordSide|.
        // Edge case: If the caret is after the last word in a soft-wrapped line
        // or the last word in the document, select that last word
        // (|kPreviousWordIfOnBoundary|).
        // Edge case: If the caret is after the last word in a paragraph, select
        // from the the end of the last word to the line break (also
        // |kNextWordIfOnBoundary|);
        const VisiblePositionTemplate<Strategy> original_end =
            CreateVisiblePosition(passed_end);
        bool is_end_of_paragraph = IsEndOfParagraph(original_end);
        // Get last word of paragraph. If original_end is already known to be
        // the last word, use that. If not the last word, find it with
        // EndOfWordPosition
        const VisiblePositionTemplate<Strategy> word_end =
            is_end_of_paragraph
                ? original_end
                : CreateVisiblePosition(EndOfWordPosition(
                      passed_end.GetPosition(), ChooseWordSide(original_end)));
        if (inclusion == WordInclusion::kMiddle) {
          const PositionTemplate<Strategy> word_start = StartOfWordPosition(
              passed_end.GetPosition(), ChooseWordSide(original_end));
          const PositionTemplate<Strategy> word_middle =
              MiddleOfWordPosition(word_start, word_end.DeepEquivalent());
          if (word_middle.IsNull() or word_middle > passed_end.GetPosition()) {
            return word_start;
          }
        }
        if (!is_end_of_paragraph)
          return word_end.DeepEquivalent();
        if (IsEmptyTableCell(start.AnchorNode()))
          return word_end.DeepEquivalent();

        // If the end was in a table cell, we don't want the \t from between
        // cells or \n after the row, so return last word
        if (EnclosingTableCell(original_end.DeepEquivalent()))
          return word_end.DeepEquivalent();

        // Select the paragraph break (the space from the end of a paragraph
        // to the start of the next one) to match TextEdit.
        const VisiblePositionTemplate<Strategy> end = NextPositionOf(word_end);
        Element* const table = TableElementJustBefore(end);
        if (!table) {
          if (end.IsNull())
            return word_end.DeepEquivalent();
          return end.DeepEquivalent();
        }

        if (!IsEnclosingBlock(table))
          return word_end.DeepEquivalent();

        // The paragraph break after the last paragraph in the last cell
        // of a block table ends at the start of the paragraph after the
        // table.
        const VisiblePositionTemplate<Strategy> next =
            NextPositionOf(end, kCannotCrossEditingBoundary);
        if (next.IsNull())
          return word_end.DeepEquivalent();
        return next.DeepEquivalent();
      }
      case TextGranularity::kSentence:
        return EndOfSentence(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
      case TextGranularity::kLine: {
        const VisiblePositionTemplate<Strategy> end =
            CreateVisiblePosition(EndOfLine(passed_end));
        if (!IsEndOfParagraph(end))
          return end.DeepEquivalent();
        // If the end of this line is at the end of a paragraph, include the
        // space after the end of the line in the selection.
        const VisiblePositionTemplate<Strategy> next = NextPositionOf(end);
        if (next.IsNull())
          return end.DeepEquivalent();
        return next.DeepEquivalent();
      }
      case TextGranularity::kLineBoundary:
        return EndOfLine(passed_end).GetPosition();
      case TextGranularity::kParagraph: {
        const VisiblePositionTemplate<Strategy> visible_paragraph_end =
            EndOfParagraph(CreateVisiblePosition(passed_end));

        // Include the "paragraph break" (the space from the end of this
        // paragraph to the start of the next one) in the selection.
        const VisiblePositionTemplate<Strategy> end =
            NextPositionOf(visible_paragraph_end);

        Element* const table = TableElementJustBefore(end);
        if (!table) {
          if (end.IsNull())
            return visible_paragraph_end.DeepEquivalent();
          return end.DeepEquivalent();
        }

        if (!IsEnclosingBlock(table)) {
          // There is no paragraph break after the last paragraph in the
          // last cell of an inline table.
          return visible_paragraph_end.DeepEquivalent();
        }

        // The paragraph break after the last paragraph in the last cell of
        // a block table ends at the start of the paragraph after the table,
        // not at the position just after the table.
        const VisiblePositionTemplate<Strategy> next =
            NextPositionOf(end, kCannotCrossEditingBoundary);
        if (next.IsNull())
          return visible_paragraph_end.DeepEquivalent();
        return next.DeepEquivalent();
      }
      case TextGranularity::kDocumentBoundary:
        return EndOfDocument(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
      case TextGranularity::kParagraphBoundary:
        return EndOfParagraph(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
      case TextGranularity::kSentenceBoundary:
        return EndOfSentence(CreateVisiblePosition(passed_end))
            .DeepEquivalent();
    }
    NOTREACHED_IN_MIGRATION();
    return passed_end.GetPosition();
  }

  template <typename Strategy>
  static SelectionTemplate<Strategy> AdjustSelection(
      const SelectionTemplate<Strategy>& canonicalized_selection,
      TextGranularity granularity,
      const WordInclusion inclusion) {
    const TextAffinity affinity = canonicalized_selection.Affinity();

    const PositionTemplate<Strategy> start =
        canonicalized_selection.ComputeStartPosition();
    const PositionTemplate<Strategy> new_start =
        ComputeStartRespectingGranularityAlgorithm(
            PositionWithAffinityTemplate<Strategy>(start, affinity),
            granularity, inclusion);
    const PositionTemplate<Strategy> expanded_start =
        new_start.IsNotNull() ? new_start : start;

    const PositionTemplate<Strategy> end =
        canonicalized_selection.ComputeEndPosition();
    const PositionTemplate<Strategy> new_end =
        ComputeEndRespectingGranularityAlgorithm(
            expanded_start,
            PositionWithAffinityTemplate<Strategy>(end, affinity), granularity,
            inclusion);
    const PositionTemplate<Strategy> expanded_end =
        new_end.IsNotNull() ? new_end : end;

    const EphemeralRangeTemplate<Strategy> expanded_range =
        AdjustStartAndEnd(expanded_start, expanded_end);

    return ComputeAdjustedSelection(canonicalized_selection, expanded_range);
  }

 private:
  template <typename Strategy>
  static WordSide ChooseWordSide(
      const VisiblePositionTemplate<Strategy>& position) {
    return IsEndOfEditableOrNonEditableContent(position) ||
                   (IsEndOfLine(position) && !IsStartOfLine(position) &&
                    !IsEndOfParagraph(position))
               ? kPreviousWordIfOnBoundary
               : kNextWordIfOnBoundary;
  }

  // Because of expansion is done in flat tree, in case of |start| and |end| are
  // distributed, |start| can be after |end|.
  static EphemeralRange AdjustStartAndEnd(const Position& start,
                                          const Position& end) {
    if (start <= end)
      return EphemeralRange(start, end);
    return EphemeralRange(end, start);
  }

  static EphemeralRangeInFlatTree AdjustStartAndEnd(
      const PositionInFlatTree& start,
      const PositionInFlatTree& end) {
    return EphemeralRangeInFlatTree(start, end);
  }
};

PositionInFlatTree ComputeStartRespectingGranularity(
    const PositionInFlatTreeWithAffinity& start,
    TextGranularity granularity) {
  return GranularityAdjuster::ComputeStartRespectingGranularityAlgorithm(
      start, granularity);
}

PositionInFlatTree ComputeEndRespectingGranularity(
    const PositionInFlatTree& start,
    const PositionInFlatTreeWithAffinity& end,
    TextGranularity granularity) {
  return GranularityAdjuster::ComputeEndRespectingGranularityAlgorithm(
      start, end, granularity);
}

SelectionInDOMTree SelectionAdjuster::AdjustSelectionRespectingGranularity(
    const SelectionInDOMTree& selection,
    TextGranularity granularity,
    const WordInclusion inclusion = WordInclusion::kDefault) {
  return GranularityAdjuster::AdjustSelection(selection, granularity,
                                              inclusion);
}

SelectionInFlatTree SelectionAdjuster::AdjustSelectionRespectingGranularity(
    const SelectionInFlatTree& selection,
    TextGranularity granularity,
    const WordInclusion inclusion = WordInclusion::kDefault) {
  return GranularityAdjuster::AdjustSelection(selection, granularity,
                                              inclusion);
}

class ShadowBoundaryAdjuster final {
  STATIC_ONLY(ShadowBoundaryAdjuster);

 public:
  template <typename Strategy>
  static SelectionTemplate<Strategy> AdjustSelection(
      const SelectionTemplate<Strategy>& selection) {
    if (!selection.IsRange())
      return selection;

    const EphemeralRangeTemplate<Strategy> expanded_range =
        selection.ComputeRange();

    if (selection.IsAnchorFirst()) {
      PositionTemplate<Strategy> adjusted_end =
          AdjustSelectionEndToAvoidCrossingShadowBoundaries(expanded_range);
      if (adjusted_end.IsNull())
        adjusted_end = expanded_range.StartPosition();
      const EphemeralRangeTemplate<Strategy> shadow_adjusted_range(
          expanded_range.StartPosition(), adjusted_end);
      return ComputeAdjustedSelection(selection, shadow_adjusted_range);
    }
    PositionTemplate<Strategy> adjusted_start =
        AdjustSelectionStartToAvoidCrossingShadowBoundaries(expanded_range);
    if (adjusted_start.IsNull())
      adjusted_start = expanded_range.EndPosition();
    const EphemeralRangeTemplate<Strategy> shadow_adjusted_range(
        adjusted_start, expanded_range.EndPosition());
    return ComputeAdjustedSelection(selection, shadow_adjusted_range);
  }

 private:
  static Node* EnclosingShadowHost(Node* node) {
    for (Node* runner = node; runner;
         runner = FlatTreeTraversal::Parent(*runner)) {
      if (IsShadowHost(runner))
        return runner;
    }
    return nullptr;
  }

  static bool IsEnclosedBy(const PositionInFlatTree& position,
                           const Node& node) {
    DCHECK(position.IsNotNull());
    Node* anchor_node = position.AnchorNode();
    if (anchor_node == node)
      return !position.IsAfterAnchor() && !position.IsBeforeAnchor();

    return FlatTreeTraversal::IsDescendantOf(*anchor_node, node);
  }

  static Node* EnclosingShadowHostForStart(const PositionInFlatTree& position) {
    Node* node = position.NodeAsRangeFirstNode();
    if (!node)
      return nullptr;
    Node* shadow_host = EnclosingShadowHost(node);
    if (!shadow_host)
      return nullptr;
    if (!IsEnclosedBy(position, *shadow_host))
      return nullptr;
    return IsUserSelectContain(*shadow_host) ? shadow_host : nullptr;
  }

  static Node* EnclosingShadowHostForEnd(const PositionInFlatTree& position) {
    Node* node = position.NodeAsRangeLastNode();
    if (!node)
      return nullptr;
    Node* shadow_host = EnclosingShadowHost(node);
    if (!shadow_host)
      return nullptr;
    if (!IsEnclosedBy(position, *shadow_host))
      return nullptr;
    return IsUserSelectContain(*shadow_host) ? shadow_host : nullptr;
  }

  static PositionInFlatTree AdjustPositionInFlatTreeForStart(
      const PositionInFlatTree& position,
      Node* shadow_host) {
    if (IsEnclosedBy(position, *shadow_host)) {
      if (position.IsBeforeChildren())
        return PositionInFlatTree::BeforeNode(*shadow_host);
      return PositionInFlatTree::AfterNode(*shadow_host);
    }

    // We use |firstChild|'s after instead of beforeAllChildren for backward
    // compatibility. The positions are same but the anchors would be different,
    // and selection painting uses anchor nodes.
    if (Node* first_child = FlatTreeTraversal::FirstChild(*shadow_host))
      return PositionInFlatTree::BeforeNode(*first_child);
    return PositionInFlatTree();
  }

  static Position AdjustPositionForEnd(const Position& current_position,
                                       Node* start_container_node) {
    TreeScope& tree_scope = start_container_node->GetTreeScope();

    DCHECK(current_position.ComputeContainerNode()->GetTreeScope() !=
           tree_scope);

    if (Node* ancestor = tree_scope.AncestorInThisScope(
            current_position.ComputeContainerNode())) {
      if (ancestor->contains(start_container_node))
        return Position::AfterNode(*ancestor);
      return Position::BeforeNode(*ancestor);
    }

    if (Node* last_child = tree_scope.RootNode().lastChild())
      return Position::AfterNode(*last_child);

    return Position();
  }

  static PositionInFlatTree AdjustPositionInFlatTreeForEnd(
      const PositionInFlatTree& position,
      Node* shadow_host) {
    if (IsEnclosedBy(position, *shadow_host)) {
      if (position.IsAfterChildren())
        return PositionInFlatTree::AfterNode(*shadow_host);
      return PositionInFlatTree::BeforeNode(*shadow_host);
    }

    // We use |lastChild|'s after instead of afterAllChildren for backward
    // compatibility. The positions are same but the anchors would be different,
    // and selection painting uses anchor nodes.
    if (Node* last_child = FlatTreeTraversal::LastChild(*shadow_host))
      return PositionInFlatTree::AfterNode(*last_child);
    return PositionInFlatTree();
  }

  static Position AdjustPositionForStart(const Position& current_position,
                                         Node* end_container_node) {
    TreeScope& tree_scope = end_container_node->GetTreeScope();

    DCHECK(current_position.ComputeContainerNode()->GetTreeScope() !=
           tree_scope);

    if (Node* ancestor = tree_scope.AncestorInThisScope(
            current_position.ComputeContainerNode())) {
      if (ancestor->contains(end_container_node))
        return Position::BeforeNode(*ancestor);
      return Position::AfterNode(*ancestor);
    }

    if (Node* first_child = tree_scope.RootNode().firstChild())
      return Position::BeforeNode(*first_child);

    return Position();
  }

  // TODO(hajimehoshi): Checking treeScope is wrong when a node is
  // distributed, but we leave it as it is for backward compatibility.
  static bool IsCrossingShadowBoundaries(const EphemeralRange& range) {
    DCHECK(range.IsNotNull());
    return range.StartPosition().AnchorNode()->GetTreeScope() !=
           range.EndPosition().AnchorNode()->GetTreeScope();
  }

  static Position AdjustSelectionStartToAvoidCrossingShadowBoundaries(
      const EphemeralRange& range) {
    DCHECK(range.IsNotNull());
    if (!IsCrossingShadowBoundaries(range))
      return range.StartPosition();
    return AdjustPositionForStart(range.StartPosition(),
                                  range.EndPosition().ComputeContainerNode());
  }

  static Position AdjustSelectionEndToAvoidCrossingShadowBoundaries(
      const EphemeralRange& range) {
    DCHECK(range.IsNotNull());
    if (!IsCrossingShadowBoundaries(range))
      return range.EndPosition();
    return AdjustPositionForEnd(range.EndPosition(),
                                range.StartPosition().ComputeContainerNode());
  }

  static PositionInFlatTree AdjustSelectionStartToAvoidCrossingShadowBoundaries(
      const EphemeralRangeInFlatTree& range) {
    Node* const shadow_host_start =
        EnclosingShadowHostForStart(range.StartPosition());
    Node* const shadow_host_end =
        EnclosingShadowHostForEnd(range.EndPosition());
    if (shadow_host_start == shadow_host_end)
      return range.StartPosition();
    Node* const shadow_host =
        shadow_host_end ? shadow_host_end : shadow_host_start;
    return AdjustPositionInFlatTreeForStart(range.StartPosition(), shadow_host);
  }

  static PositionInFlatTree AdjustSelectionEndToAvoidCrossingShadowBoundaries(
      const EphemeralRangeInFlatTree& range) {
    Node* const shadow_host_start =
        EnclosingShadowHostForStart(range.StartPosition());
    Node* const shadow_host_end =
        EnclosingShadowHostForEnd(range.EndPosition());
    if (shadow_host_start == shadow_host_end)
      return range.EndPosition();
    Node* const shadow_host =
        shadow_host_start ? shadow_host_start : shadow_host_end;
    return AdjustPositionInFlatTreeForEnd(range.EndPosition(), shadow_host);
  }
};

SelectionInDOMTree
SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(
    const SelectionInDOMTree& selection) {
  return ShadowBoundaryAdjuster::AdjustSelection(selection);
}
SelectionInFlatTree
SelectionAdjuster::AdjustSelectionToAvoidCrossingShadowBoundaries(
    const SelectionInFlatTree& selection) {
  return ShadowBoundaryAdjuster::AdjustSelection(selection);
}

class EditingBoundaryAdjuster final {
  STATIC_ONLY(EditingBoundaryAdjuster);

 public:
  template <typename Strategy>
  static SelectionTemplate<Strategy> AdjustSelection(
      const SelectionTemplate<Strategy>& selection) {
    const auto adjusted = AdjustFocus(selection);
    // TODO(editing-dev): This DCHECK now fails on crossing <body> selection.
    // Test ApplyBlockElementCommandTest.selectionCrossingOverBody has anchor
    // outside of <body> and focus inside of <body>, after adjustment, new
    // focus is still inside of <body>, so RBE is not the same.
    // DCHECK_EQ(
    //     &RootBoundaryElementOf<Strategy>(
    //         *selection.Anchor().ComputeContainerNode()),
    //     &RootBoundaryElementOf<Strategy>(*adjusted.ComputeContainerNode()))
    //     << std::endl
    //     << selection << std::endl
    //     << adjusted;
    return typename SelectionTemplate<Strategy>::Builder(selection)
        .Extend(adjusted)
        .Build();
  }

 private:
  template <typename Strategy>
  static bool IsEditingBoundary(const Node& node,
                                const Node& previous_node,
                                bool is_previous_node_editable) {
    return IsEditable(node) != is_previous_node_editable;
  }

  // Returns the highest ancestor of |start| along the parent chain, so that
  // all node in between them including the ancestor have the same
  // IsEditable() bit with |start|. Note that it only consider the <body>
  // subtree.
  template <typename Strategy>
  static const Node& RootBoundaryElementOf(const Node& start) {
    if (IsA<HTMLBodyElement>(start))
      return start;

    const bool is_editable = IsEditable(start);
    const Node* result = &start;
    for (const Node& ancestor : Strategy::AncestorsOf(start)) {
      if (IsEditingBoundary<Strategy>(ancestor, *result, is_editable))
        break;
      result = &ancestor;
      if (IsA<HTMLBodyElement>(*result))
        break;
    }

    return *result;
  }

  // TODO(editing-dev): The input |selection| for this function might cross
  // shadow boundary in DOM tree in flat tree selection case. We still want to
  // adjust the selection on DOM tree since currently editibility is defined on
  // DOM tree according to spec, so we need to deal with shadow boundary in this
  // function for flat tree selection. We ended with no good way but just
  // templated the DOM tree algorithm including |RootBoundaryElementOf()| for
  // flat tree.
  template <typename Strategy>
  static PositionTemplate<Strategy> AdjustFocus(
      const SelectionTemplate<Strategy>& selection) {
    DCHECK(!selection.IsNone()) << selection;

    const Node* const anchor_node = selection.Anchor().ComputeContainerNode();
    const Node* const focus_node = selection.Focus().ComputeContainerNode();

    // In the same node, no need to adjust.
    if (anchor_node == focus_node) {
      return selection.Focus();
    }

    const Node& anchor_rbe = RootBoundaryElementOf<Strategy>(*anchor_node);
    const Node& focus_rbe = RootBoundaryElementOf<Strategy>(*focus_node);

    // In the same RBE, no need to adjust.
    if (anchor_rbe == focus_rbe) {
      return selection.Focus();
    }

    // |focus_rbe| is not in |anchor_rbe| subtree, in this case, the result
    // should be the first/last position in the |anchor_rbe| subtree.
    if (!Strategy::IsDescendantOf(focus_rbe, anchor_rbe)) {
      if (selection.IsAnchorFirst()) {
        return PositionTemplate<Strategy>::LastPositionInNode(anchor_rbe);
      }
      return PositionTemplate<Strategy>::FirstPositionInNode(anchor_rbe);
    }

    // |focus_rbe| is in |anchor_rbe| subtree. We want to find the last boundary
    // the selection crossed from focus. Which is the highest ancestor node of
    // focus in |anchor_rbe| subtree that RBE(ancestor) != |anchor_rbe|.
    const Node* boundary = &focus_rbe;
    const Node* previous_ancestor = &focus_rbe;
    bool previous_editable = IsEditable(focus_rbe);
    for (const Node& ancestor : Strategy::AncestorsOf(focus_rbe)) {
      if (IsEditingBoundary<Strategy>(ancestor, *previous_ancestor,
                                      previous_editable)) {
        boundary = previous_ancestor;
      }

      if (ancestor == anchor_rbe || IsA<HTMLBodyElement>(ancestor)) {
        break;
      }
      previous_editable = IsEditable(ancestor);
      previous_ancestor = &ancestor;
    }

    if (selection.IsAnchorFirst()) {
      return PositionTemplate<Strategy>::BeforeNode(*boundary);
    }
    return PositionTemplate<Strategy>::AfterNode(*boundary);
  }
};

template <>
inline bool
EditingBoundaryAdjuster::IsEditingBoundary<EditingInFlatTreeStrategy>(
    const Node& node,
    const Node& previous_node,
    bool is_previous_node_editable) {
  // We want to treat shadow host as not editable element if |previous_node|
  // is in the shadow tree attached to the shadow host.
  if (IsShadowHost(&node) && is_previous_node_editable &&
      previous_node.OwnerShadowHost() == &node)
    return true;
  return IsEditable(node) != is_previous_node_editable;
}

SelectionInDOMTree
SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
    const SelectionInDOMTree& selection) {
  return EditingBoundaryAdjuster::AdjustSelection(selection);
}
SelectionInFlatTree
SelectionAdjuster::AdjustSelectionToAvoidCrossingEditingBoundaries(
    const SelectionInFlatTree& selection) {
  return EditingBoundaryAdjuster::AdjustSelection(selection);
}

class SelectionTypeAdjuster final {
  STATIC_ONLY(SelectionTypeAdjuster);

 public:
  template <typename Strategy>
  static SelectionTemplate<Strategy> AdjustSelection(
      const SelectionTemplate<Strategy>& selection) {
    if (selection.IsNone())
      return selection;
    const EphemeralRangeTemplate<Strategy>& range = selection.ComputeRange();
    DCHECK(!NeedsLayoutTreeUpdate(range.StartPosition())) << range;
    if (range.IsCollapsed() ||
        // TODO(editing-dev): Consider this canonicalization is really needed.
        MostBackwardCaretPosition(range.StartPosition()) ==
            MostBackwardCaretPosition(range.EndPosition())) {
      return typename SelectionTemplate<Strategy>::Builder()
          .Collapse(PositionWithAffinityTemplate<Strategy>(
              range.StartPosition(), selection.Affinity()))
          .Build();
    }
    // "Constrain" the selection to be the smallest equivalent range of
    // nodes. This is a somewhat arbitrary choice, but experience shows that
    // it is useful to make to make the selection "canonical" (if only for
    // purposes of comparing selections). This is an ideal point of the code
    // to do this operation, since all selection changes that result in a
    // RANGE come through here before anyone uses it.
    // TODO(editing-dev): Consider this canonicalization is really needed.
    PositionTemplate<Strategy> forward_start_position =
        MostForwardCaretPosition(range.StartPosition());
    PositionTemplate<Strategy> backward_end_position =
        MostBackwardCaretPosition(range.EndPosition());
    // When the start and end of `range` have different editability, and the
    // return value of `CanonicalPositionOf` is null, `VisiblePosition` of
    // `selection` will be a caret. For example, `EndPosition().AnchorNode()` is
    // non-editable and its previous sibling node which is the
    // `StartPosition().AnchorNode()` is editable. In this case, we shouldn't
    // forward/backward the start/end position of `range`.
    // See http://crbug.com/1371268 for more details.
    if (IsEditablePosition(backward_end_position) &&
        CanonicalPositionOf(forward_start_position).IsNull()) {
      forward_start_position = range.StartPosition();
    }
    if (IsEditablePosition(forward_start_position) &&
        CanonicalPositionOf(backward_end_position).IsNull()) {
      backward_end_position = range.EndPosition();
    }
    const EphemeralRangeTemplate<Strategy> minimal_range(forward_start_position,
                                                         backward_end_position);
    if (minimal_range.IsCollapsed() || selection.IsAnchorFirst()) {
      return typename SelectionTemplate<Strategy>::Builder()
          .SetAsForwardSelection(minimal_range)
          .Build();
    }
    return typename SelectionTemplate<Strategy>::Builder()
        .SetAsBackwardSelection(minimal_range)
        .Build();
  }
};

SelectionInDOMTree SelectionAdjuster::AdjustSelectionType(
    const SelectionInDOMTree& selection) {
  return SelectionTypeAdjuster::AdjustSelection(selection);
}
SelectionInFlatTree SelectionAdjuster::AdjustSelectionType(
    const SelectionInFlatTree& selection) {
  return SelectionTypeAdjuster::AdjustSelection(selection);
}

}  // namespace blink

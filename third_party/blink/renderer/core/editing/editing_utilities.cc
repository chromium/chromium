/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/editing/editing_utilities.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer_access_policy.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"
#include "third_party/blink/renderer/core/editing/editing_strategy.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/position_iterator.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/html_interchange.h"
#include "third_party/blink/renderer/core/editing/state_machines/backspace_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/backward_grapheme_boundary_state_machine.h"
#include "third_party/blink/renderer/core/editing/state_machines/forward_grapheme_boundary_state_machine.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_dlist_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_olist_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_element_factory.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

using mojom::blink::FormControlType;

namespace {

std::ostream& operator<<(std::ostream& os, PositionMoveType type) {
  static const char* const kTexts[] = {"CodeUnit", "BackwardDeletion",
                                       "GraphemeCluster"};
  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(type);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown PositionMoveType value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown PositionMoveType value";
  return os << *it;
}

UChar WhitespaceRebalancingCharToAppend(const String& string,
                                        bool start_is_start_of_paragraph,
                                        bool should_emit_nbsp_before_end,
                                        wtf_size_t index,
                                        UChar previous) {
  DCHECK_LT(index, string.length());

  if (!IsWhitespace(string[index]))
    return string[index];

  if (!index && start_is_start_of_paragraph)
    return kNoBreakSpaceCharacter;
  if (index + 1 == string.length() && should_emit_nbsp_before_end)
    return kNoBreakSpaceCharacter;

  // Generally, alternate between space and no-break space.
  if (previous == ' ')
    return kNoBreakSpaceCharacter;
  if (previous == kNoBreakSpaceCharacter)
    return ' ';

  // Run of two or more spaces starts with a no-break space (crbug.com/453042).
  if (index + 1 < string.length() && IsWhitespace(string[index + 1]))
    return kNoBreakSpaceCharacter;

  return ' ';
}

}  // namespace

bool NeedsLayoutTreeUpdate(const Node& node) {
  const Document& document = node.GetDocument();
  if (document.NeedsLayoutTreeUpdate())
    return true;
  // TODO(yosin): We should make |document::needsLayoutTreeUpdate()| to
  // check |LayoutView::needsLayout()|.
  return document.View() && document.View()->NeedsLayout();
}

template <typename PositionType>
static bool NeedsLayoutTreeUpdateAlgorithm(const PositionType& position) {
  const Node* node = position.AnchorNode();
  if (!node)
    return false;
  return NeedsLayoutTreeUpdate(*node);
}

bool NeedsLayoutTreeUpdate(const Position& position) {
  return NeedsLayoutTreeUpdateAlgorithm<Position>(position);
}

bool NeedsLayoutTreeUpdate(const PositionInFlatTree& position) {
  return NeedsLayoutTreeUpdateAlgorithm<PositionInFlatTree>(position);
}

// Atomic means that the node has no children, or has children which are ignored
// for the purposes of editing.
bool IsAtomicNode(const Node* node) {
  return node && (!node->hasChildren() || EditingIgnoresContent(*node));
}

bool IsAtomicNodeInFlatTree(const Node* node) {
  return node && (!FlatTreeTraversal::HasChildren(*node) ||
                  EditingIgnoresContent(*node));
}

bool IsNodeFullyContained(const EphemeralRange& range, const Node& node) {
  if (range.IsNull())
    return false;

  if (!NodeTraversal::CommonAncestor(*range.StartPosition().AnchorNode(), node))
    return false;

  return range.StartPosition() <= Position::BeforeNode(node) &&
         Position::AfterNode(node) <= range.EndPosition();
}

// TODO(editing-dev): We should implement real version which refers
// "user-select" CSS property.
bool IsUserSelectContain(const Node& node) {
  return IsA<HTMLTextAreaElement>(node) || IsA<HTMLInputElement>(node) ||
         IsA<HTMLSelectElement>(node);
}

enum EditableLevel { kEditable, kRichlyEditable };
static bool HasEditableLevel(const Node& node, EditableLevel editable_level) {
  DCHECK(node.GetDocument().IsActive());
  // TODO(editing-dev): We should have this check:
  // DCHECK_GE(node.document().lifecycle().state(),
  //           DocumentLifecycle::StyleClean);
  if (node.IsPseudoElement())
    return false;

  // Ideally we'd call DCHECK(!needsStyleRecalc()) here, but
  // ContainerNode::setFocus() calls setNeedsStyleRecalc(), so the assertion
  // would fire in the middle of Document::setFocusedNode().

  for (const Node& ancestor : NodeTraversal::InclusiveAncestorsOf(node)) {
    if (!(ancestor.IsHTMLElement() || ancestor.IsDocumentNode()))
      continue;
    // An inert subtree should not contain any content or controls which are
    // critical to understanding or using aspects of the page which are not in
    // the inert state. Content in an inert subtree will not be perceivable by
    // all users, or interactive. See
    // https://html.spec.whatwg.org/multipage/interaction.html#the-inert-attribute.
    // To prevent the invisible inert element being overlooked, the
    // inert attribute of the element is initially assessed. See
    // https://issues.chromium.org/issues/41490809.
    if (RuntimeEnabledFeatures::InertElementNonEditableEnabled()) {
      const Element* element = DynamicTo<Element>(ancestor);
      if (element && element->IsInertRoot()) {
        return false;
      }
    }

    const ComputedStyle* style = ancestor.GetComputedStyle();
    if (!style)
      continue;
    switch (style->UsedUserModify()) {
      case EUserModify::kReadOnly:
        return false;
      case EUserModify::kReadWrite:
        return true;
      case EUserModify::kReadWritePlaintextOnly:
        return editable_level != kRichlyEditable;
    }
  }

  return false;
}

bool IsEditable(const Node& node) {
  // TODO(editing-dev): We shouldn't check editable style in inactive documents.
  // We should hoist this check in the call stack, replace it by a DCHECK of
  // active document and ultimately cleanup the code paths with inactive
  // documents.  See crbug.com/667681
  if (!node.GetDocument().IsActive())
    return false;

  return HasEditableLevel(node, kEditable);
}

bool IsRichlyEditable(const Node& node) {
  // TODO(editing-dev): We shouldn't check editable style in inactive documents.
  // We should hoist this check in the call stack, replace it by a DCHECK of
  // active document and ultimately cleanup the code paths with inactive
  // documents.  See crbug.com/667681
  if (!node.GetDocument().IsActive())
    return false;

  return HasEditableLevel(node, kRichlyEditable);
}

bool IsRootEditableElement(const Node& node) {
  return IsEditable(node) && node.IsElementNode() &&
         (!node.parentNode() || !IsEditable(*node.parentNode()) ||
          !node.parentNode()->IsElementNode() ||
          &node == node.GetDocument().body());
}

Element* RootEditableElement(const Node& node) {
  const Element* result = nullptr;
  for (const Node* n = &node; n && IsEditable(*n); n = n->parentNode()) {
    if (auto* element = DynamicTo<Element>(n))
      result = element;
    if (node.GetDocument().body() == n)
      break;
  }
  return const_cast<Element*>(result);
}

ContainerNode* HighestEditableRoot(const Position& position) {
  if (position.IsNull())
    return nullptr;

  ContainerNode* highest_root = RootEditableElementOf(position);
  if (!highest_root)
    return nullptr;

  if (IsA<HTMLBodyElement>(*highest_root))
    return highest_root;

  ContainerNode* node = highest_root->parentNode();
  while (node) {
    if (IsEditable(*node))
      highest_root = node;
    if (IsA<HTMLBodyElement>(*node))
      break;
    node = node->parentNode();
  }

  return highest_root;
}

ContainerNode* HighestEditableRoot(const PositionInFlatTree& position) {
  return HighestEditableRoot(ToPositionInDOMTree(position));
}

bool IsEditablePosition(const Position& position) {
  const Node* node = position.ComputeContainerNode();
  if (!node)
    return false;
  DCHECK(node->GetDocument().IsActive());
  if (node->GetDocument().Lifecycle().GetState() >=
      DocumentLifecycle::kInStyleRecalc) {
    // TODO(yosin): Update the condition and DCHECK here given that
    // https://codereview.chromium.org/2665823002/ avoided this function from
    // being called during InStyleRecalc.
  } else {
    DCHECK(!NeedsLayoutTreeUpdate(position)) << position;
  }

  if (IsDisplayInsideTable(node))
    node = node->parentNode();

  if (node->IsDocumentNode())
    return false;
  return IsEditable(*node);
}

bool IsEditablePosition(const PositionInFlatTree& p) {
  return IsEditablePosition(ToPositionInDOMTree(p));
}

bool IsRichlyEditablePosition(const Position& p) {
  const Node* node = p.AnchorNode();
  if (!node)
    return false;

  if (IsDisplayInsideTable(node))
    node = node->parentNode();

  return IsRichlyEditable(*node);
}

Element* RootEditableElementOf(const Position& p) {
  Node* node = p.ComputeContainerNode();
  if (!node)
    return nullptr;

  if (IsDisplayInsideTable(node))
    node = node->parentNode();

  return RootEditableElement(*node);
}

Element* RootEditableElementOf(const PositionInFlatTree& p) {
  return RootEditableElementOf(ToPositionInDOMTree(p));
}

template <typename Strategy>
PositionTemplate<Strategy> NextCandidateAlgorithm(
    const PositionTemplate<Strategy>& position) {
  TRACE_EVENT0("input", "EditingUtility::nextCandidateAlgorithm");
  PositionIteratorAlgorithm<Strategy> p(position);

  p.Increment();
  while (!p.AtEnd()) {
    PositionTemplate<Strategy> candidate = p.ComputePosition();
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;

    p.Increment();
  }

  return PositionTemplate<Strategy>();
}

Position NextCandidate(const Position& position) {
  return NextCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree NextCandidate(const PositionInFlatTree& position) {
  return NextCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

// |nextVisuallyDistinctCandidate| is similar to |nextCandidate| except
// for returning position which |downstream()| not equal to initial position's
// |downstream()|.
template <typename Strategy>
static PositionTemplate<Strategy> NextVisuallyDistinctCandidateAlgorithm(
    const PositionTemplate<Strategy>& position) {
  TRACE_EVENT0("input",
               "EditingUtility::nextVisuallyDistinctCandidateAlgorithm");
  if (position.IsNull())
    return PositionTemplate<Strategy>();

  PositionIteratorAlgorithm<Strategy> p(position);
  const PositionTemplate<Strategy> downstream_start =
      MostForwardCaretPosition(position);
  const PositionTemplate<Strategy> upstream_start =
      MostBackwardCaretPosition(position);

  p.Increment();
  while (!p.AtEnd()) {
    PositionTemplate<Strategy> candidate = p.ComputePosition();
    if (IsVisuallyEquivalentCandidate(candidate) &&
        MostForwardCaretPosition(candidate) != downstream_start &&
        MostBackwardCaretPosition(candidate) != upstream_start)
      return candidate;

    p.Increment();
  }

  return PositionTemplate<Strategy>();
}

Position NextVisuallyDistinctCandidate(const Position& position) {
  return NextVisuallyDistinctCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree NextVisuallyDistinctCandidate(
    const PositionInFlatTree& position) {
  return NextVisuallyDistinctCandidateAlgorithm<EditingInFlatTreeStrategy>(
      position);
}

template <typename Strategy>
PositionTemplate<Strategy> PreviousCandidateAlgorithm(
    const PositionTemplate<Strategy>& position) {
  TRACE_EVENT0("input", "EditingUtility::previousCandidateAlgorithm");
  PositionIteratorAlgorithm<Strategy> p(position);

  p.Decrement();
  while (!p.AtStart()) {
    PositionTemplate<Strategy> candidate = p.ComputePosition();
    if (IsVisuallyEquivalentCandidate(candidate))
      return candidate;

    p.Decrement();
  }

  return PositionTemplate<Strategy>();
}

Position PreviousCandidate(const Position& position) {
  return PreviousCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree PreviousCandidate(const PositionInFlatTree& position) {
  return PreviousCandidateAlgorithm<EditingInFlatTreeStrategy>(position);
}

// |previousVisuallyDistinctCandidate| is similar to |previousCandidate| except
// for returning position which |downstream()| not equal to initial position's
// |downstream()|.
template <typename Strategy>
PositionTemplate<Strategy> PreviousVisuallyDistinctCandidateAlgorithm(
    const PositionTemplate<Strategy>& position) {
  TRACE_EVENT0("input",
               "EditingUtility::previousVisuallyDistinctCandidateAlgorithm");
  if (position.IsNull())
    return PositionTemplate<Strategy>();

  PositionIteratorAlgorithm<Strategy> p(position);
  PositionTemplate<Strategy> downstream_start =
      MostForwardCaretPosition(position);
  const PositionTemplate<Strategy> upstream_start =
      MostBackwardCaretPosition(position);

  p.Decrement();
  while (!p.AtStart()) {
    PositionTemplate<Strategy> candidate = p.ComputePosition();
    if (IsVisuallyEquivalentCandidate(candidate) &&
        MostForwardCaretPosition(candidate) != downstream_start &&
        MostBackwardCaretPosition(candidate) != upstream_start)
      return candidate;

    p.Decrement();
  }

  return PositionTemplate<Strategy>();
}

Position PreviousVisuallyDistinctCandidate(const Position& position) {
  return PreviousVisuallyDistinctCandidateAlgorithm<EditingStrategy>(position);
}

PositionInFlatTree PreviousVisuallyDistinctCandidate(
    const PositionInFlatTree& position) {
  return PreviousVisuallyDistinctCandidateAlgorithm<EditingInFlatTreeStrategy>(
      position);
}

template <typename Strategy>
PositionTemplate<Strategy> FirstEditablePositionAfterPositionInRootAlgorithm(
    const PositionTemplate<Strategy>& position,
    const Node& highest_root) {
  DCHECK(!NeedsLayoutTreeUpdate(highest_root))
      << position << ' ' << highest_root;
  // position falls before highestRoot.
  if (position.CompareTo(PositionTemplate<Strategy>::FirstPositionInNode(
          highest_root)) == -1 &&
      IsEditable(highest_root))
    return PositionTemplate<Strategy>::FirstPositionInNode(highest_root);

  PositionTemplate<Strategy> editable_position = position;

  if (position.AnchorNode()->GetTreeScope() != highest_root.GetTreeScope()) {
    Node* shadow_ancestor = highest_root.GetTreeScope().AncestorInThisScope(
        editable_position.AnchorNode());
    if (!shadow_ancestor)
      return PositionTemplate<Strategy>();

    editable_position = PositionTemplate<Strategy>::AfterNode(*shadow_ancestor);
  }

  Node* non_editable_node = nullptr;
  while (editable_position.AnchorNode() &&
         !IsEditablePosition(editable_position) &&
         editable_position.AnchorNode()->IsDescendantOf(&highest_root)) {
    non_editable_node = editable_position.AnchorNode();
    editable_position = IsAtomicNode(editable_position.AnchorNode())
                            ? PositionTemplate<Strategy>::InParentAfterNode(
                                  *editable_position.AnchorNode())
                            : NextVisuallyDistinctCandidate(editable_position);
  }

  if (editable_position.AnchorNode() &&
      editable_position.AnchorNode() != &highest_root &&
      !editable_position.AnchorNode()->IsDescendantOf(&highest_root))
    return PositionTemplate<Strategy>();

  // If `non_editable_node` is the last child of
  // `editable_position.AnchorNode()`, obtain the next sibling position.
  // - If we do not obtain the next sibling position, we will be unable to
  //   access the next paragraph within the `InsertListCommand::DoApply` while
  //   loop. See http://crbug.com/571420 for more details.
  // - If `non_editable_node` is not the last child, we will bypass the next
  //   editable sibling position. See http://crbug.com/1334557 for more details.
  bool need_obtain_next =
      non_editable_node && editable_position.AnchorNode() &&
      non_editable_node == editable_position.AnchorNode()->lastChild();
  if (need_obtain_next) {
    // Make sure not to move out of |highest_root|
    const PositionTemplate<Strategy> boundary =
        PositionTemplate<Strategy>::LastPositionInNode(highest_root);
    // `NextVisuallyDistinctCandidate` is similar to `NextCandidate`, but
    // it skips the next visually equivalent of `editable_position`.
    // `editable_position` is already "visually distinct" relative to
    // `position`, so use `NextCandidate` here.
    // See http://crbug.com/1406207 for more details.
    const PositionTemplate<Strategy> next_candidate =
        NextCandidate(editable_position);
    editable_position = next_candidate.IsNotNull()
                            ? std::min(boundary, next_candidate)
                            : boundary;
  }
  return editable_position;
}

Position FirstEditablePositionAfterPositionInRoot(const Position& position,
                                                  const Node& highest_root) {
  return FirstEditablePositionAfterPositionInRootAlgorithm<EditingStrategy>(
      position, highest_root);
}

PositionInFlatTree FirstEditablePositionAfterPositionInRoot(
    const PositionInFlatTree& position,
    const Node& highest_root) {
  return FirstEditablePositionAfterPositionInRootAlgorithm<
      EditingInFlatTreeStrategy>(position, highest_root);
}

template <typename Strategy>
PositionTemplate<Strategy> LastEditablePositionBeforePositionInRootAlgorithm(
    const PositionTemplate<Strategy>& position,
    const Node& highest_root) {
  DCHECK(!NeedsLayoutTreeUpdate(highest_root))
      << position << ' ' << highest_root;
  // When position falls after highestRoot, the result is easy to compute.
  if (position.CompareTo(
          PositionTemplate<Strategy>::LastPositionInNode(highest_root)) == 1)
    return PositionTemplate<Strategy>::LastPositionInNode(highest_root);

  PositionTemplate<Strategy> editable_position = position;

  if (position.AnchorNode()->GetTreeScope() != highest_root.GetTreeScope()) {
    Node* shadow_ancestor = highest_root.GetTreeScope().AncestorInThisScope(
        editable_position.AnchorNode());
    if (!shadow_ancestor)
      return PositionTemplate<Strategy>();

    editable_position = PositionTemplate<Strategy>::FirstPositionInOrBeforeNode(
        *shadow_ancestor);
  }

  while (editable_position.AnchorNode() &&
         !IsEditablePosition(editable_position) &&
         editable_position.AnchorNode()->IsDescendantOf(&highest_root))
    editable_position =
        IsAtomicNode(editable_position.AnchorNode())
            ? PositionTemplate<Strategy>::InParentBeforeNode(
                  *editable_position.AnchorNode())
            : PreviousVisuallyDistinctCandidate(editable_position);

  if (editable_position.AnchorNode() &&
      editable_position.AnchorNode() != &highest_root &&
      !editable_position.AnchorNode()->IsDescendantOf(&highest_root))
    return PositionTemplate<Strategy>();
  return editable_position;
}

Position LastEditablePositionBeforePositionInRoot(const Position& position,
                                                  const Node& highest_root) {
  return LastEditablePositionBeforePositionInRootAlgorithm<EditingStrategy>(
      position, highest_root);
}

PositionInFlatTree LastEditablePositionBeforePositionInRoot(
    const PositionInFlatTree& position,
    const Node& highest_root) {
  return LastEditablePositionBeforePositionInRootAlgorithm<
      EditingInFlatTreeStrategy>(position, highest_root);
}

template <typename StateMachine>
int FindNextBoundaryOffset(const String& str, int current) {
  StateMachine machine;
  TextSegmentationMachineState state = TextSegmentationMachineState::kInvalid;

  for (int i = current - 1; i >= 0; --i) {
    state = machine.FeedPrecedingCodeUnit(str[i]);
    if (state != TextSegmentationMachineState::kNeedMoreCodeUnit)
      break;
  }
  if (current == 0 || state == TextSegmentationMachineState::kNeedMoreCodeUnit)
    state = machine.TellEndOfPrecedingText();
  if (state == TextSegmentationMachineState::kFinished)
    return current + machine.FinalizeAndGetBoundaryOffset();
  const int length = str.length();
  DCHECK_EQ(TextSegmentationMachineState::kNeedFollowingCodeUnit, state);
  for (int i = current; i < length; ++i) {
    state = machine.FeedFollowingCodeUnit(str[i]);
    if (state != TextSegmentationMachineState::kNeedMoreCodeUnit)
      break;
  }
  return current + machine.FinalizeAndGetBoundaryOffset();
}

// Explicit instantiation to avoid link error for the usage in EditContext.
template int FindNextBoundaryOffset<BackwardGraphemeBoundaryStateMachine>(
    const String& str,
    int current);
template int FindNextBoundaryOffset<ForwardGraphemeBoundaryStateMachine>(
    const String& str,
    int current);

int PreviousGraphemeBoundaryOf(const Node& node, int current) {
  // TODO(yosin): Need to support grapheme crossing |Node| boundary.
  DCHECK_GE(current, 0);
  auto* text_node = DynamicTo<Text>(node);
  if (current <= 1 || !text_node)
    return current - 1;
  const String& text = text_node->data();
  // TODO(yosin): Replace with DCHECK for out-of-range request.
  if (static_cast<unsigned>(current) > text.length())
    return current - 1;
  return FindNextBoundaryOffset<BackwardGraphemeBoundaryStateMachine>(text,
                                                                      current);
}

static int PreviousBackwardDeletionOffsetOf(const Node& node, int current) {
  DCHECK_GE(current, 0);
  if (current <= 1)
    return 0;
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return current - 1;

  const String& text = text_node->data();
  DCHECK_LT(static_cast<unsigned>(current - 1), text.length());
  return FindNextBoundaryOffset<BackspaceStateMachine>(text, current);
}

int NextGraphemeBoundaryOf(const Node& node, int current) {
  // TODO(yosin): Need to support grapheme crossing |Node| boundary.
  auto* text_node = DynamicTo<Text>(node);
  if (!text_node)
    return current + 1;
  const String& text = text_node->data();
  const int length = text.length();
  DCHECK_LE(current, length);
  if (current >= length - 1)
    return current + 1;
  return FindNextBoundaryOffset<ForwardGraphemeBoundaryStateMachine>(text,
                                                                     current);
}

template <typename Strategy>
PositionTemplate<Strategy> PreviousPositionOfAlgorithm(
    const PositionTemplate<Strategy>& position,
    PositionMoveType move_type) {
  Node* const node = position.AnchorNode();
  if (!node)
    return position;

  const int offset = position.ComputeEditingOffset();

  if (offset > 0) {
    if (EditingIgnoresContent(*node))
      return PositionTemplate<Strategy>::BeforeNode(*node);
    if (Node* child = Strategy::ChildAt(*node, offset - 1)) {
      return PositionTemplate<Strategy>::LastPositionInOrAfterNode(*child);
    }

    // There are two reasons child might be 0:
    //   1) The node is node like a text node that is not an element, and
    //      therefore has no children. Going backward one character at a
    //      time is correct.
    //   2) The old offset was a bogus offset like (<br>, 1), and there is
    //      no child. Going from 1 to 0 is correct.
    switch (move_type) {
      case PositionMoveType::kCodeUnit:
        return PositionTemplate<Strategy>(node, offset - 1);
      case PositionMoveType::kBackwardDeletion:
        return PositionTemplate<Strategy>(
            node, PreviousBackwardDeletionOffsetOf(*node, offset));
      case PositionMoveType::kGraphemeCluster:
        return PositionTemplate<Strategy>(
            node, PreviousGraphemeBoundaryOf(*node, offset));
      default:
        NOTREACHED_IN_MIGRATION() << "Unhandled moveType: " << move_type;
    }
  }

  if (ContainerNode* parent = Strategy::Parent(*node)) {
    if (EditingIgnoresContent(*parent))
      return PositionTemplate<Strategy>::BeforeNode(*parent);
    // TODO(yosin) We should use |Strategy::index(Node&)| instead of
    // |Node::nodeIndex()|.
    return PositionTemplate<Strategy>(parent, node->NodeIndex());
  }
  return position;
}

Position PreviousPositionOf(const Position& position,
                            PositionMoveType move_type) {
  return PreviousPositionOfAlgorithm<EditingStrategy>(position, move_type);
}

PositionInFlatTree PreviousPositionOf(const PositionInFlatTree& position,
                                      PositionMoveType move_type) {
  return PreviousPositionOfAlgorithm<EditingInFlatTreeStrategy>(position,
                                                                move_type);
}

template <typename Strategy>
PositionTemplate<Strategy> NextPositionOfAlgorithm(
    const PositionTemplate<Strategy>& position,
    PositionMoveType move_type) {
  // TODO(yosin): We should have printer for PositionMoveType.
  DCHECK(move_type != PositionMoveType::kBackwardDeletion);

  Node* node = position.AnchorNode();
  if (!node)
    return position;

  const int offset = position.ComputeEditingOffset();

  if (Node* child = Strategy::ChildAt(*node, offset)) {
    return PositionTemplate<Strategy>::FirstPositionInOrBeforeNode(*child);
  }

  // TODO(yosin) We should use |Strategy::lastOffsetForEditing()| instead of
  // DOM tree version.
  if (!Strategy::HasChildren(*node) &&
      offset < EditingStrategy::LastOffsetForEditing(node)) {
    // There are two reasons child might be 0:
    //   1) The node is node like a text node that is not an element, and
    //      therefore has no children. Going forward one character at a time
    //      is correct.
    //   2) The new offset is a bogus offset like (<br>, 1), and there is no
    //      child. Going from 0 to 1 is correct.
    switch (move_type) {
      case PositionMoveType::kCodeUnit:
        return PositionTemplate<Strategy>::EditingPositionOf(node, offset + 1);
      case PositionMoveType::kBackwardDeletion:
        NOTREACHED_IN_MIGRATION()
            << "BackwardDeletion is only available for prevPositionOf "
            << "functions.";
        return PositionTemplate<Strategy>::EditingPositionOf(node, offset + 1);
      case PositionMoveType::kGraphemeCluster:
        return PositionTemplate<Strategy>::EditingPositionOf(
            node, NextGraphemeBoundaryOf(*node, offset));
      default:
        NOTREACHED_IN_MIGRATION() << "Unhandled moveType: " << move_type;
    }
  }

  if (ContainerNode* parent = Strategy::Parent(*node))
    return PositionTemplate<Strategy>::EditingPositionOf(
        parent, Strategy::Index(*node) + 1);
  return position;
}

Position NextPositionOf(const Position& position, PositionMoveType move_type) {
  return NextPositionOfAlgorithm<EditingStrategy>(position, move_type);
}

PositionInFlatTree NextPositionOf(const PositionInFlatTree& position,
                                  PositionMoveType move_type) {
  return NextPositionOfAlgorithm<EditingInFlatTreeStrategy>(position,
                                                            move_type);
}

bool IsEnclosingBlock(const Node* node) {
  return node && node->GetLayoutObject() &&
         !node->GetLayoutObject()->IsInline() &&
         !node->GetLayoutObject()->IsRubyText();
}

// TODO(yosin) Deploy this in all of the places where |enclosingBlockFlow()| and
// |enclosingBlockFlowOrTableElement()| are used.
// TODO(yosin) Callers of |Node| version of |enclosingBlock()| should use
// |Position| version The enclosing block of [table, x] for example, should be
// the block that contains the table and not the table, and this function should
// be the only one responsible for knowing about these kinds of special cases.
Element* EnclosingBlock(const Node* node, EditingBoundaryCrossingRule rule) {
  if (!node)
    return nullptr;
  return EnclosingBlock(FirstPositionInOrBeforeNode(*node), rule);
}

template <typename Strategy>
Element* EnclosingBlockAlgorithm(const PositionTemplate<Strategy>& position,
                                 EditingBoundaryCrossingRule rule) {
  Node* enclosing_node = EnclosingNodeOfType(position, IsEnclosingBlock, rule);
  return DynamicTo<Element>(enclosing_node);
}

Element* EnclosingBlock(const Position& position,
                        EditingBoundaryCrossingRule rule) {
  return EnclosingBlockAlgorithm<EditingStrategy>(position, rule);
}

Element* EnclosingBlock(const PositionInFlatTree& position,
                        EditingBoundaryCrossingRule rule) {
  return EnclosingBlockAlgorithm<EditingInFlatTreeStrategy>(position, rule);
}

Element* EnclosingBlockFlowElement(const Node& node) {
  if (IsBlockFlowElement(node))
    return const_cast<Element*>(To<Element>(&node));

  for (Node& runner : NodeTraversal::AncestorsOf(node)) {
    if (IsBlockFlowElement(runner) || IsA<HTMLBodyElement>(runner))
      return To<Element>(&runner);
  }
  return nullptr;
}

template <typename Strategy>
TextDirection DirectionOfEnclosingBlockOfAlgorithm(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.IsNotNull());
  Element* enclosing_block_element =
      EnclosingBlock(PositionTemplate<Strategy>::FirstPositionInOrBeforeNode(
                         *position.ComputeContainerNode()),
                     kCannotCrossEditingBoundary);
  if (!enclosing_block_element)
    return TextDirection::kLtr;
  LayoutObject* layout_object = enclosing_block_element->GetLayoutObject();
  return layout_object ? layout_object->Style()->Direction()
                       : TextDirection::kLtr;
}

TextDirection DirectionOfEnclosingBlockOf(const Position& position) {
  return DirectionOfEnclosingBlockOfAlgorithm<EditingStrategy>(position);
}

TextDirection DirectionOfEnclosingBlockOf(const PositionInFlatTree& position) {
  return DirectionOfEnclosingBlockOfAlgorithm<EditingInFlatTreeStrategy>(
      position);
}

TextDirection PrimaryDirectionOf(const Node& node) {
  TextDirection primary_direction = TextDirection::kLtr;
  for (const LayoutObject* r = node.GetLayoutObject(); r; r = r->Parent()) {
    if (r->IsLayoutBlockFlow()) {
      primary_direction = r->Style()->Direction();
      break;
    }
  }

  return primary_direction;
}

String StringWithRebalancedWhitespace(const String& string,
                                      bool start_is_start_of_paragraph,
                                      bool should_emit_nbs_pbefore_end) {
  unsigned length = string.length();

  StringBuilder rebalanced_string;
  rebalanced_string.ReserveCapacity(length);

  UChar char_to_append = 0;
  for (wtf_size_t index = 0; index < length; index++) {
    char_to_append = WhitespaceRebalancingCharToAppend(
        string, start_is_start_of_paragraph, should_emit_nbs_pbefore_end, index,
        char_to_append);
    rebalanced_string.Append(char_to_append);
  }

  DCHECK_EQ(rebalanced_string.length(), length);

  return rebalanced_string.ToString();
}

String RepeatString(const String& string, unsigned count) {
  StringBuilder builder;
  builder.ReserveCapacity(string.length() * count);
  for (unsigned counter = 0; counter < count; ++counter)
    builder.Append(string);
  return builder.ToString();
}

template <typename Strategy>
static Element* TableElementJustBeforeAlgorithm(
    const VisiblePositionTemplate<Strategy>& visible_position) {
  const PositionTemplate<Strategy> upstream(
      MostBackwardCaretPosition(visible_position.DeepEquivalent()));
  if (IsDisplayInsideTable(upstream.AnchorNode()) &&
      upstream.AtLastEditingPositionForNode())
    return To<Element>(upstream.AnchorNode());

  return nullptr;
}

Element* TableElementJustBefore(const VisiblePosition& visible_position) {
  return TableElementJustBeforeAlgorithm<EditingStrategy>(visible_position);
}

Element* TableElementJustBefore(
    const VisiblePositionInFlatTree& visible_position) {
  return TableElementJustBeforeAlgorithm<EditingInFlatTreeStrategy>(
      visible_position);
}

Element* EnclosingTableCell(const Position& p) {
  return To<Element>(EnclosingNodeOfType(p, IsTableCell));
}
Element* EnclosingTableCell(const PositionInFlatTree& p) {
  return To<Element>(EnclosingNodeOfType(p, IsTableCell));
}

Element* TableElementJustAfter(const VisiblePosition& visible_position) {
  Position downstream(
      MostForwardCaretPosition(visible_position.DeepEquivalent()));
  if (IsDisplayInsideTable(downstream.AnchorNode()) &&
      downstream.AtFirstEditingPositionForNode())
    return To<Element>(downstream.AnchorNode());

  return nullptr;
}

// Returns the position at the beginning of a node
Position PositionBeforeNode(const Node& node) {
  DCHECK(!NeedsLayoutTreeUpdate(node));
  if (node.hasChildren())
    return FirstPositionInOrBeforeNode(node);
  DCHECK(node.parentNode()) << node;
  DCHECK(!node.parentNode()->IsShadowRoot()) << node.parentNode();
  return Position::InParentBeforeNode(node);
}

// Returns the position at the ending of a node
Position PositionAfterNode(const Node& node) {
  DCHECK(!NeedsLayoutTreeUpdate(node));
  if (node.hasChildren())
    return LastPositionInOrAfterNode(node);
  DCHECK(node.parentNode()) << node.parentNode();
  DCHECK(!node.parentNode()->IsShadowRoot()) << node.parentNode();
  return Position::InParentAfterNode(node);
}

bool IsHTMLListElement(const Node* n) {
  return (n && (IsA<HTMLUListElement>(*n) || IsA<HTMLOListElement>(*n) ||
                IsA<HTMLDListElement>(*n)));
}

bool IsListItem(const Node* n) {
  return n && n->GetLayoutObject() && n->GetLayoutObject()->IsListItem();
}

bool IsListItemTag(const Node* n) {
  return n && (n->HasTagName(html_names::kLiTag) ||
               n->HasTagName(html_names::kDdTag) ||
               n->HasTagName(html_names::kDtTag));
}

bool IsListElementTag(const Node* n) {
  return n && (n->HasTagName(html_names::kUlTag) ||
               n->HasTagName(html_names::kOlTag) ||
               n->HasTagName(html_names::kDlTag));
}

bool IsPresentationalHTMLElement(const Node* node) {
  const auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return false;

  return element->HasTagName(html_names::kUTag) ||
         element->HasTagName(html_names::kSTag) ||
         element->HasTagName(html_names::kStrikeTag) ||
         element->HasTagName(html_names::kITag) ||
         element->HasTagName(html_names::kEmTag) ||
         element->HasTagName(html_names::kBTag) ||
         element->HasTagName(html_names::kStrongTag);
}

Element* AssociatedElementOf(const Position& position) {
  Node* node = position.AnchorNode();
  if (!node)
    return nullptr;

  if (auto* element = DynamicTo<Element>(node))
    return element;

  ContainerNode* parent = NodeTraversal::Parent(*node);
  return DynamicTo<Element>(parent);
}

Element* EnclosingElementWithTag(const Position& p,
                                 const QualifiedName& tag_name) {
  if (p.IsNull())
    return nullptr;

  ContainerNode* root = HighestEditableRoot(p);
  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(*p.AnchorNode())) {
    auto* ancestor = DynamicTo<Element>(runner);
    if (!ancestor)
      continue;
    if (root && !IsEditable(*ancestor))
      continue;
    if (ancestor->HasTagName(tag_name))
      return ancestor;
    if (ancestor == root)
      return nullptr;
  }

  return nullptr;
}

template <typename Strategy>
static Node* EnclosingNodeOfTypeAlgorithm(const PositionTemplate<Strategy>& p,
                                          bool (*node_is_of_type)(const Node*),
                                          EditingBoundaryCrossingRule rule) {
  // TODO(yosin) support CanSkipCrossEditingBoundary
  DCHECK(rule == kCanCrossEditingBoundary ||
         rule == kCannotCrossEditingBoundary)
      << rule;
  if (p.IsNull())
    return nullptr;

  ContainerNode* const root =
      rule == kCannotCrossEditingBoundary ? RootEditableElementOf(p) : nullptr;
  for (Node* n = p.AnchorNode(); n; n = Strategy::Parent(*n)) {
    // Don't return a non-editable node if the input position was editable,
    // since the callers from editing will no doubt want to perform editing
    // inside the returned node.
    if (root && !IsEditable(*n))
      continue;
    if (node_is_of_type(n))
      return n;
    if (n == root)
      return nullptr;
  }

  return nullptr;
}

Node* EnclosingNodeOfType(const Position& p,
                          bool (*node_is_of_type)(const Node*),
                          EditingBoundaryCrossingRule rule) {
  return EnclosingNodeOfTypeAlgorithm<EditingStrategy>(p, node_is_of_type,
                                                       rule);
}

Node* EnclosingNodeOfType(const PositionInFlatTree& p,
                          bool (*node_is_of_type)(const Node*),
                          EditingBoundaryCrossingRule rule) {
  return EnclosingNodeOfTypeAlgorithm<EditingInFlatTreeStrategy>(
      p, node_is_of_type, rule);
}

Node* HighestEnclosingNodeOfType(const Position& p,
                                 bool (*node_is_of_type)(const Node*),
                                 EditingBoundaryCrossingRule rule,
                                 Node* stay_within) {
  Node* highest = nullptr;
  ContainerNode* root =
      rule == kCannotCrossEditingBoundary ? HighestEditableRoot(p) : nullptr;
  for (Node* n = p.ComputeContainerNode(); n && n != stay_within;
       n = n->parentNode()) {
    if (root && !IsEditable(*n))
      continue;
    if (node_is_of_type(n))
      highest = n;
    if (n == root)
      break;
  }

  return highest;
}

Element* EnclosingAnchorElement(const Position& p) {
  if (p.IsNull())
    return nullptr;

  for (Element* ancestor =
           ElementTraversal::FirstAncestorOrSelf(*p.AnchorNode());
       ancestor; ancestor = ElementTraversal::FirstAncestor(*ancestor)) {
    if (ancestor->IsLink())
      return ancestor;
  }
  return nullptr;
}

bool IsDisplayInsideTable(const Node* node) {
  return node && node->GetLayoutObject() && IsA<HTMLTableElement>(node);
}

bool IsTableCell(const Node* node) {
  DCHECK(node);
  LayoutObject* r = node->GetLayoutObject();
  return r ? r->IsTableCell() : IsA<HTMLTableCellElement>(*node);
}

HTMLElement* CreateDefaultParagraphElement(Document& document) {
  switch (document.GetFrame()->GetEditor().DefaultParagraphSeparator()) {
    case EditorParagraphSeparator::kIsDiv:
      return MakeGarbageCollected<HTMLDivElement>(document);
    case EditorParagraphSeparator::kIsP:
      return MakeGarbageCollected<HTMLParagraphElement>(document);
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool IsTabHTMLSpanElement(const Node* node) {
  if (!IsA<HTMLSpanElement>(node))
    return false;
  const Node* const first_child = NodeTraversal::FirstChild(*node);
  auto* first_child_text_node = DynamicTo<Text>(first_child);
  if (!first_child_text_node)
    return false;
  if (!first_child_text_node->data().Contains('\t'))
    return false;
  // TODO(editing-dev): Hoist the call of UpdateStyleAndLayoutTree to callers.
  // See crbug.com/590369 for details.
  node->GetDocument().UpdateStyleAndLayoutTree();
  const ComputedStyle* style = node->GetComputedStyle();
  return style && style->WhiteSpace() == EWhiteSpace::kPre;
}

bool IsTabHTMLSpanElementTextNode(const Node* node) {
  return node && node->IsTextNode() && node->parentNode() &&
         IsTabHTMLSpanElement(node->parentNode());
}

HTMLSpanElement* TabSpanElement(const Node* node) {
  return IsTabHTMLSpanElementTextNode(node)
             ? To<HTMLSpanElement>(node->parentNode())
             : nullptr;
}

static HTMLSpanElement* CreateTabSpanElement(Document& document,
                                             Text* tab_text_node) {
  // Make the span to hold the tab.
  auto* span_element = MakeGarbageCollected<HTMLSpanElement>(document);
  span_element->setAttribute(html_names::kStyleAttr,
                             AtomicString("white-space:pre"));

  // Add tab text to that span.
  if (!tab_text_node)
    tab_text_node = document.CreateEditingTextNode("\t");

  span_element->AppendChild(tab_text_node);

  return span_element;
}

HTMLSpanElement* CreateTabSpanElement(Document& document,
                                      const String& tab_text) {
  return CreateTabSpanElement(document, document.createTextNode(tab_text));
}

HTMLSpanElement* CreateTabSpanElement(Document& document) {
  return CreateTabSpanElement(document, nullptr);
}

static bool IsInPlaceholder(const TextControlElement& text_control,
                            const Position& position) {
  const auto* const placeholder_element = text_control.PlaceholderElement();
  if (!placeholder_element)
    return false;
  return placeholder_element->contains(position.ComputeContainerNode());
}

// Returns user-select:contain boundary element of specified position.
// Because of we've not yet implemented "user-select:contain", we consider
// following elements having "user-select:contain"
//  - root editable
//  - inner editor of text control (<input> and <textarea>)
// Note: inner editor of readonly text control isn't content editable.
// TODO(yosin): We should handle elements with "user-select:contain".
// See http:/crbug.com/658129
static Element* UserSelectContainBoundaryOf(const Position& position) {
  if (auto* text_control = EnclosingTextControl(position)) {
    if (IsInPlaceholder(*text_control, position))
      return nullptr;
    // for <input readonly>. See http://crbug.com/185089
    return text_control->InnerEditorElement();
  }
  // Note: Until we implement "user-select:contain", we treat root editable
  // element and text control as having "user-select:contain".
  if (Element* editable = RootEditableElementOf(position))
    return editable;
  return nullptr;
}

PositionWithAffinity PositionRespectingEditingBoundary(
    const Position& position,
    const HitTestResult& hit_test_result) {
  Node* target_node = hit_test_result.InnerPossiblyPseudoNode();
  DCHECK(target_node);
  const LayoutObject* target_object = target_node->GetLayoutObject();
  if (!target_object)
    return PositionWithAffinity();

  Element* editable_element = UserSelectContainBoundaryOf(position);
  if (!editable_element || editable_element->contains(target_node))
    return hit_test_result.GetPosition();

  const LayoutObject* editable_object = editable_element->GetLayoutObject();
  if (!editable_object || !editable_object->VisibleToHitTesting())
    return PositionWithAffinity();

  // TODO(yosin): Is this kIgnoreTransforms correct here?
  PhysicalOffset selection_end_point = hit_test_result.LocalPoint();
  PhysicalOffset absolute_point = target_object->LocalToAbsolutePoint(
      selection_end_point, kIgnoreTransforms);
  selection_end_point =
      editable_object->AbsoluteToLocalPoint(absolute_point, kIgnoreTransforms);
  target_object = editable_object;
  // TODO(kojii): Support fragment-based |PositionForPoint|. LayoutObject-based
  // |PositionForPoint| may not work if NG block fragmented.
  return target_object->PositionForPoint(selection_end_point);
}

PositionWithAffinity AdjustForEditingBoundary(
    const PositionWithAffinity& position_with_affinity) {
  if (position_with_affinity.IsNull())
    return position_with_affinity;
  const Position& position = position_with_affinity.GetPosition();
  const Node& node = *position.ComputeContainerNode();
  if (IsEditable(node))
    return position_with_affinity;
  // TODO(yosin): Once we fix |MostBackwardCaretPosition()| to handle
  // positions other than |kOffsetInAnchor|, we don't need to use
  // |adjusted_position|, e.g. <outer><inner contenteditable> with position
  // before <inner> vs. outer@0[1].
  // [1] editing/selection/click-outside-editable-div.html
  const Position& adjusted_position = IsEditable(*position.AnchorNode())
                                          ? position.ToOffsetInAnchor()
                                          : position;
  const Position& forward =
      MostForwardCaretPosition(adjusted_position, kCanCrossEditingBoundary);
  if (IsEditable(*forward.ComputeContainerNode()))
    return PositionWithAffinity(forward);
  const Position& backward =
      MostBackwardCaretPosition(adjusted_position, kCanCrossEditingBoundary);
  if (IsEditable(*backward.ComputeContainerNode()))
    return PositionWithAffinity(backward);
  return PositionWithAffinity(adjusted_position,
                              position_with_affinity.Affinity());
}

PositionWithAffinity AdjustForEditingBoundary(const Position& position) {
  return AdjustForEditingBoundary(PositionWithAffinity(position));
}

Position ComputePlaceholderToCollapseAt(const Position& insertion_pos) {
  Position placeholder;
  // We want to remove preserved newlines and brs that will collapse (and thus
  // become unnecessary) when content is inserted just before them.
  // FIXME: We shouldn't really have to do this, but removing placeholders is a
  // workaround for 9661.
  // If the caret is just before a placeholder, downstream will normalize the
  // caret to it.
  Position downstream(MostForwardCaretPosition(insertion_pos));
  if (LineBreakExistsAtPosition(downstream)) {
    // FIXME: This doesn't handle placeholders at the end of anonymous blocks.
    VisiblePosition caret = CreateVisiblePosition(insertion_pos);
    if (IsEndOfBlock(caret) && IsStartOfParagraph(caret)) {
      placeholder = downstream;
    }
    // Don't remove the placeholder yet, otherwise the block we're inserting
    // into would collapse before we get a chance to insert into it.  We check
    // for a placeholder now, though, because doing so requires the creation of
    // a VisiblePosition, and if we did that post-insertion it would force a
    // layout.
  }
  return placeholder;
}

Position ComputePositionForNodeRemoval(const Position& position,
                                       const Node& node) {
  if (position.IsNull())
    return position;
  Node* container_node;
  Node* anchor_node;
  switch (position.AnchorType()) {
    case PositionAnchorType::kAfterChildren:
      container_node = position.ComputeContainerNode();
      if (!container_node ||
          !node.IsShadowIncludingInclusiveAncestorOf(*container_node)) {
        return position;
      }
      return Position::InParentBeforeNode(node);
    case PositionAnchorType::kOffsetInAnchor:
      container_node = position.ComputeContainerNode();
      if (container_node == node.parentNode() &&
          static_cast<unsigned>(position.OffsetInContainerNode()) >
              node.NodeIndex()) {
        return Position(container_node, position.OffsetInContainerNode() - 1);
      }
      if (!container_node ||
          !node.IsShadowIncludingInclusiveAncestorOf(*container_node)) {
        return position;
      }
      return Position::InParentBeforeNode(node);
    case PositionAnchorType::kAfterAnchor:
      anchor_node = position.AnchorNode();
      if (!anchor_node ||
          !node.IsShadowIncludingInclusiveAncestorOf(*anchor_node))
        return position;
      return Position::InParentBeforeNode(node);
    case PositionAnchorType::kBeforeAnchor:
      anchor_node = position.AnchorNode();
      if (!anchor_node ||
          !node.IsShadowIncludingInclusiveAncestorOf(*anchor_node))
        return position;
      return Position::InParentBeforeNode(node);
  }
  NOTREACHED_IN_MIGRATION() << "We should handle all PositionAnchorType";
  return position;
}

bool IsMailHTMLBlockquoteElement(const Node* node) {
  const auto* element = DynamicTo<HTMLElement>(*node);
  if (!element)
    return false;

  return element->HasTagName(html_names::kBlockquoteTag) &&
         element->getAttribute(html_names::kTypeAttr) == "cite";
}

bool ElementCannotHaveEndTag(const Node& node) {
  auto* html_element = DynamicTo<HTMLElement>(node);
  if (!html_element)
    return false;

  return !html_element->ShouldSerializeEndTag();
}

// FIXME: indexForVisiblePosition and visiblePositionForIndex use TextIterators
// to convert between VisiblePositions and indices. But TextIterator iteration
// using TextIteratorEmitsCharactersBetweenAllVisiblePositions does not exactly
// match VisiblePosition iteration, so using them to preserve a selection during
// an editing opertion is unreliable. TextIterator's
// TextIteratorEmitsCharactersBetweenAllVisiblePositions mode needs to be fixed,
// or these functions need to be changed to iterate using actual
// VisiblePositions.
// FIXME: Deploy these functions everywhere that TextIterators are used to
// convert between VisiblePositions and indices.
int IndexForVisiblePosition(const VisiblePosition& visible_position,
                            ContainerNode*& scope) {
  if (visible_position.IsNull())
    return 0;

  Position p(visible_position.DeepEquivalent());
  Document& document = *p.GetDocument();
  DCHECK(!document.NeedsLayoutTreeUpdate());

  ShadowRoot* shadow_root = p.AnchorNode()->ContainingShadowRoot();

  if (shadow_root)
    scope = shadow_root;
  else
    scope = document.documentElement();

  EphemeralRange range(Position::FirstPositionInNode(*scope),
                       p.ParentAnchoredEquivalent());

  const TextIteratorBehavior& behavior =
      TextIteratorBehavior::Builder(
          TextIteratorBehavior::AllVisiblePositionsRangeLengthBehavior())
          .SetSuppressesExtraNewlineEmission(true)
          .Build();
  return TextIterator::RangeLength(range.StartPosition(), range.EndPosition(),
                                   behavior);
}

EphemeralRange MakeRange(const VisiblePosition& start,
                         const VisiblePosition& end) {
  if (start.IsNull() || end.IsNull())
    return EphemeralRange();

  Position s = start.DeepEquivalent().ParentAnchoredEquivalent();
  Position e = end.DeepEquivalent().ParentAnchoredEquivalent();
  if (s.IsNull() || e.IsNull())
    return EphemeralRange();

  return EphemeralRange(s, e);
}

template <typename Strategy>
static EphemeralRangeTemplate<Strategy> NormalizeRangeAlgorithm(
    const EphemeralRangeTemplate<Strategy>& range) {
  DCHECK(range.IsNotNull());
  DCHECK(!range.GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      range.GetDocument().Lifecycle());

  // TODO(yosin) We should not call |parentAnchoredEquivalent()|, it is
  // redundant.
  const PositionTemplate<Strategy> normalized_start =
      MostForwardCaretPosition(range.StartPosition())
          .ParentAnchoredEquivalent();
  const PositionTemplate<Strategy> normalized_end =
      MostBackwardCaretPosition(range.EndPosition()).ParentAnchoredEquivalent();
  // The order of the positions of |start| and |end| can be swapped after
  // upstream/downstream. e.g. editing/pasteboard/copy-display-none.html
  if (normalized_start.CompareTo(normalized_end) > 0)
    return EphemeralRangeTemplate<Strategy>(normalized_end, normalized_start);
  return EphemeralRangeTemplate<Strategy>(normalized_start, normalized_end);
}

EphemeralRange NormalizeRange(const EphemeralRange& range) {
  return NormalizeRangeAlgorithm<EditingStrategy>(range);
}

EphemeralRangeInFlatTree NormalizeRange(const EphemeralRangeInFlatTree& range) {
  return NormalizeRangeAlgorithm<EditingInFlatTreeStrategy>(range);
}

VisiblePosition VisiblePositionForIndex(int index, ContainerNode* scope) {
  if (!scope)
    return VisiblePosition();
  DCHECK(!scope->GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      scope->GetDocument().Lifecycle());

  EphemeralRange range =
      PlainTextRange(index).CreateRangeForSelectionIndexing(*scope);
  // Check for an invalid index. Certain editing operations invalidate indices
  // because of problems with
  // TextIteratorEmitsCharactersBetweenAllVisiblePositions.
  if (range.IsNull())
    return VisiblePosition();
  return CreateVisiblePosition(range.StartPosition());
}

template <typename Strategy>
bool AreSameRangesAlgorithm(Node* node,
                            const PositionTemplate<Strategy>& start_position,
                            const PositionTemplate<Strategy>& end_position) {
  DCHECK(node);
  const EphemeralRange range =
      CreateVisibleSelection(
          SelectionInDOMTree::Builder().SelectAllChildren(*node).Build())
          .ToNormalizedEphemeralRange();
  return ToPositionInDOMTree(start_position) == range.StartPosition() &&
         ToPositionInDOMTree(end_position) == range.EndPosition();
}

bool AreSameRanges(Node* node,
                   const Position& start_position,
                   const Position& end_position) {
  return AreSameRangesAlgorithm<EditingStrategy>(node, start_position,
                                                 end_position);
}

bool AreSameRanges(Node* node,
                   const PositionInFlatTree& start_position,
                   const PositionInFlatTree& end_position) {
  return AreSameRangesAlgorithm<EditingInFlatTreeStrategy>(node, start_position,
                                                           end_position);
}

bool IsRenderedAsNonInlineTableImageOrHR(const Node* node) {
  if (!node)
    return false;
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object || layout_object->IsInline()) {
    return false;
  }
  return layout_object->IsTable() || layout_object->IsImage() ||
         layout_object->IsHR();
}

bool IsNonTableCellHTMLBlockElement(const Node* node) {
  const auto* element = DynamicTo<HTMLElement>(node);
  if (!element)
    return false;

  return element->HasTagName(html_names::kListingTag) ||
         element->HasTagName(html_names::kOlTag) ||
         element->HasTagName(html_names::kPreTag) ||
         element->HasTagName(html_names::kTableTag) ||
         element->HasTagName(html_names::kUlTag) ||
         element->HasTagName(html_names::kXmpTag) ||
         element->HasTagName(html_names::kH1Tag) ||
         element->HasTagName(html_names::kH2Tag) ||
         element->HasTagName(html_names::kH3Tag) ||
         element->HasTagName(html_names::kH4Tag) ||
         element->HasTagName(html_names::kH5Tag);
}

bool IsBlockFlowElement(const Node& node) {
  LayoutObject* layout_object = node.GetLayoutObject();
  return node.IsElementNode() && layout_object &&
         layout_object->IsLayoutBlockFlow();
}

bool IsInPasswordField(const Position& position) {
  TextControlElement* text_control = EnclosingTextControl(position);
  auto* html_input_element = DynamicTo<HTMLInputElement>(text_control);
  return html_input_element && html_input_element->FormControlType() ==
                                   FormControlType::kInputPassword;
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest left grapheme boundary.
wtf_size_t ComputeDistanceToLeftGraphemeBoundary(const Position& position) {
  const Position& adjusted_position = PreviousPositionOf(
      NextPositionOf(position, PositionMoveType::kGraphemeCluster),
      PositionMoveType::kGraphemeCluster);
  DCHECK_EQ(position.AnchorNode(), adjusted_position.AnchorNode());
  DCHECK_GE(position.ComputeOffsetInContainerNode(),
            adjusted_position.ComputeOffsetInContainerNode());
  return static_cast<wtf_size_t>(
      position.ComputeOffsetInContainerNode() -
      adjusted_position.ComputeOffsetInContainerNode());
}

// If current position is at grapheme boundary, return 0; otherwise, return the
// distance to its nearest right grapheme boundary.
wtf_size_t ComputeDistanceToRightGraphemeBoundary(const Position& position) {
  const Position& adjusted_position = NextPositionOf(
      PreviousPositionOf(position, PositionMoveType::kGraphemeCluster),
      PositionMoveType::kGraphemeCluster);
  DCHECK_EQ(position.AnchorNode(), adjusted_position.AnchorNode());
  DCHECK_GE(adjusted_position.ComputeOffsetInContainerNode(),
            position.ComputeOffsetInContainerNode());
  return static_cast<wtf_size_t>(
      adjusted_position.ComputeOffsetInContainerNode() -
      position.ComputeOffsetInContainerNode());
}

gfx::QuadF LocalToAbsoluteQuadOf(const LocalCaretRect& caret_rect) {
  return caret_rect.layout_object->LocalRectToAbsoluteQuad(caret_rect.rect);
}

const StaticRangeVector* TargetRangesForInputEvent(const Node& node) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited. see http://crbug.com/590369 for more details.
  node.GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kEditing);
  if (!IsRichlyEditable(node))
    return nullptr;
  const EphemeralRange& range =
      FirstEphemeralRangeOf(node.GetDocument()
                                .GetFrame()
                                ->Selection()
                                .ComputeVisibleSelectionInDOMTree());
  if (range.IsNull())
    return nullptr;
  return MakeGarbageCollected<StaticRangeVector>(1, StaticRange::Create(range));
}

DispatchEventResult DispatchBeforeInputInsertText(
    Node* target,
    const String& data,
    InputEvent::InputType input_type,
    const StaticRangeVector* ranges) {
  if (!target)
    return DispatchEventResult::kNotCanceled;
  // TODO(editing-dev): Pass appropriate |ranges| after it's defined on spec.
  // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
  InputEvent* before_input_event = InputEvent::CreateBeforeInput(
      input_type, data, InputEvent::EventIsComposing::kNotComposing,
      ranges ? ranges : TargetRangesForInputEvent(*target));
  return target->DispatchEvent(*before_input_event);
}

DispatchEventResult DispatchBeforeInputEditorCommand(
    Node* target,
    InputEvent::InputType input_type,
    const StaticRangeVector* ranges) {
  if (!target)
    return DispatchEventResult::kNotCanceled;
  InputEvent* before_input_event = InputEvent::CreateBeforeInput(
      input_type, g_null_atom, InputEvent::EventIsComposing::kNotComposing,
      ranges);
  return target->DispatchEvent(*before_input_event);
}

DispatchEventResult DispatchBeforeInputDataTransfer(
    Node* target,
    InputEvent::InputType input_type,
    DataTransfer* data_transfer) {
  if (!target)
    return DispatchEventResult::kNotCanceled;

  DCHECK(input_type == InputEvent::InputType::kInsertFromPaste ||
         input_type == InputEvent::InputType::kInsertReplacementText ||
         input_type == InputEvent::InputType::kInsertFromDrop ||
         input_type == InputEvent::InputType::kDeleteByCut)
      << "Unsupported inputType: " << (int)input_type;

  InputEvent* before_input_event;

  if (IsRichlyEditable(*target) || !data_transfer) {
    before_input_event = InputEvent::CreateBeforeInput(
        input_type, data_transfer, InputEvent::EventIsComposing::kNotComposing,
        TargetRangesForInputEvent(*target));
  } else {
    const String& data = data_transfer->getData(kMimeTypeTextPlain);
    // TODO(editing-dev): Pass appropriate |ranges| after it's defined on spec.
    // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
    before_input_event = InputEvent::CreateBeforeInput(
        input_type, data, InputEvent::EventIsComposing::kNotComposing,
        TargetRangesForInputEvent(*target));
  }
  return target->DispatchEvent(*before_input_event);
}

void InsertTextAndSendInputEventsOfTypeInsertReplacementText(
    LocalFrame& frame,
    const String& replacement,
    bool allow_edit_context) {
  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kSpellCheck);

  Document& current_document = *frame.GetDocument();

  // Dispatch 'beforeinput'.
  Element* const target = FindEventTargetFrom(
      frame, frame.Selection().ComputeVisibleSelectionInDOMTree());

  // Copy the original target text into a string, in case the 'beforeinput'
  // event handler modifies the text.
  const String before_input_target_string = target->GetInnerTextWithoutUpdate();

  DataTransfer* const data_transfer = DataTransfer::Create(
      DataTransfer::DataTransferType::kInsertReplacementText,
      DataTransferAccessPolicy::kReadable,
      DataObject::CreateFromString(replacement));

  const bool is_canceled =
      DispatchBeforeInputDataTransfer(
          target, InputEvent::InputType::kInsertReplacementText,
          data_transfer) != DispatchEventResult::kNotCanceled;

  // 'beforeinput' event handler may destroy target frame.
  if (current_document != frame.GetDocument()) {
    return;
  }

  // If the 'beforeinput' event handler has modified the input text, then the
  // replacement text shouldn't be inserted.
  if (target->innerText() != before_input_target_string) {
    return;
  }

  // When allowed, insert the text into the active edit context if it exists.
  if (auto* edit_context =
          frame.GetInputMethodController().GetActiveEditContext()) {
    if (allow_edit_context) {
      edit_context->InsertText(replacement);
    }
    return;
  }

  // TODO(editing-dev): The use of UpdateStyleAndLayout
  // needs to be audited.  See http://crbug.com/590369 for more details.
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kSpellCheck);

  if (is_canceled) {
    return;
  }

  frame.GetEditor().InsertTextWithoutSendingTextEvent(
      replacement, false, nullptr,
      InputEvent::InputType::kInsertReplacementText);
}

// |IsEmptyNonEditableNodeInEditable()| is introduced for fixing
// http://crbug.com/428986.
static bool IsEmptyNonEditableNodeInEditable(const Node& node) {
  // Editability is defined the DOM tree rather than the flat tree. For example:
  // DOM:
  //   <host>
  //     <span>unedittable</span>
  //     <shadowroot><div ce><content /></div></shadowroot>
  //   </host>
  //
  // Flat Tree:
  //   <host><div ce><span1>unedittable</span></div></host>
  // e.g. editing/shadow/breaking-editing-boundaries.html
  return !NodeTraversal::HasChildren(node) && !IsEditable(node) &&
         node.parentNode() && IsEditable(*node.parentNode());
}

// TODO(yosin): We should not use |IsEmptyNonEditableNodeInEditable()| in
// |EditingIgnoresContent()| since |IsEmptyNonEditableNodeInEditable()|
// requires clean layout tree.
bool EditingIgnoresContent(const Node& node) {
  return !node.CanContainRangeEndPoint() ||
         IsEmptyNonEditableNodeInEditable(node);
}

ContainerNode* RootEditableElementOrTreeScopeRootNodeOf(
    const Position& position) {
  Element* const selection_root = RootEditableElementOf(position);
  if (selection_root)
    return selection_root;

  Node* const node = position.ComputeContainerNode();
  return node ? &node->GetTreeScope().RootNode() : nullptr;
}

static scoped_refptr<Image> ImageFromNode(const Node& node) {
  DCHECK(!node.GetDocument().NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      node.GetDocument().Lifecycle());

  const LayoutObject* const layout_object = node.GetLayoutObject();
  if (!layout_object)
    return nullptr;

  if (layout_object->IsCanvas()) {
    return To<HTMLCanvasElement>(const_cast<Node&>(node))
        .Snapshot(FlushReason::kClipboard, kFrontBuffer);
  }

  if (!layout_object->IsImage())
    return nullptr;

  const auto& layout_image = To<LayoutImage>(*layout_object);
  const ImageResourceContent* const cached_image = layout_image.CachedImage();
  if (!cached_image || cached_image->ErrorOccurred())
    return nullptr;
  return cached_image->GetImage();
}

AtomicString GetUrlStringFromNode(const Node& node) {
  // TODO(editing-dev): This should probably be reconciled with
  // HitTestResult::absoluteImageURL.
  if (IsA<HTMLImageElement>(node) || IsA<HTMLInputElement>(node))
    return To<HTMLElement>(node).FastGetAttribute(html_names::kSrcAttr);
  if (IsA<SVGImageElement>(node))
    return To<SVGElement>(node).ImageSourceURL();
  if (IsA<HTMLEmbedElement>(node) || IsA<HTMLObjectElement>(node) ||
      IsA<HTMLCanvasElement>(node))
    return To<HTMLElement>(node).ImageSourceURL();
  return AtomicString();
}

void WriteImageToClipboard(SystemClipboard& system_clipboard,
                           const scoped_refptr<Image>& image,
                           const KURL& url_string,
                           const String& title) {
  system_clipboard.WriteImageWithTag(image.get(), url_string, title);
  system_clipboard.CommitWrite();
}

void WriteImageNodeToClipboard(SystemClipboard& system_clipboard,
                               const Node& node,
                               const String& title) {
  const scoped_refptr<Image> image = ImageFromNode(node);
  if (!image.get())
    return;
  const KURL url_string = node.GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(GetUrlStringFromNode(node)));
  WriteImageToClipboard(system_clipboard, image, url_string, title);
}

Element* FindEventTargetFrom(LocalFrame& frame,
                             const VisibleSelection& selection) {
  Element* const target = AssociatedElementOf(selection.Start());
  if (!target)
    return frame.GetDocument()->body();
  if (target->IsInUserAgentShadowRoot())
    return target->OwnerShadowHost();
  return target;
}

HTMLImageElement* ImageElementFromImageDocument(const Document* document) {
  if (!document)
    return nullptr;
  if (!IsA<ImageDocument>(document))
    return nullptr;

  const HTMLElement* const body = document->body();
  if (!body)
    return nullptr;

  return DynamicTo<HTMLImageElement>(body->firstChild());
}

}  // namespace blink

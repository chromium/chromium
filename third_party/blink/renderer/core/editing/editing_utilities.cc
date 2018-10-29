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

#include "third_party/blink/renderer/core/editing/editing_utilities.h"

#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/data_object.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_strategy.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
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
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_br_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_li_element.h"
#include "third_party/blink/renderer/core/html/html_paragraph_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_ulist_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_element_factory.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

using namespace HTMLNames;

namespace {

std::ostream& operator<<(std::ostream& os, PositionMoveType type) {
  static const char* const kTexts[] = {"CodeUnit", "BackwardDeletion",
                                       "GraphemeCluster"};
  auto* const* const it = std::begin(kTexts) + static_cast<size_t>(type);
  DCHECK_GE(it, std::begin(kTexts)) << "Unknown PositionMoveType value";
  DCHECK_LT(it, std::end(kTexts)) << "Unknown PositionMoveType value";
  return os << *it;
}

InputEvent::EventCancelable InputTypeIsCancelable(
    InputEvent::InputType input_type) {
  using InputType = InputEvent::InputType;
  switch (input_type) {
    case InputType::kInsertText:
    case InputType::kInsertLineBreak:
    case InputType::kInsertParagraph:
    case InputType::kInsertCompositionText:
    case InputType::kInsertReplacementText:
    case InputType::kDeleteWordBackward:
    case InputType::kDeleteWordForward:
    case InputType::kDeleteSoftLineBackward:
    case InputType::kDeleteSoftLineForward:
    case InputType::kDeleteHardLineBackward:
    case InputType::kDeleteHardLineForward:
    case InputType::kDeleteContentBackward:
    case InputType::kDeleteContentForward:
      return InputEvent::EventCancelable::kNotCancelable;
    default:
      return InputEvent::EventCancelable::kIsCancelable;
  }
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

template <typename Traversal>
static int ComparePositions(const Node* container_a,
                            int offset_a,
                            const Node* container_b,
                            int offset_b,
                            bool* disconnected) {
  DCHECK(container_a);
  DCHECK(container_b);

  if (disconnected)
    *disconnected = false;

  if (!container_a)
    return -1;
  if (!container_b)
    return 1;

  // see DOM2 traversal & range section 2.5

  // case 1: both points have the same container
  if (container_a == container_b) {
    if (offset_a == offset_b)
      return 0;  // A is equal to B
    if (offset_a < offset_b)
      return -1;  // A is before B
    return 1;     // A is after B
  }

  // case 2: node C (container B or an ancestor) is a child node of A
  const Node* c = container_b;
  while (c && Traversal::Parent(*c) != container_a)
    c = Traversal::Parent(*c);
  if (c) {
    int offset_c = 0;
    Node* n = Traversal::FirstChild(*container_a);
    while (n != c && offset_c < offset_a) {
      offset_c++;
      n = Traversal::NextSibling(*n);
    }

    if (offset_a <= offset_c)
      return -1;  // A is before B
    return 1;     // A is after B
  }

  // case 3: node C (container A or an ancestor) is a child node of B
  c = container_a;
  while (c && Traversal::Parent(*c) != container_b)
    c = Traversal::Parent(*c);
  if (c) {
    int offset_c = 0;
    Node* n = Traversal::FirstChild(*container_b);
    while (n != c && offset_c < offset_b) {
      offset_c++;
      n = Traversal::NextSibling(*n);
    }

    if (offset_c < offset_b)
      return -1;  // A is before B
    return 1;     // A is after B
  }

  // case 4: containers A & B are siblings, or children of siblings
  // ### we need to do a traversal here instead
  Node* common_ancestor = Traversal::CommonAncestor(*container_a, *container_b);
  if (!common_ancestor) {
    if (disconnected)
      *disconnected = true;
    return 0;
  }
  const Node* child_a = container_a;
  while (child_a && Traversal::Parent(*child_a) != common_ancestor)
    child_a = Traversal::Parent(*child_a);
  if (!child_a)
    child_a = common_ancestor;
  const Node* child_b = container_b;
  while (child_b && Traversal::Parent(*child_b) != common_ancestor)
    child_b = Traversal::Parent(*child_b);
  if (!child_b)
    child_b = common_ancestor;

  if (child_a == child_b)
    return 0;  // A is equal to B

  Node* n = Traversal::FirstChild(*common_ancestor);
  while (n) {
    if (n == child_a)
      return -1;  // A is before B
    if (n == child_b)
      return 1;  // A is after B
    n = Traversal::NextSibling(*n);
  }

  // Should never reach this point.
  NOTREACHED();
  return 0;
}

int ComparePositionsInDOMTree(const Node* container_a,
                              int offset_a,
                              const Node* container_b,
                              int offset_b,
                              bool* disconnected) {
  return ComparePositions<NodeTraversal>(container_a, offset_a, container_b,
                                         offset_b, disconnected);
}

int ComparePositionsInFlatTree(const Node* container_a,
                               int offset_a,
                               const Node* container_b,
                               int offset_b,
                               bool* disconnected) {
  return ComparePositions<FlatTreeTraversal>(container_a, offset_a, container_b,
                                             offset_b, disconnected);
}

// Compare two positions, taking into account the possibility that one or both
// could be inside a shadow tree. Only works for non-null values.
int ComparePositions(const Position& a, const Position& b) {
  DCHECK(a.IsNotNull());
  DCHECK(b.IsNotNull());
  const TreeScope* common_scope = Position::CommonAncestorTreeScope(a, b);

  DCHECK(common_scope);
  if (!common_scope)
    return 0;

  Node* node_a = common_scope->AncestorInThisScope(a.ComputeContainerNode());
  DCHECK(node_a);
  bool has_descendent_a = node_a != a.ComputeContainerNode();
  int offset_a = has_descendent_a ? 0 : a.ComputeOffsetInContainerNode();

  Node* node_b = common_scope->AncestorInThisScope(b.ComputeContainerNode());
  DCHECK(node_b);
  bool has_descendent_b = node_b != b.ComputeContainerNode();
  int offset_b = has_descendent_b ? 0 : b.ComputeOffsetInContainerNode();

  int bias = 0;
  if (node_a == node_b) {
    if (has_descendent_a)
      bias = -1;
    else if (has_descendent_b)
      bias = 1;
  }

  int result = ComparePositionsInDOMTree(node_a, offset_a, node_b, offset_b);
  return result ? result : bias;
}

int ComparePositions(const PositionWithAffinity& a,
                     const PositionWithAffinity& b) {
  return ComparePositions(a.GetPosition(), b.GetPosition());
}

int ComparePositions(const VisiblePosition& a, const VisiblePosition& b) {
  return ComparePositions(a.DeepEquivalent(), b.DeepEquivalent());
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
// TODO(editing-dev): We should make |SelectionAdjuster| to use this funciton
// instead of |isSelectionBondary()|.
bool IsUserSelectContain(const Node& node) {
  return IsHTMLTextAreaElement(node) || IsHTMLInputElement(node) ||
         IsHTMLSelectElement(node);
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
    const ComputedStyle* style = ancestor.GetComputedStyle();
    if (!style)
      continue;
    switch (style->UserModify()) {
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

bool HasEditableStyle(const Node& node) {
  // TODO(editing-dev): We shouldn't check editable style in inactive documents.
  // We should hoist this check in the call stack, replace it by a DCHECK of
  // active document and ultimately cleanup the code paths with inactive
  // documents.  See crbug.com/667681
  if (!node.GetDocument().IsActive())
    return false;

  return HasEditableLevel(node, kEditable);
}

bool HasRichlyEditableStyle(const Node& node) {
  // TODO(editing-dev): We shouldn't check editable style in inactive documents.
  // We should hoist this check in the call stack, replace it by a DCHECK of
  // active document and ultimately cleanup the code paths with inactive
  // documents.  See crbug.com/667681
  if (!node.GetDocument().IsActive())
    return false;

  return HasEditableLevel(node, kRichlyEditable);
}

bool IsRootEditableElement(const Node& node) {
  return HasEditableStyle(node) && node.IsElementNode() &&
         (!node.parentNode() || !HasEditableStyle(*node.parentNode()) ||
          !node.parentNode()->IsElementNode() ||
          &node == node.GetDocument().body());
}

Element* RootEditableElement(const Node& node) {
  const Node* result = nullptr;
  for (const Node* n = &node; n && HasEditableStyle(*n); n = n->parentNode()) {
    if (n->IsElementNode())
      result = n;
    if (node.GetDocument().body() == n)
      break;
  }
  return ToElement(const_cast<Node*>(result));
}

ContainerNode* HighestEditableRoot(
    const Position& position,
    Element* (*root_editable_element_of)(const Position&),
    bool (*has_editable_style)(const Node&)) {
  if (position.IsNull())
    return nullptr;

  ContainerNode* highest_root = root_editable_element_of(position);
  if (!highest_root)
    return nullptr;

  if (IsHTMLBodyElement(*highest_root))
    return highest_root;

  ContainerNode* node = highest_root->parentNode();
  while (node) {
    if (has_editable_style(*node))
      highest_root = node;
    if (IsHTMLBodyElement(*node))
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
  return HasEditableStyle(*node);
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

  return HasRichlyEditableStyle(*node);
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

  p.Increment();
  while (!p.AtEnd()) {
    PositionTemplate<Strategy> candidate = p.ComputePosition();
    if (IsVisuallyEquivalentCandidate(candidate) &&
        MostForwardCaretPosition(candidate) != downstream_start)
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

  p.Decrement();
  while (!p.AtStart()) {
    PositionTemplate<Strategy> candidate = p.ComputePosition();
    if (IsVisuallyEquivalentCandidate(candidate) &&
        MostForwardCaretPosition(candidate) != downstream_start)
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

VisiblePosition FirstEditableVisiblePositionAfterPositionInRoot(
    const Position& position,
    ContainerNode& highest_root) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  return CreateVisiblePosition(
      FirstEditablePositionAfterPositionInRoot(position, highest_root));
}

VisiblePositionInFlatTree FirstEditableVisiblePositionAfterPositionInRoot(
    const PositionInFlatTree& position,
    ContainerNode& highest_root) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  return CreateVisiblePosition(
      FirstEditablePositionAfterPositionInRoot(position, highest_root));
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
      HasEditableStyle(highest_root))
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

  // If |editablePosition| has the non-editable child skipped, get the next
  // sibling position. If not, we can't get the next paragraph in
  // InsertListCommand::doApply's while loop. See http://crbug.com/571420
  if (non_editable_node &&
      non_editable_node->IsDescendantOf(editable_position.AnchorNode()))
    editable_position = NextVisuallyDistinctCandidate(editable_position);
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

VisiblePosition LastEditableVisiblePositionBeforePositionInRoot(
    const Position& position,
    ContainerNode& highest_root) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  return CreateVisiblePosition(
      LastEditablePositionBeforePositionInRoot(position, highest_root));
}

VisiblePositionInFlatTree LastEditableVisiblePositionBeforePositionInRoot(
    const PositionInFlatTree& position,
    ContainerNode& highest_root) {
  DCHECK(!NeedsLayoutTreeUpdate(position));
  return CreateVisiblePosition(
      LastEditablePositionBeforePositionInRoot(position, highest_root));
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

int PreviousGraphemeBoundaryOf(const Node& node, int current) {
  // TODO(yosin): Need to support grapheme crossing |Node| boundary.
  DCHECK_GE(current, 0);
  if (current <= 1 || !node.IsTextNode())
    return current - 1;
  const String& text = ToText(node).data();
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
  if (!node.IsTextNode())
    return current - 1;

  const String& text = ToText(node).data();
  DCHECK_LT(static_cast<unsigned>(current - 1), text.length());
  return FindNextBoundaryOffset<BackspaceStateMachine>(text, current);
}

int NextGraphemeBoundaryOf(const Node& node, int current) {
  // TODO(yosin): Need to support grapheme crossing |Node| boundary.
  if (!node.IsTextNode())
    return current + 1;
  const String& text = ToText(node).data();
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
        NOTREACHED() << "Unhandled moveType: " << move_type;
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
        NOTREACHED() << "BackwardDeletion is only available for prevPositionOf "
                     << "functions.";
        return PositionTemplate<Strategy>::EditingPositionOf(node, offset + 1);
      case PositionMoveType::kGraphemeCluster:
        return PositionTemplate<Strategy>::EditingPositionOf(
            node, NextGraphemeBoundaryOf(*node, offset));
      default:
        NOTREACHED() << "Unhandled moveType: " << move_type;
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
  return enclosing_node && enclosing_node->IsElementNode()
             ? ToElement(enclosing_node)
             : nullptr;
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
    return const_cast<Element*>(&ToElement(node));

  for (Node& runner : NodeTraversal::AncestorsOf(node)) {
    if (IsBlockFlowElement(runner) || IsHTMLBodyElement(runner))
      return ToElement(&runner);
  }
  return nullptr;
}

EUserSelect UsedValueOfUserSelect(const Node& node) {
  if (node.IsHTMLElement() && ToHTMLElement(node).IsTextControl())
    return EUserSelect::kText;
  if (!node.GetLayoutObject())
    return EUserSelect::kNone;

  const ComputedStyle* style = node.GetLayoutObject()->Style();
  if (style->UserModify() != EUserModify::kReadOnly)
    return EUserSelect::kText;

  return style->UserSelect();
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
    return ToElement(upstream.AnchorNode());

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

Element* TableElementJustAfter(const VisiblePosition& visible_position) {
  Position downstream(
      MostForwardCaretPosition(visible_position.DeepEquivalent()));
  if (IsDisplayInsideTable(downstream.AnchorNode()) &&
      downstream.AtFirstEditingPositionForNode())
    return ToElement(downstream.AnchorNode());

  return nullptr;
}

// Returns the visible position at the beginning of a node
VisiblePosition VisiblePositionBeforeNode(const Node& node) {
  DCHECK(!NeedsLayoutTreeUpdate(node));
  if (node.hasChildren())
    return CreateVisiblePosition(FirstPositionInOrBeforeNode(node));
  DCHECK(node.parentNode()) << node;
  DCHECK(!node.parentNode()->IsShadowRoot()) << node.parentNode();
  return VisiblePosition::InParentBeforeNode(node);
}

// Returns the visible position at the ending of a node
VisiblePosition VisiblePositionAfterNode(const Node& node) {
  DCHECK(!NeedsLayoutTreeUpdate(node));
  if (node.hasChildren())
    return CreateVisiblePosition(LastPositionInOrAfterNode(node));
  DCHECK(node.parentNode()) << node.parentNode();
  DCHECK(!node.parentNode()->IsShadowRoot()) << node.parentNode();
  return VisiblePosition::InParentAfterNode(node);
}

bool IsHTMLListElement(const Node* n) {
  return (n && (IsHTMLUListElement(*n) || IsHTMLOListElement(*n) ||
                IsHTMLDListElement(*n)));
}

bool IsListItem(const Node* n) {
  return n && n->GetLayoutObject() && n->GetLayoutObject()->IsListItem();
}

bool IsPresentationalHTMLElement(const Node* node) {
  if (!node->IsHTMLElement())
    return false;

  const HTMLElement& element = ToHTMLElement(*node);
  return element.HasTagName(uTag) || element.HasTagName(sTag) ||
         element.HasTagName(strikeTag) || element.HasTagName(iTag) ||
         element.HasTagName(emTag) || element.HasTagName(bTag) ||
         element.HasTagName(strongTag);
}

Element* AssociatedElementOf(const Position& position) {
  Node* node = position.AnchorNode();
  if (!node || node->IsElementNode())
    return ToElement(node);
  ContainerNode* parent = NodeTraversal::Parent(*node);
  return parent && parent->IsElementNode() ? ToElement(parent) : nullptr;
}

Element* EnclosingElementWithTag(const Position& p,
                                 const QualifiedName& tag_name) {
  if (p.IsNull())
    return nullptr;

  ContainerNode* root = HighestEditableRoot(p);
  Element* ancestor = p.AnchorNode()->IsElementNode()
                          ? ToElement(p.AnchorNode())
                          : p.AnchorNode()->parentElement();
  for (; ancestor; ancestor = ancestor->parentElement()) {
    if (root && !HasEditableStyle(*ancestor))
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
      rule == kCannotCrossEditingBoundary ? HighestEditableRoot(p) : nullptr;
  for (Node* n = p.AnchorNode(); n; n = Strategy::Parent(*n)) {
    // Don't return a non-editable node if the input position was editable,
    // since the callers from editing will no doubt want to perform editing
    // inside the returned node.
    if (root && !HasEditableStyle(*n))
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
    if (root && !HasEditableStyle(*n))
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
  return node && node->GetLayoutObject() && IsHTMLTableElement(node);
}

bool IsTableCell(const Node* node) {
  DCHECK(node);
  LayoutObject* r = node->GetLayoutObject();
  return r ? r->IsTableCell() : IsHTMLTableCellElement(*node);
}

HTMLElement* CreateDefaultParagraphElement(Document& document) {
  switch (document.GetFrame()->GetEditor().DefaultParagraphSeparator()) {
    case EditorParagraphSeparator::kIsDiv:
      return HTMLDivElement::Create(document);
    case EditorParagraphSeparator::kIsP:
      return HTMLParagraphElement::Create(document);
  }

  NOTREACHED();
  return nullptr;
}

bool IsTabHTMLSpanElement(const Node* node) {
  if (!IsHTMLSpanElement(node))
    return false;
  const Node* const first_child = NodeTraversal::FirstChild(*node);
  if (!first_child || !first_child->IsTextNode())
    return false;
  if (!ToText(first_child)->data().Contains('\t'))
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
             ? ToHTMLSpanElement(node->parentNode())
             : nullptr;
}

static HTMLSpanElement* CreateTabSpanElement(Document& document,
                                             Text* tab_text_node) {
  // Make the span to hold the tab.
  HTMLSpanElement* span_element = HTMLSpanElement::Create(document);
  span_element->setAttribute(styleAttr, "white-space:pre");

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

PositionWithAffinity PositionRespectingEditingBoundary(
    const Position& position,
    const LayoutPoint& local_point,
    Node* target_node) {
  if (!target_node->GetLayoutObject())
    return PositionWithAffinity();

  LayoutPoint selection_end_point = local_point;
  Element* editable_element = RootEditableElementOf(position);

  if (editable_element && !editable_element->contains(target_node)) {
    if (!editable_element->GetLayoutObject())
      return PositionWithAffinity();

    FloatPoint absolute_point = target_node->GetLayoutObject()->LocalToAbsolute(
        FloatPoint(selection_end_point));
    selection_end_point = LayoutPoint(
        editable_element->GetLayoutObject()->AbsoluteToLocal(absolute_point));
    target_node = editable_element;
  }

  return target_node->GetLayoutObject()->PositionForPoint(selection_end_point);
}

Position ComputePositionForNodeRemoval(const Position& position,
                                       const Node& node) {
  if (position.IsNull())
    return position;
  switch (position.AnchorType()) {
    case PositionAnchorType::kBeforeChildren:
      if (!node.IsShadowIncludingInclusiveAncestorOf(
              position.ComputeContainerNode())) {
        return position;
      }
      return Position::InParentBeforeNode(node);
    case PositionAnchorType::kAfterChildren:
      if (!node.IsShadowIncludingInclusiveAncestorOf(
              position.ComputeContainerNode())) {
        return position;
      }
      return Position::InParentAfterNode(node);
    case PositionAnchorType::kOffsetInAnchor:
      if (position.ComputeContainerNode() == node.parentNode() &&
          static_cast<unsigned>(position.OffsetInContainerNode()) >
              node.NodeIndex()) {
        return Position(position.ComputeContainerNode(),
                        position.OffsetInContainerNode() - 1);
      }
      if (!node.IsShadowIncludingInclusiveAncestorOf(
              position.ComputeContainerNode())) {
        return position;
      }
      return Position::InParentBeforeNode(node);
    case PositionAnchorType::kAfterAnchor:
      if (!node.IsShadowIncludingInclusiveAncestorOf(position.AnchorNode()))
        return position;
      return Position::InParentAfterNode(node);
    case PositionAnchorType::kBeforeAnchor:
      if (!node.IsShadowIncludingInclusiveAncestorOf(position.AnchorNode()))
        return position;
      return Position::InParentBeforeNode(node);
  }
  NOTREACHED() << "We should handle all PositionAnchorType";
  return position;
}

bool IsMailHTMLBlockquoteElement(const Node* node) {
  if (!node || !node->IsHTMLElement())
    return false;

  const HTMLElement& element = ToHTMLElement(*node);
  return element.HasTagName(blockquoteTag) &&
         element.getAttribute("type") == "cite";
}

bool ElementCannotHaveEndTag(const Node& node) {
  if (!node.IsHTMLElement())
    return false;

  return !ToHTMLElement(node).ShouldSerializeEndTag();
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

bool IsRenderedAsNonInlineTableImageOrHR(const Node* node) {
  if (!node)
    return false;
  LayoutObject* layout_object = node->GetLayoutObject();
  return layout_object &&
         ((layout_object->IsTable() && !layout_object->IsInline()) ||
          (layout_object->IsImage() && !layout_object->IsInline()) ||
          layout_object->IsHR());
}

bool IsNonTableCellHTMLBlockElement(const Node* node) {
  if (!node->IsHTMLElement())
    return false;

  const HTMLElement& element = ToHTMLElement(*node);
  return element.HasTagName(listingTag) || element.HasTagName(olTag) ||
         element.HasTagName(preTag) || element.HasTagName(tableTag) ||
         element.HasTagName(ulTag) || element.HasTagName(xmpTag) ||
         element.HasTagName(h1Tag) || element.HasTagName(h2Tag) ||
         element.HasTagName(h3Tag) || element.HasTagName(h4Tag) ||
         element.HasTagName(h5Tag);
}

bool IsBlockFlowElement(const Node& node) {
  LayoutObject* layout_object = node.GetLayoutObject();
  return node.IsElementNode() && layout_object &&
         layout_object->IsLayoutBlockFlow();
}

bool IsInPasswordField(const Position& position) {
  TextControlElement* text_control = EnclosingTextControl(position);
  return IsHTMLInputElement(text_control) &&
         ToHTMLInputElement(text_control)->type() == InputTypeNames::password;
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

FloatQuad LocalToAbsoluteQuadOf(const LocalCaretRect& caret_rect) {
  return caret_rect.layout_object->LocalToAbsoluteQuad(
      FloatRect(caret_rect.rect));
}

const StaticRangeVector* TargetRangesForInputEvent(const Node& node) {
  // TODO(editing-dev): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited. see http://crbug.com/590369 for more details.
  node.GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (!HasRichlyEditableStyle(node))
    return nullptr;
  const EphemeralRange& range =
      FirstEphemeralRangeOf(node.GetDocument()
                                .GetFrame()
                                ->Selection()
                                .ComputeVisibleSelectionInDOMTree());
  if (range.IsNull())
    return nullptr;
  return new StaticRangeVector(1, StaticRange::Create(range));
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
      input_type, data, InputTypeIsCancelable(input_type),
      InputEvent::EventIsComposing::kNotComposing,
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
      input_type, g_null_atom, InputTypeIsCancelable(input_type),
      InputEvent::EventIsComposing::kNotComposing, ranges);
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

  if (HasRichlyEditableStyle(*(target->ToNode())) || !data_transfer) {
    before_input_event = InputEvent::CreateBeforeInput(
        input_type, data_transfer, InputTypeIsCancelable(input_type),
        InputEvent::EventIsComposing::kNotComposing,
        TargetRangesForInputEvent(*target));
  } else {
    const String& data = data_transfer->getData(kMimeTypeTextPlain);
    // TODO(editing-dev): Pass appropriate |ranges| after it's defined on spec.
    // http://w3c.github.io/editing/input-events.html#dom-inputevent-inputtype
    before_input_event = InputEvent::CreateBeforeInput(
        input_type, data, InputTypeIsCancelable(input_type),
        InputEvent::EventIsComposing::kNotComposing,
        TargetRangesForInputEvent(*target));
  }
  return target->DispatchEvent(*before_input_event);
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
  return !NodeTraversal::HasChildren(node) && !HasEditableStyle(node) &&
         node.parentNode() && HasEditableStyle(*node.parentNode());
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
    return ToHTMLCanvasElement(const_cast<Node&>(node))
        .Snapshot(kFrontBuffer, kPreferNoAcceleration);
  }

  if (!layout_object->IsImage())
    return nullptr;

  const LayoutImage& layout_image = ToLayoutImage(*layout_object);
  const ImageResourceContent* const cached_image = layout_image.CachedImage();
  if (!cached_image || cached_image->ErrorOccurred())
    return nullptr;
  return cached_image->GetImage();
}

AtomicString GetUrlStringFromNode(const Node& node) {
  // TODO(editing-dev): This should probably be reconciled with
  // HitTestResult::absoluteImageURL.
  if (IsHTMLImageElement(node) || IsHTMLInputElement(node))
    return ToHTMLElement(node).getAttribute(srcAttr);
  if (IsSVGImageElement(node))
    return ToSVGElement(node).ImageSourceURL();
  if (IsHTMLEmbedElement(node) || IsHTMLObjectElement(node) ||
      IsHTMLCanvasElement(node))
    return ToHTMLElement(node).ImageSourceURL();
  return AtomicString();
}

void WriteImageNodeToClipboard(const Node& node, const String& title) {
  const scoped_refptr<Image> image = ImageFromNode(node);
  if (!image.get())
    return;
  const KURL url_string = node.GetDocument().CompleteURL(
      StripLeadingAndTrailingHTMLSpaces(GetUrlStringFromNode(node)));
  SystemClipboard::GetInstance().WriteImage(image.get(), url_string, title);
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
  if (!document->IsImageDocument())
    return nullptr;

  const HTMLElement* const body = document->body();
  if (!body)
    return nullptr;

  return ToHTMLImageElementOrNull(body->firstChild());
}

}  // namespace blink

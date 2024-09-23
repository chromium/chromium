/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Motorola Mobility. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/dom/range.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/node_with_index.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_list.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/page/scrolling/sync_scroll_attempt_heuristic.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

class RangeUpdateScope {
  STACK_ALLOCATED();

 public:
  explicit RangeUpdateScope(Range* range) {
    DCHECK(range);
    if (++scope_count_ == 1) {
      range_ = range;
      old_document_ = &range->OwnerDocument();
#if DCHECK_IS_ON()
      current_range_ = range;
    } else {
      DCHECK_EQ(current_range_, range);
#endif
    }
  }
  RangeUpdateScope(const RangeUpdateScope&) = delete;
  RangeUpdateScope& operator=(const RangeUpdateScope&) = delete;

  ~RangeUpdateScope() {
    DCHECK_GE(scope_count_, 1);
    if (--scope_count_ > 0)
      return;
    Settings* settings = old_document_->GetFrame()
                             ? old_document_->GetFrame()->GetSettings()
                             : nullptr;
    if (!settings ||
        !settings->GetDoNotUpdateSelectionOnMutatingSelectionRange()) {
      range_->RemoveFromSelectionIfInDifferentRoot(*old_document_);
      range_->UpdateSelectionIfAddedToSelection();
    }

    range_->ScheduleVisualUpdateIfInRegisteredHighlight(
        range_->OwnerDocument());
    if (*old_document_ != range_->OwnerDocument()) {
      range_->ScheduleVisualUpdateIfInRegisteredHighlight(*old_document_);
    }
#if DCHECK_IS_ON()
    current_range_ = nullptr;
#endif
  }

 private:
  static int scope_count_;
#if DCHECK_IS_ON()
  // This raw pointer is safe because
  //  - s_currentRange has a valid pointer only if RangeUpdateScope instance is
  //  live.
  //  - RangeUpdateScope is used only in Range member functions.
  static Range* current_range_;
#endif
  Range* range_ = nullptr;
  Document* old_document_ = nullptr;

};

int RangeUpdateScope::scope_count_ = 0;
#if DCHECK_IS_ON()
Range* RangeUpdateScope::current_range_;
#endif

Range::Range(Document& owner_document)
    : owner_document_(&owner_document),
      start_(*owner_document_),
      end_(*owner_document_) {
  owner_document_->AttachRange(this);
}

Range* Range::Create(Document& owner_document) {
  return MakeGarbageCollected<Range>(owner_document);
}

Range::Range(Document& owner_document,
             Node* start_container,
             unsigned start_offset,
             Node* end_container,
             unsigned end_offset)
    : owner_document_(&owner_document),
      start_(*owner_document_),
      end_(*owner_document_) {
  owner_document_->AttachRange(this);

  // Simply setting the containers and offsets directly would not do any of the
  // checking that setStart and setEnd do, so we call those functions.
  setStart(start_container, start_offset);
  setEnd(end_container, end_offset);
}

Range::Range(Document& owner_document,
             const Position& start,
             const Position& end)
    : Range(owner_document,
            start.ComputeContainerNode(),
            start.ComputeOffsetInContainerNode(),
            end.ComputeContainerNode(),
            end.ComputeOffsetInContainerNode()) {}

void Range::Dispose() {
  // A prompt detach from the owning Document helps avoid GC overhead.
  owner_document_->DetachRange(this);
}

bool Range::IsConnected() const {
  DCHECK_EQ(start_.IsConnected(), end_.IsConnected());
  return start_.IsConnected();
}

void Range::SetDocument(Document& document) {
  DCHECK_NE(owner_document_, document);
  DCHECK(owner_document_);
  owner_document_->DetachRange(this);
  owner_document_ = &document;
  start_.SetToStartOfNode(document);
  end_.SetToStartOfNode(document);
  owner_document_->AttachRange(this);
}

Node* Range::commonAncestorContainer() const {
  return commonAncestorContainer(&start_.Container(), &end_.Container());
}

Node* Range::commonAncestorContainer(const Node* container_a,
                                     const Node* container_b) {
  if (!container_a || !container_b)
    return nullptr;
  return container_a->CommonAncestor(*container_b, NodeTraversal::Parent);
}

void Range::setStart(Node* ref_node,
                     unsigned offset,
                     ExceptionState& exception_state) {
  if (!ref_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  RangeUpdateScope scope(this);
  bool did_move_document = false;
  if (ref_node->GetDocument() != owner_document_) {
    SetDocument(ref_node->GetDocument());
    did_move_document = true;
  }

  Node* child_node = CheckNodeWOffset(ref_node, offset, exception_state);
  if (exception_state.HadException())
    return;

  start_.Set(*ref_node, offset, child_node);

  CollapseIfNeeded(did_move_document, /*collapse_to_start=*/true);
}

void Range::setEnd(Node* ref_node,
                   unsigned offset,
                   ExceptionState& exception_state) {
  if (!ref_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  RangeUpdateScope scope(this);
  bool did_move_document = false;
  if (ref_node->GetDocument() != owner_document_) {
    SetDocument(ref_node->GetDocument());
    did_move_document = true;
  }

  Node* child_node = CheckNodeWOffset(ref_node, offset, exception_state);
  if (exception_state.HadException())
    return;

  end_.Set(*ref_node, offset, child_node);

  CollapseIfNeeded(did_move_document, /*collapse_to_start=*/false);
}

void Range::setStart(const Position& start, ExceptionState& exception_state) {
  Position parent_anchored = start.ParentAnchoredEquivalent();
  setStart(parent_anchored.ComputeContainerNode(),
           parent_anchored.OffsetInContainerNode(), exception_state);
}

void Range::setEnd(const Position& end, ExceptionState& exception_state) {
  Position parent_anchored = end.ParentAnchoredEquivalent();
  setEnd(parent_anchored.ComputeContainerNode(),
         parent_anchored.OffsetInContainerNode(), exception_state);
}

void Range::collapse(bool to_start) {
  RangeUpdateScope scope(this);
  if (to_start) {
    end_ = start_;
  } else {
    start_ = end_;
  }
  // If Range is collapsed, then the start and end endpoints are the same.
  // It cannot be across a composed tree.
  composed_range_ = nullptr;
}

void Range::CollapseIfNeeded(bool did_move_document, bool collapse_to_start) {
  RangeBoundaryPoint original_start(start_);
  RangeBoundaryPoint original_end(end_);

  bool different_tree_scopes =
      HasDifferentRootContainer(&start_.Container(), &end_.Container());
  // If document moved, we are in different tree scopes, or start boundary point
  // is after end boundary point, we should collapse the range.
  if (did_move_document || different_tree_scopes ||
      compareBoundaryPoints(start_, end_, ASSERT_NO_EXCEPTION) > 0) {
    collapse(collapse_to_start);
  } else {
    // Else, if endpoints should stay as is, then we can return without checking
    // the composed range.
    composed_range_ = nullptr;
    return;
  }
  // If endpoints are in different tree scopes, but in the same document, then
  // we should compare boundary points across the flat tree to determine if
  // composed range should be stored.
  if (RuntimeEnabledFeatures::SelectionAcrossShadowDOMEnabled() &&
      !did_move_document && different_tree_scopes) {
    bool no_common_ancestor = false;
    bool composed_start_before_or_equal_end =
        ComparePositionsInFlatTree(
            &original_start.Container(), original_start.Offset(),
            &original_end.Container(), original_end.Offset(),
            &no_common_ancestor) <= 0;
    // If endpoints are not in the same flat tree, we do not store the composed
    // range.
    if (no_common_ancestor) {
      return;
    }
    if (composed_start_before_or_equal_end) {
      composed_range_ = MakeGarbageCollected<RangeBoundaryPoints>(
          original_start, original_end);
    }
  }
}

bool Range::HasSameRoot(const Node& node) const {
  if (node.GetDocument() != owner_document_)
    return false;
  // commonAncestorContainer() is O(depth). We should avoid to call it in common
  // cases.
  if (node.IsInTreeScope() && start_.Container().IsInTreeScope() &&
      &node.GetTreeScope() == &start_.Container().GetTreeScope())
    return true;
  return node.CommonAncestor(start_.Container(), NodeTraversal::Parent);
}

bool Range::isPointInRange(Node* ref_node,
                           unsigned offset,
                           ExceptionState& exception_state) const {
  if (!ref_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return false;
  }
  if (!HasSameRoot(*ref_node))
    return false;

  CheckNodeWOffset(ref_node, offset, exception_state);
  if (exception_state.HadException())
    return false;

  return compareBoundaryPoints(ref_node, offset, &start_.Container(),
                               start_.Offset(), exception_state) >= 0 &&
         !exception_state.HadException() &&
         compareBoundaryPoints(ref_node, offset, &end_.Container(),
                               end_.Offset(), exception_state) <= 0 &&
         !exception_state.HadException();
}

int16_t Range::comparePoint(Node* ref_node,
                            unsigned offset,
                            ExceptionState& exception_state) const {
  // http://developer.mozilla.org/en/docs/DOM:range.comparePoint
  // This method returns -1, 0 or 1 depending on if the point described by the
  // refNode node and an offset within the node is before, same as, or after the
  // range respectively.

  if (!HasSameRoot(*ref_node)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kWrongDocumentError,
        "The node provided and the Range are not in the same tree.");
    return 0;
  }

  CheckNodeWOffset(ref_node, offset, exception_state);
  if (exception_state.HadException())
    return 0;

  // compare to start, and point comes before
  if (compareBoundaryPoints(ref_node, offset, &start_.Container(),
                            start_.Offset(), exception_state) < 0)
    return -1;

  if (exception_state.HadException())
    return 0;

  // compare to end, and point comes after
  bool start_after_end =
      compareBoundaryPoints(ref_node, offset, &end_.Container(), end_.Offset(),
                            exception_state) > 0;
  if (start_after_end && !exception_state.HadException()) {
    return 1;
  }

  // point is in the middle of this range, or on the boundary points
  return 0;
}

int16_t Range::compareBoundaryPoints(unsigned how,
                                     const Range* source_range,
                                     ExceptionState& exception_state) const {
  if (!(how == kStartToStart || how == kStartToEnd || how == kEndToEnd ||
        how == kEndToStart)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The comparison method provided must be "
        "one of 'START_TO_START', 'START_TO_END', "
        "'END_TO_END', or 'END_TO_START'.");
    return 0;
  }

  Node* this_cont = commonAncestorContainer();
  Node* source_cont = source_range->commonAncestorContainer();
  if (this_cont->GetDocument() != source_cont->GetDocument()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kWrongDocumentError,
        "The source range is in a different document than this range.");
    return 0;
  }

  Node* this_top = this_cont;
  Node* source_top = source_cont;
  while (this_top->parentNode())
    this_top = this_top->parentNode();
  while (source_top->parentNode())
    source_top = source_top->parentNode();
  if (this_top != source_top) {  // in different DocumentFragments
    exception_state.ThrowDOMException(
        DOMExceptionCode::kWrongDocumentError,
        "The source range is in a different document than this range.");
    return 0;
  }

  switch (how) {
    case kStartToStart:
      return compareBoundaryPoints(start_, source_range->start_,
                                   exception_state);
    case kStartToEnd:
      return compareBoundaryPoints(end_, source_range->start_, exception_state);
    case kEndToEnd:
      return compareBoundaryPoints(end_, source_range->end_, exception_state);
    case kEndToStart:
      return compareBoundaryPoints(start_, source_range->end_, exception_state);
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
}

int16_t Range::compareBoundaryPoints(Node* container_a,
                                     unsigned offset_a,
                                     Node* container_b,
                                     unsigned offset_b,
                                     ExceptionState& exception_state) {
  bool disconnected = false;
  int16_t result = ComparePositionsInDOMTree(container_a, offset_a, container_b,
                                             offset_b, &disconnected);
  if (disconnected) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kWrongDocumentError,
        "The two ranges are in separate tree scopes.");
    return 0;
  }
  return result;
}

int16_t Range::compareBoundaryPoints(const RangeBoundaryPoint& boundary_a,
                                     const RangeBoundaryPoint& boundary_b,
                                     ExceptionState& exception_state) {
  return compareBoundaryPoints(&boundary_a.Container(), boundary_a.Offset(),
                               &boundary_b.Container(), boundary_b.Offset(),
                               exception_state);
}

bool Range::BoundaryPointsValid() const {
  DummyExceptionStateForTesting exception_state;
  bool start_after_end =
      compareBoundaryPoints(start_, end_, exception_state) > 0;
  return !start_after_end && !exception_state.HadException();
}

void Range::deleteContents(ExceptionState& exception_state) {
  DCHECK(BoundaryPointsValid());

  {
    EventQueueScope event_queue_scope;
    ProcessContents(kDeleteContents, exception_state);
  }
}

bool Range::intersectsNode(Node* ref_node, ExceptionState& exception_state) {
  // http://developer.mozilla.org/en/docs/DOM:range.intersectsNode
  // Returns a bool if the node intersects the range.
  if (!ref_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return false;
  }
  if (!HasSameRoot(*ref_node))
    return false;

  ContainerNode* parent_node = ref_node->parentNode();
  if (!parent_node)
    return true;

  int node_index = ref_node->NodeIndex();
  return Position(parent_node, node_index) < end_.ToPosition() &&
         Position(parent_node, node_index + 1) > start_.ToPosition();
}

static inline Node* HighestAncestorUnderCommonRoot(Node* node,
                                                   Node* common_root) {
  if (node == common_root)
    return nullptr;

  DCHECK(common_root->contains(node));

  while (node->parentNode() != common_root)
    node = node->parentNode();

  return node;
}

static inline Node* ChildOfCommonRootBeforeOffset(Node* container,
                                                  unsigned offset,
                                                  Node* common_root) {
  DCHECK(container);
  DCHECK(common_root);

  if (!common_root->contains(container))
    return nullptr;

  if (container == common_root) {
    container = container->firstChild();
    for (unsigned i = 0; container && i < offset; i++)
      container = container->nextSibling();
  } else {
    while (container->parentNode() != common_root)
      container = container->parentNode();
  }

  return container;
}

DocumentFragment* Range::ProcessContents(ActionType action,
                                         ExceptionState& exception_state) {
  DocumentFragment* fragment = nullptr;
  if (action == kExtractContents || action == kCloneContents)
    fragment = DocumentFragment::Create(*owner_document_.Get());

  if (collapsed())
    return fragment;

  Node* common_root = commonAncestorContainer();
  DCHECK(common_root);

  if (start_.Container() == end_.Container()) {
    ProcessContentsBetweenOffsets(action, fragment, &start_.Container(),
                                  start_.Offset(), end_.Offset(),
                                  exception_state);
    return fragment;
  }

  // Since mutation observers can modify the range during the process, the
  // boundary points need to be saved.
  const RangeBoundaryPoint original_start(start_);
  const RangeBoundaryPoint original_end(end_);

  // what is the highest node that partially selects the start / end of the
  // range?
  Node* partial_start =
      HighestAncestorUnderCommonRoot(&original_start.Container(), common_root);
  Node* partial_end =
      HighestAncestorUnderCommonRoot(&original_end.Container(), common_root);

  // Start and end containers are different.
  // There are three possibilities here:
  // 1. Start container == commonRoot (End container must be a descendant)
  // 2. End container == commonRoot (Start container must be a descendant)
  // 3. Neither is commonRoot, they are both descendants
  //
  // In case 3, we grab everything after the start (up until a direct child
  // of commonRoot) into leftContents, and everything before the end (up until
  // a direct child of commonRoot) into rightContents. Then we process all
  // commonRoot children between leftContents and rightContents
  //
  // In case 1 or 2, we skip either processing of leftContents or rightContents,
  // in which case the last lot of nodes either goes from the first or last
  // child of commonRoot.
  //
  // These are deleted, cloned, or extracted (i.e. both) depending on action.

  // Note that we are verifying that our common root hierarchy is still intact
  // after any DOM mutation event, at various stages below. See webkit bug
  // 60350.

  Node* left_contents = nullptr;
  if (original_start.Container() != common_root &&
      common_root->contains(&original_start.Container())) {
    left_contents = ProcessContentsBetweenOffsets(
        action, nullptr, &original_start.Container(), original_start.Offset(),
        AbstractRange::LengthOfContents(&original_start.Container()),
        exception_state);
    left_contents = ProcessAncestorsAndTheirSiblings(
        action, &original_start.Container(), kProcessContentsForward,
        left_contents, common_root, exception_state);
  }

  Node* right_contents = nullptr;
  if (end_.Container() != common_root &&
      common_root->contains(&original_end.Container())) {
    right_contents = ProcessContentsBetweenOffsets(
        action, nullptr, &original_end.Container(), 0, original_end.Offset(),
        exception_state);
    right_contents = ProcessAncestorsAndTheirSiblings(
        action, &original_end.Container(), kProcessContentsBackward,
        right_contents, common_root, exception_state);
  }

  if (exception_state.HadException()) {
    return nullptr;
  }

  // delete all children of commonRoot between the start and end container
  Node* process_start = ChildOfCommonRootBeforeOffset(
      &original_start.Container(), original_start.Offset(), common_root);
  // process_start contains nodes before start_.
  if (process_start && original_start.Container() != common_root)
    process_start = process_start->nextSibling();
  Node* process_end = ChildOfCommonRootBeforeOffset(
      &original_end.Container(), original_end.Offset(), common_root);

  // Collapse the range, making sure that the result is not within a node that
  // was partially selected.
  if (action == kExtractContents || action == kDeleteContents) {
    if (partial_start && common_root->contains(partial_start)) {
      setStart(partial_start->parentNode(), partial_start->NodeIndex() + 1,
               exception_state);
    } else if (partial_end && common_root->contains(partial_end)) {
      setStart(partial_end->parentNode(), partial_end->NodeIndex(),
               exception_state);
    }
    if (exception_state.HadException())
      return nullptr;
    end_ = start_;
  }

  // Now add leftContents, stuff in between, and rightContents to the fragment
  // (or just delete the stuff in between)

  if ((action == kExtractContents || action == kCloneContents) && left_contents)
    fragment->AppendChild(left_contents, exception_state);

  if (process_start) {
    NodeVector nodes;
    for (Node* n = process_start; n && n != process_end; n = n->nextSibling())
      nodes.push_back(n);
    ProcessNodes(action, nodes, common_root, fragment, exception_state);
  }

  if ((action == kExtractContents || action == kCloneContents) &&
      right_contents)
    fragment->AppendChild(right_contents, exception_state);

  return fragment;
}

static inline void DeleteCharacterData(CharacterData* data,
                                       unsigned start_offset,
                                       unsigned end_offset,
                                       ExceptionState& exception_state) {
  if (data->length() - end_offset)
    data->deleteData(end_offset, data->length() - end_offset, exception_state);
  if (start_offset)
    data->deleteData(0, start_offset, exception_state);
}

Node* Range::ProcessContentsBetweenOffsets(ActionType action,
                                           DocumentFragment* fragment,
                                           Node* container,
                                           unsigned start_offset,
                                           unsigned end_offset,
                                           ExceptionState& exception_state) {
  DCHECK(container);
  DCHECK_LE(start_offset, end_offset);

  // This switch statement must be consistent with that of
  // lengthOfContents.
  Node* result = nullptr;
  switch (container->getNodeType()) {
    case Node::kTextNode:
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kProcessingInstructionNode:
      end_offset = std::min(end_offset, To<CharacterData>(container)->length());
      if (action == kExtractContents || action == kCloneContents) {
        CharacterData* c =
            static_cast<CharacterData*>(container->cloneNode(true));
        DeleteCharacterData(c, start_offset, end_offset, exception_state);
        if (fragment) {
          result = fragment;
          result->appendChild(c, exception_state);
        } else {
          result = c;
        }
      }
      if (action == kExtractContents || action == kDeleteContents)
        To<CharacterData>(container)->deleteData(
            start_offset, end_offset - start_offset, exception_state);
      break;
    case Node::kElementNode:
    case Node::kAttributeNode:
    case Node::kDocumentNode:
    case Node::kDocumentTypeNode:
    case Node::kDocumentFragmentNode:
      // FIXME: Should we assert that some nodes never appear here?
      if (action == kExtractContents || action == kCloneContents) {
        if (fragment)
          result = fragment;
        else
          result = container->cloneNode(false);
      }

      Node* n = container->firstChild();
      NodeVector nodes;
      for (unsigned i = start_offset; n && i; i--)
        n = n->nextSibling();
      for (unsigned i = start_offset; n && i < end_offset;
           i++, n = n->nextSibling())
        nodes.push_back(n);

      ProcessNodes(action, nodes, container, result, exception_state);
      break;
  }

  return result;
}

void Range::ProcessNodes(ActionType action,
                         NodeVector& nodes,
                         Node* old_container,
                         Node* new_container,
                         ExceptionState& exception_state) {
  for (auto& node : nodes) {
    switch (action) {
      case kDeleteContents:
        old_container->removeChild(node.Get(), exception_state);
        break;
      case kExtractContents:
        new_container->appendChild(
            node.Release(), exception_state);  // Will remove n from its parent.
        break;
      case kCloneContents:
        new_container->appendChild(node->cloneNode(true), exception_state);
        break;
    }
  }
}

Node* Range::ProcessAncestorsAndTheirSiblings(
    ActionType action,
    Node* container,
    ContentsProcessDirection direction,
    Node* cloned_container,
    Node* common_root,
    ExceptionState& exception_state) {
  NodeVector ancestors;
  for (Node& runner : NodeTraversal::AncestorsOf(*container)) {
    if (runner == common_root)
      break;
    ancestors.push_back(runner);
  }
  // Both https://dom.spec.whatwg.org/#concept-range-clone and
  // https://dom.spec.whatwg.org/#concept-range-extract specify (in various
  // ways) that nodes are to be processed in tree order. But the algorithm below
  // processes in depth first order instead. So clone the nodes first here,
  // in reverse order, so upgrades happen in the proper order.
  HeapVector<Member<Node>> cloned_ancestors(ancestors.size(), nullptr);
  auto clone_ptr = cloned_ancestors.rbegin();
  for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
    *(clone_ptr++) = (*it)->cloneNode(false);
  }

  Node* first_child_in_ancestor_to_process =
      direction == kProcessContentsForward ? container->nextSibling()
                                           : container->previousSibling();
  for (wtf_size_t i = 0; i < ancestors.size(); ++i) {
    const auto& ancestor = ancestors[i];
    if (action == kExtractContents || action == kCloneContents) {
      // Might have been removed already during mutation event.
      if (auto cloned_ancestor = cloned_ancestors[i]) {
        cloned_ancestor->appendChild(cloned_container, exception_state);
        cloned_container = cloned_ancestor;
      }
    }

    // Copy siblings of an ancestor of start/end containers
    // FIXME: This assertion may fail if DOM is modified during mutation event
    // FIXME: Share code with Range::processNodes
    DCHECK(!first_child_in_ancestor_to_process ||
           first_child_in_ancestor_to_process->parentNode() == ancestor);

    NodeVector nodes;
    for (Node* child = first_child_in_ancestor_to_process; child;
         child = (direction == kProcessContentsForward)
                     ? child->nextSibling()
                     : child->previousSibling())
      nodes.push_back(child);

    for (const auto& node : nodes) {
      Node* child = node.Get();
      switch (action) {
        case kDeleteContents:
          // Prior call of ancestor->removeChild() may cause a tree change due
          // to DOMSubtreeModified event.  Therefore, we need to make sure
          // |ancestor| is still |child|'s parent.
          if (ancestor == child->parentNode())
            ancestor->removeChild(child, exception_state);
          break;
        case kExtractContents:  // will remove child from ancestor
          if (direction == kProcessContentsForward)
            cloned_container->appendChild(child, exception_state);
          else
            cloned_container->insertBefore(
                child, cloned_container->firstChild(), exception_state);
          break;
        case kCloneContents:
          if (direction == kProcessContentsForward)
            cloned_container->appendChild(child->cloneNode(true),
                                          exception_state);
          else
            cloned_container->insertBefore(child->cloneNode(true),
                                           cloned_container->firstChild(),
                                           exception_state);
          break;
      }
    }
    first_child_in_ancestor_to_process = direction == kProcessContentsForward
                                             ? ancestor->nextSibling()
                                             : ancestor->previousSibling();
  }

  return cloned_container;
}

DocumentFragment* Range::extractContents(ExceptionState& exception_state) {
  CheckExtractPrecondition(exception_state);
  if (exception_state.HadException())
    return nullptr;

  EventQueueScope scope;
  DocumentFragment* fragment =
      ProcessContents(kExtractContents, exception_state);
  // |extractContents| has extended attributes [NewObject, DoNotTestNewObject],
  // so it's better to have a test that exercises the following condition:
  //
  //   !fragment || DOMDataStore::GetWrapper(fragment, isolate).IsEmpty()
  //
  // however, there is no access to |isolate| so far.  So, we simply omit the
  // test so far.
  return fragment;
}

DocumentFragment* Range::cloneContents(ExceptionState& exception_state) {
  return ProcessContents(kCloneContents, exception_state);
}

// https://dom.spec.whatwg.org/#concept-range-insert
void Range::insertNode(Node* new_node, ExceptionState& exception_state) {
  if (!new_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  // 1. If range’s start node is a ProcessingInstruction or Comment node, is a
  // Text node whose parent is null, or is node, then throw a
  // HierarchyRequestError.
  Node& start_node = start_.Container();
  if (start_node.getNodeType() == Node::kProcessingInstructionNode ||
      start_node.getNodeType() == Node::kCommentNode) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Nodes of type '" + new_node->nodeName() +
            "' may not be inserted inside nodes of type '" +
            start_node.nodeName() + "'.");
    return;
  }
  const bool start_is_text = start_node.IsTextNode();
  if (start_is_text && !start_node.parentNode()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "This operation would split a text node, "
                                      "but there's no parent into which to "
                                      "insert.");
    return;
  }
  if (start_node == new_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Unable to insert a node into a Range starting from the node itself.");
    return;
  }

  // According to the specification, the following condition is checked in the
  // step 6. However our EnsurePreInsertionValidity() supports only
  // ContainerNode parent.
  if (start_node.IsAttributeNode()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "Nodes of type '" + new_node->nodeName() +
            "' may not be inserted inside nodes of type 'Attr'.");
    return;
  }

  // 2. Let referenceNode be null.
  Node* reference_node = nullptr;
  // 3. If range’s start node is a Text node, set referenceNode to that Text
  // node.
  // 4. Otherwise, set referenceNode to the child of start node whose index is
  // start offset, and null if there is no such child.
  if (start_is_text)
    reference_node = &start_node;
  else
    reference_node = NodeTraversal::ChildAt(start_node, start_.Offset());

  // 5. Let parent be range’s start node if referenceNode is null, and
  // referenceNode’s parent otherwise.
  ContainerNode& parent = reference_node ? *reference_node->parentNode()
                                         : To<ContainerNode>(start_node);

  // 6. Ensure pre-insertion validity of node into parent before referenceNode.
  if (!parent.EnsurePreInsertionValidity(*new_node, reference_node, nullptr,
                                         exception_state))
    return;

  EventQueueScope scope;
  // 7. If range's start node is a Text node, set referenceNode to the result of
  // splitting it with offset range’s start offset.
  if (start_is_text) {
    reference_node =
        To<Text>(start_node).splitText(start_.Offset(), exception_state);
    if (exception_state.HadException())
      return;
  }

  // 8. If node is referenceNode, set referenceNode to its next sibling.
  if (new_node == reference_node)
    reference_node = reference_node->nextSibling();

  // 9. If node's parent is not null, remove node from its parent.
  if (new_node->parentNode()) {
    new_node->remove(exception_state);
    if (exception_state.HadException())
      return;
  }

  // 10. Let newOffset be parent's length if referenceNode is null, and
  // referenceNode's index otherwise.
  unsigned new_offset = reference_node
                            ? reference_node->NodeIndex()
                            : AbstractRange::LengthOfContents(&parent);

  // 11. Increase newOffset by node's length if node is a DocumentFragment node,
  // and one otherwise.
  new_offset += new_node->IsDocumentFragment()
                    ? AbstractRange::LengthOfContents(new_node)
                    : 1;

  // 12. Pre-insert node into parent before referenceNode.
  parent.insertBefore(new_node, reference_node, exception_state);
  if (exception_state.HadException())
    return;

  // 13. If range's start and end are the same, set range's end to (parent,
  // newOffset).
  if (start_ == end_)
    setEnd(&parent, new_offset, exception_state);
}

String Range::toString() const {
  StringBuilder builder;

  Node* past_last = PastLastNode();
  for (Node* n = FirstNode(); n != past_last; n = NodeTraversal::Next(*n)) {
    Node::NodeType type = n->getNodeType();
    if (type == Node::kTextNode || type == Node::kCdataSectionNode) {
      String data = To<CharacterData>(n)->data();
      unsigned length = data.length();
      unsigned start =
          (n == start_.Container()) ? std::min(start_.Offset(), length) : 0;
      unsigned end = (n == end_.Container())
                         ? std::min(std::max(start, end_.Offset()), length)
                         : length;
      builder.Append(data, start, end - start);
    }
  }

  return builder.ReleaseString();
}

String Range::GetText() const {
  DCHECK(!owner_document_->NeedsLayoutTreeUpdate());
  return PlainText(EphemeralRange(this),
                   TextIteratorBehavior::Builder()
                       .SetEmitsObjectReplacementCharacter(true)
                       .Build());
}

DocumentFragment* Range::createContextualFragment(
    const String& markup,
    ExceptionState& exception_state) {
  // Algorithm:
  // http://domparsing.spec.whatwg.org/#extensions-to-the-range-interface

  DCHECK(!markup.IsNull());

  Node* node = &start_.Container();

  // Step 1.
  Element* element;
  if (!start_.Offset() &&
      (node->IsDocumentNode() || node->IsDocumentFragment()))
    element = nullptr;
  else if (auto* node_element = DynamicTo<Element>(node))
    element = node_element;
  else
    element = node->parentElement();

  // Step 2.
  if (!element || IsA<HTMLHtmlElement>(element)) {
    Document& document = node->GetDocument();

    if (document.IsSVGDocument()) {
      element = document.documentElement();
      if (!element)
        element = MakeGarbageCollected<SVGSVGElement>(document);
    } else {
      // Optimization over spec: try to reuse the existing <body> element, if it
      // is available.
      element = document.body();
      if (!element)
        element = MakeGarbageCollected<HTMLBodyElement>(document);
    }
  }

  // Steps 3, 4, 5.
  return blink::CreateContextualFragment(
      markup, element, kAllowScriptingContentAndDoNotMarkAlreadyStarted,
      exception_state);
}

void Range::detach() {
  // This is now a no-op as per the DOM specification.
}

Node* Range::CheckNodeWOffset(Node* n,
                              unsigned offset,
                              ExceptionState& exception_state) {
  switch (n->getNodeType()) {
    case Node::kDocumentTypeNode:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidNodeTypeError,
          "The node provided is of type '" + n->nodeName() + "'.");
      return nullptr;
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kTextNode:
      if (offset > To<CharacterData>(n)->length()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The offset " + String::Number(offset) +
                " is larger than the node's length (" +
                String::Number(To<CharacterData>(n)->length()) + ").");
      } else if (offset >
                 static_cast<unsigned>(std::numeric_limits<int>::max())) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The offset " + String::Number(offset) + " is invalid.");
      }
      return nullptr;
    case Node::kProcessingInstructionNode:
      if (offset > To<ProcessingInstruction>(n)->data().length()) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The offset " + String::Number(offset) +
                " is larger than the node's length (" +
                String::Number(To<ProcessingInstruction>(n)->data().length()) +
                ").");
      } else if (offset >
                 static_cast<unsigned>(std::numeric_limits<int>::max())) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The offset " + String::Number(offset) + " is invalid.");
      }
      return nullptr;
    case Node::kAttributeNode:
    case Node::kDocumentFragmentNode:
    case Node::kDocumentNode:
    case Node::kElementNode: {
      if (!offset)
        return nullptr;
      if (offset > static_cast<unsigned>(std::numeric_limits<int>::max())) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "The offset " + String::Number(offset) + " is invalid.");
        return nullptr;
      }
      Node* child_before = NodeTraversal::ChildAt(*n, offset - 1);
      if (!child_before) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kIndexSizeError,
            "There is no child at offset " + String::Number(offset) + ".");
      }
      return child_before;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void Range::CheckNodeBA(Node* n, ExceptionState& exception_state) const {
  if (!n) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  // InvalidNodeTypeError: Raised if the root container of refNode is not an
  // Attr, Document, DocumentFragment or ShadowRoot node, or part of a SVG
  // shadow DOM tree, or if refNode is a Document, DocumentFragment, ShadowRoot,
  // Attr, Entity, or Notation node.

  if (!n->parentNode()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "the given Node has no parent.");
    return;
  }

  switch (n->getNodeType()) {
    case Node::kAttributeNode:
    case Node::kDocumentFragmentNode:
    case Node::kDocumentNode:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidNodeTypeError,
          "The node provided is of type '" + n->nodeName() + "'.");
      return;
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kDocumentTypeNode:
    case Node::kElementNode:
    case Node::kProcessingInstructionNode:
    case Node::kTextNode:
      break;
  }

  Node* root = n;
  while (ContainerNode* parent = root->parentNode())
    root = parent;

  switch (root->getNodeType()) {
    case Node::kAttributeNode:
    case Node::kDocumentNode:
    case Node::kDocumentFragmentNode:
    case Node::kElementNode:
      break;
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kDocumentTypeNode:
    case Node::kProcessingInstructionNode:
    case Node::kTextNode:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidNodeTypeError,
          "The node provided is of type '" + n->nodeName() + "'.");
      return;
  }
}

Range* Range::cloneRange() const {
  return MakeGarbageCollected<Range>(*owner_document_.Get(),
                                     &start_.Container(), start_.Offset(),
                                     &end_.Container(), end_.Offset());
}

void Range::setStartAfter(Node* ref_node, ExceptionState& exception_state) {
  CheckNodeBA(ref_node, exception_state);
  if (exception_state.HadException())
    return;

  setStart(ref_node->parentNode(), ref_node->NodeIndex() + 1, exception_state);
}

void Range::setEndBefore(Node* ref_node, ExceptionState& exception_state) {
  CheckNodeBA(ref_node, exception_state);
  if (exception_state.HadException())
    return;

  setEnd(ref_node->parentNode(), ref_node->NodeIndex(), exception_state);
}

void Range::setEndAfter(Node* ref_node, ExceptionState& exception_state) {
  CheckNodeBA(ref_node, exception_state);
  if (exception_state.HadException())
    return;

  setEnd(ref_node->parentNode(), ref_node->NodeIndex() + 1, exception_state);
}

void Range::selectNode(Node* ref_node, ExceptionState& exception_state) {
  if (!ref_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  if (!ref_node->parentNode()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidNodeTypeError,
                                      "the given Node has no parent.");
    return;
  }

  switch (ref_node->getNodeType()) {
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kDocumentTypeNode:
    case Node::kElementNode:
    case Node::kProcessingInstructionNode:
    case Node::kTextNode:
      break;
    case Node::kAttributeNode:
    case Node::kDocumentFragmentNode:
    case Node::kDocumentNode:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidNodeTypeError,
          "The node provided is of type '" + ref_node->nodeName() + "'.");
      return;
  }

  RangeUpdateScope scope(this);
  setStartBefore(ref_node);
  setEndAfter(ref_node);
}

void Range::selectNodeContents(Node* ref_node,
                               ExceptionState& exception_state) {
  if (!ref_node) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  // InvalidNodeTypeError: Raised if refNode or an ancestor of refNode is an
  // Entity, Notation
  // or DocumentType node.
  for (Node* n = ref_node; n; n = n->parentNode()) {
    switch (n->getNodeType()) {
      case Node::kAttributeNode:
      case Node::kCdataSectionNode:
      case Node::kCommentNode:
      case Node::kDocumentFragmentNode:
      case Node::kDocumentNode:
      case Node::kElementNode:
      case Node::kProcessingInstructionNode:
      case Node::kTextNode:
        break;
      case Node::kDocumentTypeNode:
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidNodeTypeError,
            "The node provided is of type '" + ref_node->nodeName() + "'.");
        return;
    }
  }

  RangeUpdateScope scope(this);
  if (owner_document_ != ref_node->GetDocument())
    SetDocument(ref_node->GetDocument());

  start_.SetToStartOfNode(*ref_node);
  end_.SetToEndOfNode(*ref_node);
}

bool Range::selectNodeContents(Node* ref_node, Position& start, Position& end) {
  if (!ref_node) {
    return false;
  }

  for (Node* n = ref_node; n; n = n->parentNode()) {
    switch (n->getNodeType()) {
      case Node::kAttributeNode:
      case Node::kCdataSectionNode:
      case Node::kCommentNode:
      case Node::kDocumentFragmentNode:
      case Node::kDocumentNode:
      case Node::kElementNode:
      case Node::kProcessingInstructionNode:
      case Node::kTextNode:
        break;
      case Node::kDocumentTypeNode:
        return false;
    }
  }

  RangeBoundaryPoint start_boundary_point(*ref_node);
  start_boundary_point.SetToStartOfNode(*ref_node);
  start = start_boundary_point.ToPosition();
  RangeBoundaryPoint end_boundary_point(*ref_node);
  end_boundary_point.SetToEndOfNode(*ref_node);
  end = end_boundary_point.ToPosition();
  return true;
}

// https://dom.spec.whatwg.org/#dom-range-surroundcontents
void Range::surroundContents(Node* new_parent,
                             ExceptionState& exception_state) {
  if (!new_parent) {
    // FIXME: Generated bindings code never calls with null, and neither should
    // other callers!
    exception_state.ThrowTypeError("The node provided is null.");
    return;
  }

  // 1. If a non-Text node is partially contained in the context object, then
  // throw an InvalidStateError.
  Node* start_node = &start_.Container();
  Node* end_node = &end_.Container();
  if (start_node->IsTextNode()) {
    start_node = start_node->parentNode();
  }
  if (end_node->IsTextNode()) {
    end_node = end_node->parentNode();
  }
  if (start_node != end_node) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The Range has partially selected a non-Text node.");
    return;
  }

  // 2. If newParent is a Document, DocumentType, or DocumentFragment node, then
  // throw an InvalidNodeTypeError.
  switch (new_parent->getNodeType()) {
    case Node::kAttributeNode:
    case Node::kDocumentFragmentNode:
    case Node::kDocumentNode:
    case Node::kDocumentTypeNode:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidNodeTypeError,
          "The node provided is of type '" + new_parent->nodeName() + "'.");
      return;
    case Node::kCdataSectionNode:
    case Node::kCommentNode:
    case Node::kElementNode:
    case Node::kProcessingInstructionNode:
    case Node::kTextNode:
      break;
  }

  EventQueueScope scope;

  // 3. Let fragment be the result of extracting context object.
  DocumentFragment* fragment = extractContents(exception_state);
  if (exception_state.HadException())
    return;

  // 4. If newParent has children, replace all with null within newParent.
  while (Node* n = new_parent->firstChild()) {
    To<ContainerNode>(new_parent)->RemoveChild(n, exception_state);
    if (exception_state.HadException())
      return;
  }

  // 5. If newParent has children, replace all with null within newParent.
  insertNode(new_parent, exception_state);
  if (exception_state.HadException())
    return;

  // 6. Append fragment to newParent.
  new_parent->appendChild(fragment, exception_state);
  if (exception_state.HadException())
    return;

  // 7. Select newParent within context object.
  selectNode(new_parent, exception_state);
}

void Range::setStartBefore(Node* ref_node, ExceptionState& exception_state) {
  CheckNodeBA(ref_node, exception_state);
  if (exception_state.HadException())
    return;

  setStart(ref_node->parentNode(), ref_node->NodeIndex(), exception_state);
}

void Range::CheckExtractPrecondition(ExceptionState& exception_state) {
  DCHECK(BoundaryPointsValid());

  if (!commonAncestorContainer())
    return;

  Node* past_last = PastLastNode();
  for (Node* n = FirstNode(); n != past_last; n = NodeTraversal::Next(*n)) {
    if (n->IsDocumentTypeNode()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kHierarchyRequestError,
          "The Range contains a doctype node.");
      return;
    }
  }
}

Node* Range::FirstNode() const {
  return StartPosition().NodeAsRangeFirstNode();
}

Node* Range::PastLastNode() const {
  return EndPosition().NodeAsRangePastLastNode();
}

gfx::Rect Range::BoundingBox() const {
  return ComputeTextRect(EphemeralRange(this));
}

bool AreRangesEqual(const Range* a, const Range* b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  return a->StartPosition() == b->StartPosition() &&
         a->EndPosition() == b->EndPosition();
}

static inline void BoundaryNodeChildrenWillBeRemoved(
    RangeBoundaryPoint& boundary,
    ContainerNode& container) {
  if (container.contains(&boundary.Container())) {
    boundary.SetToStartOfNode(container);
  }
}

static void BoundaryShadowNodeChildrenWillBeRemoved(
    RangeBoundaryPoint& boundary,
    ContainerNode& container) {
  if (boundary.Container().IsDescendantOrShadowDescendantOf(&container)) {
    boundary.SetToStartOfNode(container);
  }
}

void Range::NodeChildrenWillBeRemoved(ContainerNode& container) {
  DCHECK_EQ(container.GetDocument(), owner_document_);
  BoundaryNodeChildrenWillBeRemoved(start_, container);
  BoundaryNodeChildrenWillBeRemoved(end_, container);
}

void Range::FixupRemovedChildrenAcrossShadowBoundary(ContainerNode& container) {
  DCHECK_EQ(container.GetDocument(), owner_document_);
  BoundaryShadowNodeChildrenWillBeRemoved(start_, container);
  BoundaryShadowNodeChildrenWillBeRemoved(end_, container);
}

// Returns true if `boundary` was modified.
static inline bool BoundaryNodeWillBeRemoved(RangeBoundaryPoint& boundary,
                                             Node& node_to_be_removed) {
  if (boundary.ChildBefore() == node_to_be_removed) {
    boundary.ChildBeforeWillBeRemoved();
    return true;
  }

  for (Node* n = &boundary.Container(); n; n = n->parentNode()) {
    if (n == node_to_be_removed) {
      boundary.SetToBeforeChild(node_to_be_removed);
      return true;
    }
  }
  return false;
}

static inline void BoundaryShadowNodeWillBeRemoved(RangeBoundaryPoint& boundary,
                                                   Node& node_to_be_removed) {
  DCHECK_NE(boundary.ChildBefore(), node_to_be_removed);

  for (Node* node = &boundary.Container(); node;
       node = node->ParentOrShadowHostElement()) {
    if (node == node_to_be_removed) {
      boundary.SetToBeforeChild(node_to_be_removed);
      return;
    }
  }
}

void Range::NodeWillBeRemoved(Node& node) {
  DCHECK_EQ(node.GetDocument(), owner_document_);
  DCHECK_NE(node, owner_document_.Get());

  // FIXME: Once DOMNodeRemovedFromDocument mutation event removed, we
  // should change following if-statement to DCHECK(!node->parentNode).
  if (!node.parentNode())
    return;
  const bool is_collapsed = collapsed();
  const bool start_updated = BoundaryNodeWillBeRemoved(start_, node);
  if (is_collapsed) {
    if (start_updated)
      end_ = start_;
  } else {
    BoundaryNodeWillBeRemoved(end_, node);
  }
}

void Range::FixupRemovedNodeAcrossShadowBoundary(Node& node) {
  BoundaryShadowNodeWillBeRemoved(start_, node);
  BoundaryShadowNodeWillBeRemoved(end_, node);
}

static inline void BoundaryTextInserted(RangeBoundaryPoint& boundary,
                                        const CharacterData& text,
                                        unsigned offset,
                                        unsigned length) {
  if (boundary.Container() != &text)
    return;
  boundary.MarkValid();
  unsigned boundary_offset = boundary.Offset();
  if (offset >= boundary_offset)
    return;
  boundary.SetOffset(boundary_offset + length);
}

void Range::DidInsertText(const CharacterData& text,
                          unsigned offset,
                          unsigned length) {
  DCHECK_EQ(text.GetDocument(), owner_document_);
  BoundaryTextInserted(start_, text, offset, length);
  BoundaryTextInserted(end_, text, offset, length);
}

static inline void BoundaryTextRemoved(RangeBoundaryPoint& boundary,
                                       const CharacterData& text,
                                       unsigned offset,
                                       unsigned length) {
  if (boundary.Container() != &text)
    return;
  boundary.MarkValid();
  unsigned boundary_offset = boundary.Offset();
  if (offset >= boundary_offset)
    return;
  if (offset + length >= boundary_offset)
    boundary.SetOffset(offset);
  else
    boundary.SetOffset(boundary_offset - length);
}

void Range::DidRemoveText(const CharacterData& text,
                          unsigned offset,
                          unsigned length) {
  DCHECK_EQ(text.GetDocument(), owner_document_);
  BoundaryTextRemoved(start_, text, offset, length);
  BoundaryTextRemoved(end_, text, offset, length);
}

static inline void BoundaryTextNodesMerged(RangeBoundaryPoint& boundary,
                                           const NodeWithIndex& old_node,
                                           unsigned offset) {
  if (boundary.Container() == old_node.GetNode()) {
    Node* const previous_sibling = old_node.GetNode().previousSibling();
    DCHECK(previous_sibling);
    boundary.Set(*previous_sibling, boundary.Offset() + offset, nullptr);
  } else if (boundary.Container() == old_node.GetNode().parentNode() &&
             boundary.Offset() == static_cast<unsigned>(old_node.Index())) {
    Node* const previous_sibling = old_node.GetNode().previousSibling();
    DCHECK(previous_sibling);
    boundary.Set(*previous_sibling, offset, nullptr);
  }
}

void Range::DidMergeTextNodes(const NodeWithIndex& old_node, unsigned offset) {
  DCHECK_EQ(old_node.GetNode().GetDocument(), owner_document_);
  DCHECK(old_node.GetNode().parentNode());
  DCHECK(old_node.GetNode().IsTextNode());
  DCHECK(old_node.GetNode().previousSibling());
  DCHECK(old_node.GetNode().previousSibling()->IsTextNode());
  BoundaryTextNodesMerged(start_, old_node, offset);
  BoundaryTextNodesMerged(end_, old_node, offset);
}

void Range::UpdateOwnerDocumentIfNeeded() {
  Document& new_document = start_.Container().GetDocument();
  DCHECK_EQ(new_document, end_.Container().GetDocument());
  if (new_document == owner_document_)
    return;
  owner_document_->DetachRange(this);
  owner_document_ = &new_document;
  owner_document_->AttachRange(this);
}

static inline void BoundaryTextNodeSplit(RangeBoundaryPoint& boundary,
                                         const Text& old_node) {
  unsigned boundary_offset = boundary.Offset();
  if (boundary.ChildBefore() == &old_node) {
    boundary.Set(boundary.Container(), boundary_offset + 1,
                 old_node.nextSibling());
  } else if (boundary.Container() == &old_node &&
             boundary_offset > old_node.length()) {
    Node* const next_sibling = old_node.nextSibling();
    DCHECK(next_sibling);
    boundary.Set(*next_sibling, boundary_offset - old_node.length(), nullptr);
  }
}

void Range::DidSplitTextNode(const Text& old_node) {
  DCHECK_EQ(old_node.GetDocument(), owner_document_);
  DCHECK(old_node.parentNode());
  DCHECK(old_node.nextSibling());
  DCHECK(old_node.nextSibling()->IsTextNode());
  BoundaryTextNodeSplit(start_, old_node);
  BoundaryTextNodeSplit(end_, old_node);
  DCHECK(BoundaryPointsValid());
}

void Range::expand(const String& unit, ExceptionState& exception_state) {
  if (!StartPosition().IsConnected() || !EndPosition().IsConnected())
    return;
  owner_document_->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  VisiblePosition start = CreateVisiblePosition(StartPosition());
  VisiblePosition end = CreateVisiblePosition(EndPosition());
  if (unit == "word") {
    start = CreateVisiblePosition(StartOfWordPosition(start.DeepEquivalent()));
    end = CreateVisiblePosition(EndOfWordPosition(end.DeepEquivalent()));
  } else if (unit == "sentence") {
    start =
        CreateVisiblePosition(StartOfSentencePosition(start.DeepEquivalent()));
    end = CreateVisiblePosition(EndOfSentence(end.DeepEquivalent()));
  } else if (unit == "block") {
    start = StartOfParagraph(start);
    end = EndOfParagraph(end);
  } else if (unit == "document") {
    start = CreateVisiblePosition(StartOfDocument(start.DeepEquivalent()));
    end = EndOfDocument(end);
  } else {
    return;
  }
  setStart(start.DeepEquivalent().ComputeContainerNode(),
           start.DeepEquivalent().ComputeOffsetInContainerNode(),
           exception_state);
  setEnd(end.DeepEquivalent().ComputeContainerNode(),
         end.DeepEquivalent().ComputeOffsetInContainerNode(), exception_state);
}

DOMRectList* Range::getClientRects() const {
  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();
  DisplayLockUtilities::ScopedForcedUpdate force_locks(
      this, DisplayLockContext::ForcedPhase::kLayout);
  owner_document_->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  Vector<gfx::QuadF> quads;
  GetBorderAndTextQuads(quads);

  return MakeGarbageCollected<DOMRectList>(quads);
}

DOMRect* Range::getBoundingClientRect() const {
  // TODO(crbug.com/1499981): This should be removed once synchronized scrolling
  // impact is understood.
  SyncScrollAttemptHeuristic::DidAccessScrollOffset();
  return DOMRect::FromRectF(BoundingRect());
}

// TODO(editing-dev): We should make
// |Document::AdjustQuadsForScrollAndAbsoluteZoom()| as const function
// and takes |const LayoutObject&|.
static Vector<gfx::QuadF> ComputeTextQuads(const Document& owner_document,
                                           const LayoutText& layout_text,
                                           unsigned start_offset,
                                           unsigned end_offset) {
  Vector<gfx::QuadF> text_quads;
  layout_text.AbsoluteQuadsForRange(text_quads, start_offset, end_offset);
  const_cast<Document&>(owner_document)
      .AdjustQuadsForScrollAndAbsoluteZoom(
          text_quads, const_cast<LayoutText&>(layout_text));
  return text_quads;
}

// https://www.w3.org/TR/cssom-view-1/#dom-range-getclientrects
void Range::GetBorderAndTextQuads(Vector<gfx::QuadF>& quads) const {
  Node* start_container = &start_.Container();
  Node* end_container = &end_.Container();
  Node* stop_node = PastLastNode();

  // Stores the elements selected by the range.
  HeapHashSet<Member<const Node>> selected_elements;
  for (Node* node = FirstNode(); node != stop_node;
       node = NodeTraversal::Next(*node)) {
    if (!node->IsElementNode())
      continue;
    auto* parent_node = node->parentNode();
    if ((parent_node && selected_elements.Contains(parent_node)) ||
        (!node->contains(start_container) && !node->contains(end_container))) {
      DCHECK_LE(StartPosition(), Position::BeforeNode(*node));
      DCHECK_GE(EndPosition(), Position::AfterNode(*node));
      selected_elements.insert(node);
    }
  }

  for (const Node* node = FirstNode(); node != stop_node;
       node = NodeTraversal::Next(*node)) {
    auto* element_node = DynamicTo<Element>(node);
    if (element_node) {
      if (!selected_elements.Contains(node) ||
          selected_elements.Contains(node->parentNode()))
        continue;
      LayoutObject* const layout_object = element_node->GetLayoutObject();
      if (!layout_object)
        continue;
      Vector<gfx::QuadF> element_quads;
      layout_object->AbsoluteQuads(element_quads);
      owner_document_->AdjustQuadsForScrollAndAbsoluteZoom(element_quads,
                                                           *layout_object);

      quads.AppendVector(element_quads);
      continue;
    }

    auto* const text_node = DynamicTo<Text>(node);
    if (!text_node)
      continue;
    LayoutText* const layout_text = text_node->GetLayoutObject();
    if (!layout_text)
      continue;

    // TODO(editing-dev): Offset in |LayoutText| doesn't match to DOM offset
    // when |text-transform| applied. We should map DOM offset to offset in
    // |LayouText| for |start_offset| and |end_offset|.
    const unsigned start_offset =
        (node == start_container) ? start_.Offset() : 0;
    const unsigned end_offset = (node == end_container)
                                    ? end_.Offset()
                                    : std::numeric_limits<unsigned>::max();
    if (!layout_text->IsTextFragment()) {
      quads.AppendVector(ComputeTextQuads(*owner_document_, *layout_text,
                                          start_offset, end_offset));
      continue;
    }

    // Handle ::first-letter
    const auto& first_letter_part =
        *To<LayoutTextFragment>(AssociatedLayoutObjectOf(*node, 0));
    const bool overlaps_with_first_letter =
        start_offset < first_letter_part.FragmentLength() ||
        (start_offset == first_letter_part.FragmentLength() &&
         end_offset == start_offset);
    if (overlaps_with_first_letter) {
      const unsigned start_in_first_letter = start_offset;
      const unsigned end_in_first_letter =
          std::min(end_offset, first_letter_part.FragmentLength());
      quads.AppendVector(ComputeTextQuads(*owner_document_, first_letter_part,
                                          start_in_first_letter,
                                          end_in_first_letter));
    }
    const auto& remaining_part = *To<LayoutTextFragment>(layout_text);
    if (end_offset > remaining_part.Start()) {
      const unsigned start_in_remaining_part =
          std::max(start_offset, remaining_part.Start()) -
          remaining_part.Start();
      // TODO(editing-dev): As we previously set |end_offset == UINT_MAX| as a
      // hacky support for |text-transform|, we need the same hack here.
      const unsigned end_in_remaining_part =
          end_offset == UINT_MAX ? end_offset
                                 : end_offset - remaining_part.Start();
      quads.AppendVector(ComputeTextQuads(*owner_document_, remaining_part,
                                          start_in_remaining_part,
                                          end_in_remaining_part));
    }
  }
}

gfx::RectF Range::BoundingRect() const {
  std::optional<DisplayLockUtilities::ScopedForcedUpdate> force_locks;
  if (!collapsed()) {
    force_locks = DisplayLockUtilities::ScopedForcedUpdate(
        this, DisplayLockContext::ForcedPhase::kLayout);
  } else {
    force_locks = DisplayLockUtilities::ScopedForcedUpdate(
        FirstNode(), DisplayLockContext::ForcedPhase::kLayout);
  }
  owner_document_->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);

  Vector<gfx::QuadF> quads;
  GetBorderAndTextQuads(quads);

  gfx::RectF result;
  for (const gfx::QuadF& quad : quads)
    result.Union(quad.BoundingBox());  // Skips empty rects.

  // If all rects are empty, return the first rect.
  if (result.IsEmpty() && !quads.empty())
    return quads.front().BoundingBox();

  return result;
}

void Range::UpdateSelectionIfAddedToSelection() {
  if (!OwnerDocument().GetFrame())
    return;
  FrameSelection& selection = OwnerDocument().GetFrame()->Selection();
  if (this != selection.DocumentCachedRange())
    return;
  DCHECK(startContainer()->isConnected());
  DCHECK(startContainer()->GetDocument() == OwnerDocument());
  DCHECK(endContainer()->isConnected());
  DCHECK(endContainer()->GetDocument() == OwnerDocument());
  EventDispatchForbiddenScope no_events;
  selection.SetSelection(SelectionInDOMTree::Builder()
                             .Collapse(StartPosition())
                             .Extend(EndPosition())
                             .Build(),
                         SetSelectionOptions::Builder()
                             .SetShouldCloseTyping(true)
                             .SetShouldClearTypingStyle(true)
                             .SetDoNotSetFocus(true)
                             .Build());
  selection.CacheRangeOfDocument(this);
}

void Range::ScheduleVisualUpdateIfInRegisteredHighlight(Document& document) {
  if (LocalDOMWindow* window = document.domWindow()) {
    if (HighlightRegistry* highlight_registry =
            window->Supplementable<LocalDOMWindow>::RequireSupplement<
                HighlightRegistry>()) {
      for (const auto& highlight_registry_map_entry :
           highlight_registry->GetHighlights()) {
        const auto& highlight = highlight_registry_map_entry->highlight;
        if (highlight->Contains(this)) {
          highlight_registry->ScheduleRepaint();
          return;
        }
      }
    }
  }
}

void Range::RemoveFromSelectionIfInDifferentRoot(Document& old_document) {
  if (!old_document.GetFrame())
    return;
  FrameSelection& selection = old_document.GetFrame()->Selection();
  if (this != selection.DocumentCachedRange())
    return;
  if (OwnerDocument() == old_document && startContainer()->isConnected() &&
      endContainer()->isConnected())
    return;
  selection.Clear();
  selection.ClearDocumentCachedRange();
}

void Range::Trace(Visitor* visitor) const {
  visitor->Trace(owner_document_);
  visitor->Trace(start_);
  visitor->Trace(end_);
  visitor->Trace(composed_range_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

#if DCHECK_IS_ON()

void ShowTree(const blink::Range* range) {
  if (range && range->BoundaryPointsValid()) {
    LOG(INFO) << "\n"
              << range->startContainer()
                     ->ToMarkedTreeString(range->startContainer(), "S",
                                          range->endContainer(), "E")
                     .Utf8()
              << "start offset: " << range->startOffset()
              << ", end offset: " << range->endOffset();
  } else {
    LOG(INFO) << "Cannot show tree if range is null, or if boundary points are "
                 "invalid.";
  }
}

#endif

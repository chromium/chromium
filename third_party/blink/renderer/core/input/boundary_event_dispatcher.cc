// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/boundary_event_dispatcher.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"

namespace blink {

namespace {

void BuildAncestorChain(EventTarget* target,
                        HeapVector<Member<Node>, 20>* ancestors) {
  if (!event_handling_util::IsInDocument(target))
    return;
  Node* target_node = target->ToNode();
  DCHECK(target_node);
  target_node->UpdateDistributionForFlatTreeTraversal();
  // Index 0 element in the ancestors arrays will be the corresponding
  // target. So the root of their document will be their last element.
  for (Node* node = target_node; node; node = FlatTreeTraversal::Parent(*node))
    ancestors->push_back(node);
}

void BuildAncestorChainsAndFindCommonAncestors(
    EventTarget* exited_target,
    EventTarget* entered_target,
    HeapVector<Member<Node>, 20>* exited_ancestors_out,
    HeapVector<Member<Node>, 20>* entered_ancestors_out,
    wtf_size_t* exited_ancestors_common_parent_index_out,
    wtf_size_t* entered_ancestors_common_parent_index_out) {
  DCHECK(exited_ancestors_out);
  DCHECK(entered_ancestors_out);
  DCHECK(exited_ancestors_common_parent_index_out);
  DCHECK(entered_ancestors_common_parent_index_out);

  BuildAncestorChain(exited_target, exited_ancestors_out);
  BuildAncestorChain(entered_target, entered_ancestors_out);

  *exited_ancestors_common_parent_index_out = exited_ancestors_out->size();
  *entered_ancestors_common_parent_index_out = entered_ancestors_out->size();
  while (*exited_ancestors_common_parent_index_out > 0 &&
         *entered_ancestors_common_parent_index_out > 0) {
    if ((*exited_ancestors_out)[(*exited_ancestors_common_parent_index_out) -
                                1] !=
        (*entered_ancestors_out)[(*entered_ancestors_common_parent_index_out) -
                                 1])
      break;
    (*exited_ancestors_common_parent_index_out)--;
    (*entered_ancestors_common_parent_index_out)--;
  }
}

}  // namespace

void BoundaryEventDispatcher::SendBoundaryEvents(EventTarget* exited_target,
                                                 EventTarget* entered_target) {
  if (exited_target == entered_target)
    return;

  // Dispatch out event
  if (event_handling_util::IsInDocument(exited_target))
    DispatchOut(exited_target, entered_target);

  // Create lists of all exited/entered ancestors, locate the common ancestor
  // Based on httparchive, in more than 97% cases the depth of DOM is less
  // than 20.
  HeapVector<Member<Node>, 20> exited_ancestors;
  HeapVector<Member<Node>, 20> entered_ancestors;
  wtf_size_t exited_ancestors_common_parent_index = 0;
  wtf_size_t entered_ancestors_common_parent_index = 0;

  // A note on mouseenter and mouseleave: These are non-bubbling events, and
  // they are dispatched if there is a capturing event handler on an ancestor or
  // a normal event handler on the element itself. This special handling is
  // necessary to avoid O(n^2) capturing event handler checks.
  //
  // Note, however, that this optimization can possibly cause some
  // unanswered/missing/redundant mouseenter or mouseleave events in certain
  // contrived eventhandling scenarios, e.g., when:
  // - the mouseleave handler for a node sets the only
  //   capturing-mouseleave-listener in its ancestor, or
  // - DOM mods in any mouseenter/mouseleave handler changes the common ancestor
  //   of exited & entered nodes, etc.
  // We think the spec specifies a "frozen" state to avoid such corner cases
  // (check the discussion on "candidate event listeners" at
  // http://www.w3.org/TR/uievents), but our code below preserves one such
  // behavior from past only to match Firefox and IE behavior.
  //
  // TODO(mustaq): Confirm spec conformance, double-check with other browsers.

  BuildAncestorChainsAndFindCommonAncestors(
      exited_target, entered_target, &exited_ancestors, &entered_ancestors,
      &exited_ancestors_common_parent_index,
      &entered_ancestors_common_parent_index);

  bool exited_node_has_capturing_ancestor = false;
  const AtomicString& leave_event = GetLeaveEvent();
  for (wtf_size_t j = 0; j < exited_ancestors.size(); j++) {
    if (exited_ancestors[j]->HasCapturingEventListeners(leave_event)) {
      exited_node_has_capturing_ancestor = true;
      break;
    }
  }

  // Dispatch leave events, in child-to-parent order.
  for (wtf_size_t j = 0; j < exited_ancestors_common_parent_index; j++)
    DispatchLeave(exited_ancestors[j], entered_target,
                  !exited_node_has_capturing_ancestor);

  // Dispatch over event
  if (event_handling_util::IsInDocument(entered_target))
    DispatchOver(entered_target, exited_target);

  // Defer locating capturing enter listener until /after/ dispatching the leave
  // events because the leave handlers might set a capturing enter handler.
  bool entered_node_has_capturing_ancestor = false;
  const AtomicString& enter_event = GetEnterEvent();
  for (wtf_size_t i = 0; i < entered_ancestors.size(); i++) {
    if (entered_ancestors[i]->HasCapturingEventListeners(enter_event)) {
      entered_node_has_capturing_ancestor = true;
      break;
    }
  }

  // Dispatch enter events, in parent-to-child order.
  for (wtf_size_t i = entered_ancestors_common_parent_index; i > 0; i--)
    DispatchEnter(entered_ancestors[i - 1], exited_target,
                  !entered_node_has_capturing_ancestor);
}

}  // namespace blink

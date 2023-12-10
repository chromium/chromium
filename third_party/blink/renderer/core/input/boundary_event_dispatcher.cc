// Copyright 2016 The Chromium Authors
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

void BoundaryEventDispatcher::SendBoundaryEvents(
    EventTarget* exited_target,
    bool original_exited_target_removed,
    EventTarget* entered_target) {
  if (exited_target == entered_target && !original_exited_target_removed) {
    return;
  }

  // Dispatch out event
  if (event_handling_util::IsInDocument(exited_target) &&
      !original_exited_target_removed) {
    Dispatch(exited_target, entered_target, out_event_, false);
  }

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
  // See https://crbug.com/1501368.

  BuildAncestorChainsAndFindCommonAncestors(
      exited_target, entered_target, &exited_ancestors, &entered_ancestors,
      &exited_ancestors_common_parent_index,
      &entered_ancestors_common_parent_index);

  bool exited_node_has_capturing_ancestor = false;
  for (wtf_size_t j = 0; j < exited_ancestors.size(); j++) {
    if (exited_ancestors[j]->HasCapturingEventListeners(leave_event_)) {
      exited_node_has_capturing_ancestor = true;
      break;
    }
  }

  // Dispatch leave events, in child-to-parent order.
  for (wtf_size_t j = 0; j < exited_ancestors_common_parent_index; j++) {
    Dispatch(exited_ancestors[j], entered_target, leave_event_,
             !exited_node_has_capturing_ancestor);
  }

  // Dispatch over event
  if (event_handling_util::IsInDocument(entered_target)) {
    Dispatch(entered_target, exited_target, over_event_, false);
  }

  // Defer locating capturing enter listener until /after/ dispatching the leave
  // events because the leave handlers might set a capturing enter handler.
  bool entered_node_has_capturing_ancestor = false;
  for (wtf_size_t i = 0; i < entered_ancestors.size(); i++) {
    if (entered_ancestors[i]->HasCapturingEventListeners(enter_event_)) {
      entered_node_has_capturing_ancestor = true;
      break;
    }
  }

  // Dispatch enter events, in parent-to-child order.
  for (wtf_size_t i = entered_ancestors_common_parent_index; i > 0; i--) {
    Dispatch(entered_ancestors[i - 1], exited_target, enter_event_,
             !entered_node_has_capturing_ancestor);
  }
}

}  // namespace blink

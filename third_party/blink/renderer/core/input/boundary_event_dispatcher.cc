// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/boundary_event_dispatcher.h"

#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handling_util.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

HTMLFrameOwnerElement* FindFrameOwnerIfNested(
    const ExecutionContext& possible_outer_context,
    const ExecutionContext& possible_inner_context) {
  const auto* possible_outer_window =
      DynamicTo<LocalDOMWindow>(possible_outer_context);
  const auto* possible_inner_window =
      DynamicTo<LocalDOMWindow>(possible_inner_context);
  if (!possible_outer_window || !possible_inner_window) {
    return nullptr;
  }
  const Frame* possible_outer_frame = possible_outer_window->GetFrame();
  const Frame* possible_inner_frame = possible_inner_window->GetFrame();
  if (!possible_outer_frame || !possible_inner_frame) {
    return nullptr;
  }
  const Frame* child_frame = possible_inner_frame;
  while (child_frame && child_frame->Parent() != possible_outer_frame) {
    child_frame = child_frame->Parent();
  }
  if (!child_frame) {
    return nullptr;
  }
  return DynamicTo<HTMLFrameOwnerElement>(child_frame->Owner());
}

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

  // An EventTarget must never be exposed to a cross-origin ExecutionContext
  // through `relatedTarget` (e.g. the touch pointer stream is owned by the
  // local-root frame's PointerEventManager, which hit-tests through every
  // same-process child frame).
  // When the contexts have an inner/outer relationship in the frame hierarchy,
  // the event in the outer document uses the frame owner (e.g. iframe) element
  // that lives in that context as its relatedTarget. In other cases (e.g. for
  // the event in the inner document, or sibling/cousin frames), the related
  // target is null.
  if (RuntimeEnabledFeatures::ClearCrossOriginRelatedTargetEnabled() &&
      exited_target && entered_target) {
    const ExecutionContext* exited_context =
        exited_target->GetExecutionContext();
    const ExecutionContext* entered_context =
        entered_target->GetExecutionContext();
    bool is_same_origin = false;
    if (exited_context && entered_context) {
      if (exited_context == entered_context) {
        is_same_origin = true;
      } else {
        const SecurityOrigin* exited_origin =
            exited_context->GetSecurityOrigin();
        const SecurityOrigin* entered_origin =
            entered_context->GetSecurityOrigin();
        is_same_origin = exited_origin->CanAccess(entered_origin) &&
                         entered_origin->CanAccess(exited_origin);
      }
    }
    if (!is_same_origin) {
      EventTarget* exited_related_target =
          (exited_context && entered_context)
              ? FindFrameOwnerIfNested(*exited_context, *entered_context)
              : nullptr;
      EventTarget* entered_related_target =
          (exited_context && entered_context)
              ? FindFrameOwnerIfNested(*entered_context, *exited_context)
              : nullptr;
      SendBoundaryEventsSameOrigin(
          exited_target, original_exited_target_removed, exited_related_target);
      SendBoundaryEventsSameOrigin(entered_related_target,
                                   /*original_exited_target_removed=*/false,
                                   entered_target);
      return;
    }
  }

  SendBoundaryEventsSameOrigin(exited_target, original_exited_target_removed,
                               entered_target);
}

void BoundaryEventDispatcher::SendBoundaryEventsSameOrigin(
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

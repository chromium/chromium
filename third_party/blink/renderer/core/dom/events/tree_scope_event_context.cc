/*
 * Copyright (C) 2014 Google Inc. All Rights Reserved.
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
 *
 */

#include "third_party/blink/renderer/core/dom/events/tree_scope_event_context.h"

#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/dom/events/window_event_context.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/events/touch_event_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

bool TreeScopeEventContext::IsUnclosedTreeOf(
    const TreeScopeEventContext& other) {
  // Exclude closed nodes if necessary.
  // If a node is in a closed shadow root, or in a tree whose ancestor has a
  // closed shadow root, it should not be visible to nodes above the closed
  // shadow root.

  // (1) If |this| is an ancestor of |other| in tree-of-trees, include it.
  if (IsInclusiveAncestorOf(other))
    return true;

  // (2) If no closed shadow root in ancestors of this, include it.
  if (!ContainingClosedShadowTree())
    return true;

  // (3) If |this| is descendent of |other|, exclude if any closed shadow root
  // in between.
  if (IsDescendantOf(other))
    return !ContainingClosedShadowTree()->IsDescendantOf(other);

// (4) |this| and |other| must be in exclusive branches.
#if DCHECK_IS_ON()
  DCHECK(other.IsExclusivePartOf(*this));
#endif
  return false;
}

HeapVector<Member<EventTarget>>& TreeScopeEventContext::EnsureEventPath(
    EventPath& path) {
  if (event_path_)
    return *event_path_;

  event_path_ = MakeGarbageCollected<HeapVector<Member<EventTarget>>>();
  LocalDOMWindow* window = path.GetWindowEventContext().Window();
  event_path_->ReserveCapacity(path.size() + (window ? 1 : 0));

  for (auto& context : path.NodeEventContexts()) {
    if (context.GetTreeScopeEventContext().IsUnclosedTreeOf(*this))
      event_path_->push_back(context.GetNode());
  }
  if (window)
    event_path_->push_back(window);
  return *event_path_;
}

TouchEventContext& TreeScopeEventContext::EnsureTouchEventContext() {
  if (!touch_event_context_)
    touch_event_context_ = TouchEventContext::Create();
  return *touch_event_context_;
}

TreeScopeEventContext::TreeScopeEventContext(TreeScope& tree_scope)
    : tree_scope_(tree_scope),
      containing_closed_shadow_tree_(nullptr),
      pre_order_(-1),
      post_order_(-1) {}

void TreeScopeEventContext::Trace(Visitor* visitor) {
  visitor->Trace(tree_scope_);
  visitor->Trace(target_);
  visitor->Trace(related_target_);
  visitor->Trace(event_path_);
  visitor->Trace(touch_event_context_);
  visitor->Trace(containing_closed_shadow_tree_);
  visitor->Trace(children_);
}

int TreeScopeEventContext::CalculateTreeOrderAndSetNearestAncestorClosedTree(
    int order_number,
    TreeScopeEventContext* nearest_ancestor_closed_tree_scope_event_context) {
  pre_order_ = order_number;
  auto* shadow_root = DynamicTo<ShadowRoot>(&RootNode());
  containing_closed_shadow_tree_ =
      (shadow_root && !shadow_root->IsOpenOrV0())
          ? this
          : nearest_ancestor_closed_tree_scope_event_context;
  for (const auto& context : children_) {
    order_number = context->CalculateTreeOrderAndSetNearestAncestorClosedTree(
        order_number + 1, ContainingClosedShadowTree());
  }
  post_order_ = order_number + 1;

  return order_number + 1;
}

}  // namespace blink

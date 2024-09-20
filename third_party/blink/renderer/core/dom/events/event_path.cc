/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/events/event_path.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/window_event_context.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/events/touch_event.h"
#include "third_party/blink/renderer/core/events/touch_event_context.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/input/touch.h"
#include "third_party/blink/renderer/core/input/touch_list.h"

namespace blink {

EventTarget& EventPath::EventTargetRespectingTargetRules(Node& reference_node) {
  if (reference_node.IsPseudoElement() &&
      !reference_node.IsScrollControlPseudoElement()) {
    DCHECK(reference_node.parentNode());
    return *reference_node.parentNode();
  }

  return reference_node;
}

static inline bool ShouldStopAtShadowRoot(Event& event,
                                          ShadowRoot& shadow_root,
                                          EventTarget& target) {
  // An event is scoped by default unless event.composed flag is set.
  return !event.composed() && target.ToNode() &&
         target.ToNode()->OwnerShadowHost() == shadow_root.host();
}

EventPath::EventPath(Node& node, Event* event) : node_(node), event_(event) {
  Initialize();
}

void EventPath::InitializeWith(Node& node, Event* event) {
  node_ = &node;
  event_ = event;
  window_event_context_ = nullptr;
  node_event_contexts_.clear();
  tree_scope_event_contexts_.clear();
  Initialize();
}

static inline bool EventPathShouldBeEmptyFor(Node& node) {
  // Event path should be empty for orphaned pseudo elements, and nodes
  // whose document is stopped. In corner cases (crbug.com/1210480), the node
  // document can get detached before we can remove event listeners.
  if (RuntimeEnabledFeatures::PseudoElementsFocusableEnabled() &&
      node.IsScrollControlPseudoElement()) {
    return false;
  }
  return (node.IsPseudoElement() && !node.parentElement()) ||
         node.GetDocument().IsStopped();
}

void EventPath::Initialize() {
  if (EventPathShouldBeEmptyFor(*node_))
    return;

  CalculatePath();
  CalculateAdjustedTargets();
  CalculateTreeOrderAndSetNearestAncestorClosedTree();
}

void EventPath::CalculatePath() {
  DCHECK(node_);
  DCHECK(node_event_contexts_.empty());

  // For performance and memory usage reasons we want to store the
  // path using as few bytes as possible and with as few allocations
  // as possible which is why we gather the data on the stack before
  // storing it in a perfectly sized node_event_contexts_ Vector.
  HeapVector<Member<Node>, 64> nodes_in_path;
  Node* current = node_;

  nodes_in_path.push_back(current);
  while (current) {
    if (event_ && current->KeepEventInNode(*event_))
      break;
    if (current->IsChildOfShadowHost() && !current->IsPseudoElement()) {
      if (HTMLSlotElement* slot = current->AssignedSlot()) {
        current = slot;
        nodes_in_path.push_back(current);
        continue;
      }
    }
    if (auto* shadow_root = DynamicTo<ShadowRoot>(current)) {
      if (event_ && ShouldStopAtShadowRoot(*event_, *shadow_root, *node_))
        break;
      current = current->OwnerShadowHost();
      nodes_in_path.push_back(current);
    } else {
      current = current->parentNode();
      if (current)
        nodes_in_path.push_back(current);
    }
  }
  node_event_contexts_ = HeapVector<NodeEventContext>(
      nodes_in_path, [](Node* node_in_path) -> NodeEventContext {
        DCHECK(node_in_path);
        return NodeEventContext(
            *node_in_path, EventTargetRespectingTargetRules(*node_in_path));
      });
}

void EventPath::CalculateTreeOrderAndSetNearestAncestorClosedTree() {
  // Precondition:
  //   - TreeScopes in tree_scope_event_contexts_ must be *connected* in the
  //     same composed tree.
  //   - The root tree must be included.
  TreeScopeEventContext* root_tree = nullptr;
  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    TreeScope* parent =
        tree_scope_event_context.Get()->GetTreeScope().ParentTreeScope();
    if (!parent) {
      DCHECK(!root_tree);
      root_tree = tree_scope_event_context.Get();
      continue;
    }
    TreeScopeEventContext* parent_tree_scope_event_context =
        GetTreeScopeEventContext(*parent);
    DCHECK(parent_tree_scope_event_context);
    parent_tree_scope_event_context->AddChild(*tree_scope_event_context.Get());
  }
  DCHECK(root_tree);
  root_tree->CalculateTreeOrderAndSetNearestAncestorClosedTree(0, nullptr);
}

TreeScopeEventContext* EventPath::GetTreeScopeEventContext(
    TreeScope& tree_scope) {
  for (TreeScopeEventContext* tree_scope_event_context :
       tree_scope_event_contexts_) {
    if (tree_scope_event_context->GetTreeScope() == tree_scope) {
      return tree_scope_event_context;
    }
  }
  return nullptr;
}

TreeScopeEventContext* EventPath::EnsureTreeScopeEventContext(
    Node* current_target,
    TreeScope* tree_scope) {
  if (!tree_scope)
    return nullptr;
  TreeScopeEventContext* tree_scope_event_context =
      GetTreeScopeEventContext(*tree_scope);
  if (!tree_scope_event_context) {
    tree_scope_event_context =
        MakeGarbageCollected<TreeScopeEventContext>(*tree_scope);
    tree_scope_event_contexts_.push_back(tree_scope_event_context);

    TreeScopeEventContext* parent_tree_scope_event_context =
        EnsureTreeScopeEventContext(nullptr, tree_scope->ParentTreeScope());
    if (parent_tree_scope_event_context &&
        parent_tree_scope_event_context->Target()) {
      tree_scope_event_context->SetTarget(
          *parent_tree_scope_event_context->Target());
    } else if (current_target) {
      tree_scope_event_context->SetTarget(
          EventTargetRespectingTargetRules(*current_target));
    }
  } else if (!tree_scope_event_context->Target() && current_target) {
    tree_scope_event_context->SetTarget(
        EventTargetRespectingTargetRules(*current_target));
  }
  return tree_scope_event_context;
}

void EventPath::CalculateAdjustedTargets() {
  const TreeScope* last_tree_scope = nullptr;
  TreeScopeEventContext* last_tree_scope_event_context = nullptr;

  for (auto& context : node_event_contexts_) {
    Node& current_node = context.GetNode();
    TreeScope& current_tree_scope = current_node.GetTreeScope();
    if (last_tree_scope != &current_tree_scope) {
      last_tree_scope_event_context =
          EnsureTreeScopeEventContext(&current_node, &current_tree_scope);
    }
    DCHECK(last_tree_scope_event_context);
    context.SetTreeScopeEventContext(last_tree_scope_event_context);
    last_tree_scope = &current_tree_scope;
  }
}

void EventPath::BuildRelatedNodeMap(const Node& related_node,
                                    RelatedTargetMap& related_target_map) {
  EventPath* related_target_event_path =
      MakeGarbageCollected<EventPath>(const_cast<Node&>(related_node));
  for (const auto& tree_scope_event_context :
       related_target_event_path->tree_scope_event_contexts_) {
    related_target_map.insert(&tree_scope_event_context->GetTreeScope(),
                              tree_scope_event_context->Target());
  }
  // Oilpan: It is important to explicitly clear the vectors to reuse
  // the memory in subsequent event dispatchings.
  related_target_event_path->Clear();
}

EventTarget* EventPath::FindRelatedNode(TreeScope& scope,
                                        RelatedTargetMap& related_target_map) {
  HeapVector<Member<TreeScope>, 32> parent_tree_scopes;
  EventTarget* related_node = nullptr;
  for (TreeScope* current = &scope; current;
       current = current->ParentTreeScope()) {
    parent_tree_scopes.push_back(current);
    RelatedTargetMap::const_iterator iter = related_target_map.find(current);
    if (iter != related_target_map.end() && iter->value) {
      related_node = iter->value;
      break;
    }
  }
  DCHECK(related_node);
  for (const auto& entry : parent_tree_scopes)
    related_target_map.insert(entry, related_node);

  return related_node;
}

void EventPath::AdjustForRelatedTarget(Node& target,
                                       EventTarget* related_target) {
  if (!related_target)
    return;
  Node* related_target_node = related_target->ToNode();
  if (!related_target_node)
    return;
  if (target.GetDocument() != related_target_node->GetDocument())
    return;
  RetargetRelatedTarget(*related_target_node);
  ShrinkForRelatedTarget(target, *related_target_node);
}

void EventPath::RetargetRelatedTarget(const Node& related_target_node) {
  RelatedTargetMap related_node_map;
  BuildRelatedNodeMap(related_target_node, related_node_map);

  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    EventTarget* adjusted_related_target = FindRelatedNode(
        tree_scope_event_context->GetTreeScope(), related_node_map);
    DCHECK(adjusted_related_target);
    tree_scope_event_context.Get()->SetRelatedTarget(*adjusted_related_target);
  }
  // Explicitly clear the heap container to avoid memory regressions in the hot
  // path.
  // TODO(bikineev): Revisit after young generation is there.
  related_node_map.clear();
}

namespace {

bool ShouldStopEventPath(EventTarget& adjusted_target,
                         EventTarget& adjusted_related_target,
                         const Node& event_target_node,
                         const Node& event_related_target_node) {
  if (&adjusted_target != &adjusted_related_target)
    return false;
  Node* adjusted_target_node = adjusted_target.ToNode();
  if (!adjusted_target_node)
    return false;
  Node* adjusted_related_target_node = adjusted_related_target.ToNode();
  if (!adjusted_related_target_node)
    return false;
  // Events should be dispatched at least until its root even when event's
  // target and related_target are identical.
  if (adjusted_target_node->GetTreeScope() ==
          event_target_node.GetTreeScope() &&
      adjusted_related_target_node->GetTreeScope() ==
          event_related_target_node.GetTreeScope())
    return false;
  return true;
}

}  // anonymous namespace

void EventPath::ShrinkForRelatedTarget(const Node& event_target_node,
                                       const Node& event_related_target_node) {
  for (wtf_size_t i = 0; i < size(); ++i) {
    if (ShouldStopEventPath(*(*this)[i].Target(), *(*this)[i].RelatedTarget(),
                            event_target_node, event_related_target_node)) {
      Shrink(i);
      break;
    }
  }
}

void EventPath::AdjustForTouchEvent(const TouchEvent& touch_event) {
  // Each vector and a TouchEventContext share the same TouchList instance.
  HeapVector<Member<TouchList>> adjusted_touches;
  HeapVector<Member<TouchList>> adjusted_target_touches;
  HeapVector<Member<TouchList>> adjusted_changed_touches;
  HeapVector<Member<TreeScope>> tree_scopes;

  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    TouchEventContext& touch_event_context =
        tree_scope_event_context->EnsureTouchEventContext();
    adjusted_touches.push_back(&touch_event_context.Touches());
    adjusted_target_touches.push_back(&touch_event_context.TargetTouches());
    adjusted_changed_touches.push_back(&touch_event_context.ChangedTouches());
    tree_scopes.push_back(&tree_scope_event_context->GetTreeScope());
  }

  // AdjustTouchList appends adjusted Touch(es) to each member TouchList
  // instance in |adjusted_touch_list| argument, which is reflected on
  // TouchEventContext because they refer to the same TouchList instance.
  AdjustTouchList(touch_event.touches(), adjusted_touches, tree_scopes);
  AdjustTouchList(touch_event.targetTouches(), adjusted_target_touches,
                  tree_scopes);
  AdjustTouchList(touch_event.changedTouches(), adjusted_changed_touches,
                  tree_scopes);

#if DCHECK_IS_ON()
  for (const auto& tree_scope_event_context : tree_scope_event_contexts_) {
    TreeScope& tree_scope = tree_scope_event_context->GetTreeScope();
    TouchEventContext* touch_event_context =
        tree_scope_event_context->GetTouchEventContext();
    CheckReachability(tree_scope, touch_event_context->Touches());
    CheckReachability(tree_scope, touch_event_context->TargetTouches());
    CheckReachability(tree_scope, touch_event_context->ChangedTouches());
  }
#endif
}

void EventPath::AdjustTouchList(
    const TouchList* const touch_list,
    HeapVector<Member<TouchList>> adjusted_touch_list,
    const HeapVector<Member<TreeScope>>& tree_scopes) {
  if (!touch_list)
    return;
  for (wtf_size_t i = 0; i < touch_list->length(); ++i) {
    const Touch& touch = *touch_list->item(i);
    if (!touch.target())
      continue;

    Node* target_node = touch.target()->ToNode();
    if (!target_node)
      continue;

    RelatedTargetMap related_node_map;
    BuildRelatedNodeMap(*target_node, related_node_map);
    for (wtf_size_t j = 0; j < tree_scopes.size(); ++j) {
      adjusted_touch_list[j]->Append(touch.CloneWithNewTarget(
          FindRelatedNode(*tree_scopes[j], related_node_map)));
    }
    // Explicitly clear the heap container to avoid memory regressions in the
    // hot path.
    // TODO(bikineev): Revisit after young generation is there.
    related_node_map.clear();
  }
}

void EventPath::AdjustForDisabledFormControl() {
  for (unsigned i = 0; i < node_event_contexts_.size(); i++) {
    if (IsDisabledFormControl(&node_event_contexts_[i].GetNode())) {
      Shrink(i);
      return;
    }
  }
}

bool EventPath::DisabledFormControlExistsInPath() const {
  for (const auto& context : node_event_contexts_) {
    if (IsDisabledFormControl(&context.GetNode()))
      return true;
  }
  return false;
}

bool EventPath::HasEventListenersInPath(const AtomicString& event_type) const {
  for (const auto& context : node_event_contexts_) {
    if (context.GetNode().HasEventListeners(event_type))
      return true;
  }
  return false;
}

NodeEventContext& EventPath::TopNodeEventContext() {
  DCHECK(!IsEmpty());
  return Last();
}

void EventPath::EnsureWindowEventContext() {
  DCHECK(event_);
  if (!window_event_context_) {
    window_event_context_ = MakeGarbageCollected<WindowEventContext>(
        *event_, TopNodeEventContext());
  }
}

#if DCHECK_IS_ON()
void EventPath::CheckReachability(TreeScope& tree_scope,
                                  TouchList& touch_list) {
  for (wtf_size_t i = 0; i < touch_list.length(); ++i) {
    DCHECK(touch_list.item(i)
               ->target()
               ->ToNode()
               ->GetTreeScope()
               .IsInclusiveAncestorTreeScopeOf(tree_scope));
  }
}
#endif

void EventPath::Trace(Visitor* visitor) const {
  visitor->Trace(node_event_contexts_);
  visitor->Trace(node_);
  visitor->Trace(event_);
  visitor->Trace(tree_scope_event_contexts_);
  visitor->Trace(window_event_context_);
}

}  // namespace blink

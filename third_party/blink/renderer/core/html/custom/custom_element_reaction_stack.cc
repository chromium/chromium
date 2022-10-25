// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_queue.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

// TODO(dominicc): Consider using linked heap structures, avoiding
// finalizers, to make short-lived entries fast.

// static
const char CustomElementReactionStack::kSupplementName[] =
    "CustomElementReactionStackAgentData";

CustomElementReactionStack::CustomElementReactionStack(Agent& agent)
    : Supplement<Agent>(agent) {}

void CustomElementReactionStack::Trace(Visitor* visitor) const {
  Supplement<Agent>::Trace(visitor);
  visitor->Trace(map_);
  visitor->Trace(stack_);
  visitor->Trace(backup_queue_);
}

bool CustomElementReactionStack::IsEmpty() {
  return stack_.empty();
}

void CustomElementReactionStack::Push() {
  stack_.push_back(nullptr);
}

void CustomElementReactionStack::PopInvokingReactions() {
  ElementQueue* queue = stack_.back();
  if (queue)
    InvokeReactions(*queue);
  stack_.pop_back();
}

void CustomElementReactionStack::InvokeReactions(ElementQueue& queue) {
  for (wtf_size_t i = 0; i < queue.size(); ++i) {
    Element* element = queue[i];
    const auto it = map_.find(element);
    if (it == map_.end())
      continue;
    CustomElementReactionQueue* reactions = it->value;
    reactions->InvokeReactions(*element);
    CHECK(reactions->IsEmpty());
    map_.erase(element);
  }
}

void CustomElementReactionStack::EnqueueToCurrentQueue(
    Element& element,
    CustomElementReaction& reaction) {
  Enqueue(stack_.back(), element, reaction);
}

void CustomElementReactionStack::Enqueue(Member<ElementQueue>& queue,
                                         Element& element,
                                         CustomElementReaction& reaction) {
  if (!queue)
    queue = MakeGarbageCollected<ElementQueue>();
  queue->push_back(&element);

  const auto it = map_.find(&element);
  if (it != map_.end()) {
    it->value->Add(reaction);
  } else {
    CustomElementReactionQueue* reactions =
        MakeGarbageCollected<CustomElementReactionQueue>();
    map_.insert(&element, reactions);
    reactions->Add(reaction);
  }
}

void CustomElementReactionStack::EnqueueToBackupQueue(
    Element& element,
    CustomElementReaction& reaction) {
  // https://html.spec.whatwg.org/C/#backup-element-queue

  DCHECK(stack_.empty());
  DCHECK(IsMainThread());

  // If the processing the backup element queue is not set:
  if (!backup_queue_ || backup_queue_->empty()) {
    element.GetDocument().GetAgent().event_loop()->EnqueueMicrotask(
        WTF::BindOnce(&CustomElementReactionStack::InvokeBackupQueue,
                      Persistent<CustomElementReactionStack>(this)));
  }

  Enqueue(backup_queue_, element, reaction);
}

void CustomElementReactionStack::ClearQueue(Element& element) {
  const auto it = map_.find(&element);
  if (it != map_.end())
    it->value->Clear();
}

void CustomElementReactionStack::InvokeBackupQueue() {
  DCHECK(IsMainThread());
  InvokeReactions(*backup_queue_);
  backup_queue_->clear();
}

CustomElementReactionStack& CustomElementReactionStack::From(Agent& agent) {
  CustomElementReactionStack* supplement =
      Supplement<Agent>::From<CustomElementReactionStack>(agent);
  if (!supplement) {
    supplement = MakeGarbageCollected<CustomElementReactionStack>(agent);
    ProvideTo(agent, supplement);
  }
  return *supplement;
}

CustomElementReactionStack* CustomElementReactionStack::Swap(
    Agent& agent,
    CustomElementReactionStack* new_stack) {
  CustomElementReactionStack* old_stack =
      &CustomElementReactionStack::From(agent);
  CustomElementReactionStack::ProvideTo(agent, new_stack);
  return old_stack;
}

}  // namespace blink

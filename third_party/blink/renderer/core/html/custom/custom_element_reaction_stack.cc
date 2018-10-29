// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_queue.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

Persistent<CustomElementReactionStack>& GetCustomElementReactionStack() {
  DEFINE_STATIC_LOCAL(Persistent<CustomElementReactionStack>,
                      custom_element_reaction_stack,
                      (new CustomElementReactionStack));
  return custom_element_reaction_stack;
}

}  // namespace

// TODO(dominicc): Consider using linked heap structures, avoiding
// finalizers, to make short-lived entries fast.

CustomElementReactionStack::CustomElementReactionStack() = default;

void CustomElementReactionStack::Trace(blink::Visitor* visitor) {
  visitor->Trace(map_);
  visitor->Trace(stack_);
  visitor->Trace(backup_queue_);
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
    if (CustomElementReactionQueue* reactions = map_.at(element)) {
      reactions->InvokeReactions(element);
      CHECK(reactions->IsEmpty());
      map_.erase(element);
    }
  }
}

void CustomElementReactionStack::EnqueueToCurrentQueue(
    Element* element,
    CustomElementReaction* reaction) {
  Enqueue(stack_.back(), element, reaction);
}

void CustomElementReactionStack::Enqueue(Member<ElementQueue>& queue,
                                         Element* element,
                                         CustomElementReaction* reaction) {
  if (!queue)
    queue = new ElementQueue();
  queue->push_back(element);

  CustomElementReactionQueue* reactions = map_.at(element);
  if (!reactions) {
    reactions = new CustomElementReactionQueue();
    map_.insert(element, reactions);
  }

  reactions->Add(reaction);
}

void CustomElementReactionStack::EnqueueToBackupQueue(
    Element* element,
    CustomElementReaction* reaction) {
  // https://html.spec.whatwg.org/multipage/scripting.html#backup-element-queue

  DCHECK(!CEReactionsScope::Current());
  DCHECK(stack_.IsEmpty());
  DCHECK(IsMainThread());

  // If the processing the backup element queue is not set:
  if (!backup_queue_ || backup_queue_->IsEmpty()) {
    Microtask::EnqueueMicrotask(
        WTF::Bind(&CustomElementReactionStack::InvokeBackupQueue,
                  Persistent<CustomElementReactionStack>(this)));
  }

  Enqueue(backup_queue_, element, reaction);
}

void CustomElementReactionStack::ClearQueue(Element* element) {
  if (CustomElementReactionQueue* reactions = map_.at(element))
    reactions->Clear();
}

void CustomElementReactionStack::InvokeBackupQueue() {
  DCHECK(IsMainThread());
  InvokeReactions(*backup_queue_);
  backup_queue_->clear();
}

CustomElementReactionStack& CustomElementReactionStack::Current() {
  return *GetCustomElementReactionStack();
}

CustomElementReactionStack*
CustomElementReactionStackTestSupport::SetCurrentForTest(
    CustomElementReactionStack* new_stack) {
  Persistent<CustomElementReactionStack>& stack =
      GetCustomElementReactionStack();
  CustomElementReactionStack* old_stack = stack.Get();
  stack = new_stack;
  return old_stack;
}

}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

namespace blink {

CEReactionsScope* CEReactionsScope::top_of_stack_ = nullptr;

// static
CEReactionsScope* CEReactionsScope::Current() {
  DCHECK(IsMainThread());
  return top_of_stack_;
}

CEReactionsScope::CEReactionsScope() : prev_(top_of_stack_) {
  // For speed of the bindings we use a global variable to determine if
  // we have a CEReactionScope. We check that this is only on the main thread
  // otherwise this global variable will have collisions.
  DCHECK(IsMainThread());
  top_of_stack_ = this;
}

CEReactionsScope::~CEReactionsScope() {
  if (stack_)
    stack_->PopInvokingReactions();
  top_of_stack_ = top_of_stack_->prev_;
}

void CEReactionsScope::EnqueueToCurrentQueue(CustomElementReactionStack& stack,
                                             Element& element,
                                             CustomElementReaction& reaction) {
  if (!stack_)
    stack.Push();
  stack_ = &stack;
  stack.EnqueueToCurrentQueue(element, reaction);
}

}  // namespace blink

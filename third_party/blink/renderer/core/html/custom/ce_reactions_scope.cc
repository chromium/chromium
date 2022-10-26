// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

namespace blink {

CEReactionsScope* CEReactionsScope::top_of_stack_ = nullptr;

CEReactionsScope::CEReactionsScope(ExecutionContext* execution_context)
    : prev_(top_of_stack_), work_to_do_(false) {
  top_of_stack_ = this;
}

CEReactionsScope::CEReactionsScope(Agent& agent)
    : prev_(top_of_stack_), work_to_do_(false) {
  top_of_stack_ = this;
}

CEReactionsScope::~CEReactionsScope() {
  if (work_to_do_)
    InvokeReactions();
  top_of_stack_ = top_of_stack_->prev_;
}

void CEReactionsScope::EnqueueToCurrentQueue(Element& element,
                                             CustomElementReaction& reaction) {
  if (!work_to_do_) {
    work_to_do_ = true;
    CustomElementReactionStack::Current().Push();
  }
  CustomElementReactionStack::Current().EnqueueToCurrentQueue(element,
                                                              reaction);
}

void CEReactionsScope::InvokeReactions() {
  CustomElementReactionStack::Current().PopInvokingReactions();
}

}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

namespace blink {

CEReactionsScope::CEReactionsScope(ExecutionContext* execution_context)
    : CEReactionsScope(*execution_context->GetAgent()) {}

CEReactionsScope::CEReactionsScope(Agent& agent)
    : stack_(CustomElementReactionStack::From(agent)) {
  stack_.Push();
}

CEReactionsScope::~CEReactionsScope() {
  stack_.PopInvokingReactions();
}

void CEReactionsScope::EnqueueToCurrentQueue(Element& element,
                                             CustomElementReaction& reaction) {
  stack_.EnqueueToCurrentQueue(element, reaction);
}

}  // namespace blink

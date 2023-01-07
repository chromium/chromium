// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

namespace blink {

CEReactionsScope* CEReactionsScope::top_of_stack_ = nullptr;

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

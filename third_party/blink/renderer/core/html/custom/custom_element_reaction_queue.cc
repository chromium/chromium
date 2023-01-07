// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_queue.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

CustomElementReactionQueue::CustomElementReactionQueue() : index_(0u) {}

CustomElementReactionQueue::~CustomElementReactionQueue() = default;

void CustomElementReactionQueue::Trace(Visitor* visitor) const {
  visitor->Trace(reactions_);
}

void CustomElementReactionQueue::Add(CustomElementReaction& reaction) {
  reactions_.push_back(&reaction);
}

// There is one queue per element, so this could be invoked
// recursively.
void CustomElementReactionQueue::InvokeReactions(Element& element) {
  TRACE_EVENT1("blink", "CustomElementReactionQueue::invokeReactions", "name",
               element.localName().Utf8());
  while (index_ < reactions_.size()) {
    CustomElementReaction* reaction = reactions_[index_];
    reactions_[index_++] = nullptr;
    reaction->Invoke(element);
  }
  // Reactions are always inserted by steps which bump the global element queue.
  // This means we do not need queue "owner" guards.
  // https://html.spec.whatwg.org/C/#custom-element-reactions
  Clear();
}

void CustomElementReactionQueue::Clear() {
  index_ = 0;
  reactions_.resize(0);
}

}  // namespace blink

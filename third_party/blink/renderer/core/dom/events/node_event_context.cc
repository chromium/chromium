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

#include "third_party/blink/renderer/core/dom/events/node_event_context.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/focus_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/events/touch_event_context.h"
#include "third_party/blink/renderer/core/input/touch_list.h"

namespace blink {

NodeEventContext::NodeEventContext(Node& node, EventTarget& current_target)
    : node_(node, Member<Node>::AtomicInitializerTag{}),
      current_target_(current_target,
                      Member<EventTarget>::AtomicInitializerTag{}),
      tree_scope_event_context_(
          nullptr,
          Member<TreeScopeEventContext>::AtomicInitializerTag{}) {}

void NodeEventContext::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  visitor->Trace(current_target_);
  visitor->Trace(tree_scope_event_context_);
}

void NodeEventContext::HandleLocalEvents(Event& event) const {
  if (TouchEventContext* touch_context = GetTouchEventContext()) {
    touch_context->HandleLocalEvents(event);
  } else if (RelatedTarget()) {
    event.SetRelatedTargetIfExists(RelatedTarget());
  }
  event.SetTarget(Target());
  event.SetCurrentTarget(current_target_.Get());
  node_->HandleLocalEvents(event);
}

}  // namespace blink

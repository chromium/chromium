// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/popover_animation_finished_event_listener.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

PopoverAnimationFinishedEventListener::PopoverAnimationFinishedEventListener(
    Member<HTMLElement> popover_element,
    HeapHashSet<Member<EventTarget>>&& animations)
    : popover_element_(popover_element), animations_(std::move(animations)) {
  DCHECK(popover_element->HasPopoverAttribute());
  DCHECK(!animations_.empty());
  for (auto animation : animations_) {
    animation->addEventListener(event_type_names::kFinish, this,
                                /*use_capture*/ false);
    animation->addEventListener(event_type_names::kCancel, this,
                                /*use_capture*/ false);
  }
}

PopoverAnimationFinishedEventListener::
    ~PopoverAnimationFinishedEventListener() = default;

void PopoverAnimationFinishedEventListener::Dispose() {
  // Event listeners may already have been cleaned up by
  // LocalDOMWindow::RemoveAllEventListeners().
  if (!popover_element_->GetDocument().GetFrame())
    return;
  for (const auto& animation : animations_) {
    RemoveEventListeners(animation);
  }
  animations_.clear();
}

void PopoverAnimationFinishedEventListener::RemoveEventListeners(
    EventTarget* animation) const {
  animation->removeEventListener(event_type_names::kFinish, this,
                                 /*use_capture*/ false);
  animation->removeEventListener(event_type_names::kCancel, this,
                                 /*use_capture*/ false);
}

void PopoverAnimationFinishedEventListener::Invoke(ExecutionContext*,
                                                   Event* event) {
  DCHECK(!animations_.empty());
  DCHECK(event->type() == event_type_names::kFinish ||
         event->type() == event_type_names::kCancel);

  if (!event->isTrusted())
    return;

  auto* animation = event->target();
  RemoveEventListeners(animation);
  animations_.erase(animation);

  // Finish hiding the popover once all animations complete.
  if (animations_.empty()) {
    popover_element_->PopoverHideFinishIfNeeded();
  }
}

bool PopoverAnimationFinishedEventListener::IsFinished() const {
  return animations_.empty();
}

void PopoverAnimationFinishedEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(popover_element_);
  visitor->Trace(animations_);
  NativeEventListener::Trace(visitor);
}

}  // namespace blink

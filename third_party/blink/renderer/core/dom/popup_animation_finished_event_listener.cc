// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/popup_animation_finished_event_listener.h"

#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"

namespace blink {

PopupAnimationFinishedEventListener::PopupAnimationFinishedEventListener(
    Member<Element> popup_element,
    HeapHashSet<Member<EventTarget>>&& animations)
    : popup_element_(popup_element), animations_(std::move(animations)) {
  DCHECK(popup_element->HasPopupAttribute());
  DCHECK(!animations_.IsEmpty());
  for (auto animation : animations_) {
    animation->addEventListener(event_type_names::kFinish, this,
                                /*use_capture*/ false);
    animation->addEventListener(event_type_names::kCancel, this,
                                /*use_capture*/ false);
  }
}

PopupAnimationFinishedEventListener::~PopupAnimationFinishedEventListener() =
    default;

void PopupAnimationFinishedEventListener::Dispose() {
  // Event listeners may already have been cleaned up by
  // LocalDOMWindow::RemoveAllEventListeners().
  if (!popup_element_->GetDocument().GetFrame())
    return;
  for (const auto& animation : animations_) {
    RemoveEventListeners(animation);
  }
  animations_.clear();
}

void PopupAnimationFinishedEventListener::RemoveEventListeners(
    EventTarget* animation) const {
  animation->removeEventListener(event_type_names::kFinish, this,
                                 /*use_capture*/ false);
  animation->removeEventListener(event_type_names::kCancel, this,
                                 /*use_capture*/ false);
}

void PopupAnimationFinishedEventListener::Invoke(ExecutionContext*,
                                                 Event* event) {
  DCHECK(!animations_.IsEmpty());
  DCHECK(event->type() == event_type_names::kFinish ||
         event->type() == event_type_names::kCancel);
  auto* animation = event->target();
  RemoveEventListeners(animation);
  animations_.erase(animation);

  // Finish hiding the popup once all animations complete.
  if (animations_.IsEmpty()) {
    popup_element_->PopupHideFinishIfNeeded();
  }
}

bool PopupAnimationFinishedEventListener::IsFinished() const {
  return animations_.IsEmpty();
}

void PopupAnimationFinishedEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(popup_element_);
  visitor->Trace(animations_);
  NativeEventListener::Trace(visitor);
}

}  // namespace blink

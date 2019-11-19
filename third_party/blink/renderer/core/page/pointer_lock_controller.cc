/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/page/pointer_lock_controller.h"

#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/pointer_lock_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

PointerLockController::PointerLockController(Page* page)
    : page_(page), lock_pending_(false) {}

void PointerLockController::RequestPointerLock(
    Element* target,
    const PointerLockOptions* options) {
  if (!target || !target->isConnected() ||
      document_of_removed_element_while_waiting_for_unlock_) {
    EnqueueEvent(event_type_names::kPointerlockerror, target);
    return;
  }

  target->GetDocument().CountUseOnlyInCrossOriginIframe(
      WebFeature::kElementRequestPointerLockIframe);
  if (target->IsInShadowTree()) {
    UseCounter::Count(target->GetDocument(),
                      WebFeature::kElementRequestPointerLockInShadow);
  }
  if (options && options->unadjustedMovement()) {
    UseCounter::Count(target->GetDocument(),
                      WebFeature::kPointerLockUnadjustedMovement);
  }

  if (target->GetDocument().IsSandboxed(WebSandboxFlags::kPointerLock)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    target->GetDocument().AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Blocked pointer lock on an element because the element's frame is "
        "sandboxed and the 'allow-pointer-lock' permission is not set."));
    EnqueueEvent(event_type_names::kPointerlockerror, target);
    return;
  }

  if (element_) {
    if (element_->GetDocument() != target->GetDocument()) {
      EnqueueEvent(event_type_names::kPointerlockerror, target);
      return;
    }
    EnqueueEvent(event_type_names::kPointerlockchange, target);
    element_ = target;
  } else if (page_->GetChromeClient().RequestPointerLock(
                 target->GetDocument().GetFrame(),
                 (options ? options->unadjustedMovement() : false))) {
    lock_pending_ = true;
    element_ = target;
  } else {
    EnqueueEvent(event_type_names::kPointerlockerror, target);
  }
}

void PointerLockController::RequestPointerUnlock() {
  return page_->GetChromeClient().RequestPointerUnlock(
      element_->GetDocument().GetFrame());
}

void PointerLockController::ElementRemoved(Element* element) {
  if (element_ == element) {
    document_of_removed_element_while_waiting_for_unlock_ =
        &element_->GetDocument();
    RequestPointerUnlock();
    // Set element null immediately to block any future interaction with it
    // including mouse events received before the unlock completes.
    ClearElement();
  }
}

void PointerLockController::DocumentDetached(Document* document) {
  if (element_ && element_->GetDocument() == document) {
    RequestPointerUnlock();
    ClearElement();
  }
}

bool PointerLockController::LockPending() const {
  return lock_pending_;
}

Element* PointerLockController::GetElement() const {
  return element_.Get();
}

void PointerLockController::DidAcquirePointerLock() {
  EnqueueEvent(event_type_names::kPointerlockchange, element_.Get());
  lock_pending_ = false;
  if (element_) {
    LocalFrame* frame = element_->GetDocument().GetFrame();
    pointer_lock_position_ = frame->LocalFrameRoot()
                                 .GetEventHandler()
                                 .LastKnownMousePositionInRootFrame();
    pointer_lock_screen_position_ = frame->LocalFrameRoot()
                                        .GetEventHandler()
                                        .LastKnownMouseScreenPosition();
  }
}

void PointerLockController::DidNotAcquirePointerLock() {
  EnqueueEvent(event_type_names::kPointerlockerror, element_.Get());
  ClearElement();
}

void PointerLockController::DidLosePointerLock() {
  Document* pointer_lock_document =
      element_ ? &element_->GetDocument()
               : document_of_removed_element_while_waiting_for_unlock_.Get();
  EnqueueEvent(event_type_names::kPointerlockchange, pointer_lock_document);

  // Set the last mouse position back the locked position.
  if (pointer_lock_document && pointer_lock_document->GetFrame()) {
    pointer_lock_document->GetFrame()
        ->GetEventHandler()
        .ResetMousePositionForPointerUnlock();
  }

  ClearElement();
  document_of_removed_element_while_waiting_for_unlock_ = nullptr;
}

void PointerLockController::DispatchLockedMouseEvent(
    const WebMouseEvent& event,
    const Vector<WebMouseEvent>& coalesced_events,
    const Vector<WebMouseEvent>& predicted_events,
    const AtomicString& event_type) {
  if (!element_ || !element_->GetDocument().GetFrame())
    return;

  if (LocalFrame* frame = element_->GetDocument().GetFrame()) {
    frame->GetEventHandler().HandleTargetedMouseEvent(
        element_, event, event_type, coalesced_events, predicted_events);

    // Event handlers may remove element.
    if (!element_)
      return;

    // Create click events
    if (event_type == event_type_names::kMouseup) {
      frame->GetEventHandler().HandleTargetedMouseEvent(
          element_, event, event_type_names::kClick, Vector<WebMouseEvent>(),
          Vector<WebMouseEvent>());
    }
  }
}

void PointerLockController::GetPointerLockPosition(
    FloatPoint* lock_position,
    FloatPoint* lock_screen_position) {
  if (element_ && !lock_pending_) {
    DCHECK(lock_position);
    DCHECK(lock_screen_position);
    *lock_position = pointer_lock_position_;
    *lock_screen_position = pointer_lock_screen_position_;
  }
}

void PointerLockController::ClearElement() {
  lock_pending_ = false;
  element_ = nullptr;
}

void PointerLockController::EnqueueEvent(const AtomicString& type,
                                         Element* element) {
  if (element)
    EnqueueEvent(type, &element->GetDocument());
}

void PointerLockController::EnqueueEvent(const AtomicString& type,
                                         Document* document) {
  if (document && document->domWindow()) {
    document->domWindow()->EnqueueDocumentEvent(*Event::Create(type),
                                                TaskType::kMiscPlatformAPI);
  }
}

void PointerLockController::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(element_);
  visitor->Trace(document_of_removed_element_while_waiting_for_unlock_);
}

}  // namespace blink

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

#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pointer_lock_options.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

PointerLockController::PointerLockController(Page* page)
    : page_(page), lock_pending_(false) {}

bool PointerLockController::RequestPointerLock(Element* target,
                                               ResultCallback callback) {
  if (!target || !target->isConnected() ||
      document_of_removed_element_while_waiting_for_unlock_ || element_) {
    return false;
  }
  LocalDOMWindow* window = To<LocalDOMWindow>(target->GetExecutionContext());
  window->GetFrame()->GetWidgetForLocalRoot()->RequestMouseLock(
      LocalFrame::HasTransientUserActivation(window->GetFrame()),
      /*unadjusted_movement_requested=*/false,
      WTF::BindOnce(&PointerLockController::LockRequestCallback,
                    WrapWeakPersistent(this), std::move(callback),
                    /*unadjusted_movement_requested=*/false));
  lock_pending_ = true;
  element_ = target;
  return true;
}

void PointerLockController::RequestPointerLock(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    Element* target,
    const PointerLockOptions* options) {
  if (!target || !target->isConnected() ||
      document_of_removed_element_while_waiting_for_unlock_) {
    EnqueueEvent(event_type_names::kPointerlockerror, target);
    resolver->RejectWithDOMException(DOMExceptionCode::kWrongDocumentError,
                                     "Target Element removed from DOM");
    return;
  }

  LocalDOMWindow* window = To<LocalDOMWindow>(target->GetExecutionContext());
  window->CountUseOnlyInCrossOriginIframe(
      WebFeature::kElementRequestPointerLockIframe);
  if (target->IsInShadowTree()) {
    UseCounter::Count(window, WebFeature::kElementRequestPointerLockInShadow);
  }
  if (options && options->unadjustedMovement()) {
    UseCounter::Count(window, WebFeature::kPointerLockUnadjustedMovement);
  }

  if (window->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kPointerLock)) {
    // FIXME: This message should be moved off the console once a solution to
    // https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
    if (!window->GetFrame()->IsInFencedFrameTree()) {
      window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError,
          "Blocked pointer lock on an element because the element's frame is "
          "sandboxed and the 'allow-pointer-lock' permission is not set."));
    }
    EnqueueEvent(event_type_names::kPointerlockerror, target);
    resolver->RejectWithSecurityError(
        window->GetFrame()->IsInFencedFrameTree()
            ? "Blocked pointer lock on an element because the element is "
              "contained "
              "in a fence frame tree."
            : "Blocked pointer lock on an element because the element's frame "
              "is "
              "sandboxed and the 'allow-pointer-lock' permission is not set.",
        "");
    return;
  }

  bool unadjusted_movement_requested =
      options ? options->unadjustedMovement() : false;
  if (element_) {
    if (element_->GetDocument() != target->GetDocument()) {
      EnqueueEvent(event_type_names::kPointerlockerror, target);
      resolver->RejectWithDOMException(
          DOMExceptionCode::kWrongDocumentError,
          "The new element is not in the same shadow-root document as the "
          "element that currently holds the lock.");
      return;
    }
    // Attempt to change options if necessary.
    if (unadjusted_movement_requested != current_unadjusted_movement_setting_) {
      if (!mouse_lock_context_.is_bound() || lock_pending_) {
        EnqueueEvent(event_type_names::kPointerlockerror, target);
        resolver->RejectWithDOMException(DOMExceptionCode::kInUseAttributeError,
                                         "Pointer lock pending.");
        return;
      }

      mouse_lock_context_->RequestMouseLockChange(
          unadjusted_movement_requested,
          WTF::BindOnce(
              &PointerLockController::ChangeLockRequestCallback,
              WrapWeakPersistent(this), WrapWeakPersistent(target),
              WTF::BindOnce(&PointerLockController::ProcessResultPromise,
                            WrapPersistent(resolver)),
              unadjusted_movement_requested));
      return;
    }

    EnqueueEvent(event_type_names::kPointerlockchange, target);
    element_ = target;
    resolver->Resolve();

    // Subsequent steps are handled in the browser process.
  } else {
    window->GetFrame()->GetWidgetForLocalRoot()->RequestMouseLock(
        LocalFrame::HasTransientUserActivation(window->GetFrame()),
        unadjusted_movement_requested,
        WTF::BindOnce(
            &PointerLockController::LockRequestCallback,
            WrapWeakPersistent(this),
            WTF::BindOnce(&PointerLockController::ProcessResultPromise,
                          WrapPersistent(resolver)),
            unadjusted_movement_requested));
    lock_pending_ = true;
    element_ = target;
  }
}

void PointerLockController::ChangeLockRequestCallback(
    Element* target,
    ResultCallback callback,
    bool unadjusted_movement_requested,
    mojom::blink::PointerLockResult result) {
  if (result == mojom::blink::PointerLockResult::kSuccess)
    element_ = target;

  ProcessResult(std::move(callback), unadjusted_movement_requested, result);
}

void PointerLockController::LockRequestCallback(
    ResultCallback callback,
    bool unadjusted_movement_requested,
    mojom::blink::PointerLockResult result,
    mojo::PendingRemote<blink::mojom::blink::PointerLockContext> context) {
  if (element_ && context) {
    mouse_lock_context_.Bind(std::move(context),
                             element_->GetExecutionContext()->GetTaskRunner(
                                 TaskType::kUserInteraction));
    // The browser might unlock the mouse for many reasons including closing
    // the tab, the user hitting esc, the page losing focus, and more.
    mouse_lock_context_.set_disconnect_handler(WTF::BindOnce(
        &PointerLockController::ExitPointerLock, WrapWeakPersistent(this)));
  }
  ProcessResult(std::move(callback), unadjusted_movement_requested, result);
  if (result == mojom::blink::PointerLockResult::kSuccess) {
    DidAcquirePointerLock();
  } else {
    DidNotAcquirePointerLock();
  }
}

void PointerLockController::ProcessResultPromise(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    mojom::blink::PointerLockResult result) {
  if (result == mojom::blink::PointerLockResult::kSuccess) {
    resolver->Resolve();
    return;
  }
  DOMException* exception = ConvertResultToException(result);
  resolver->Reject(exception);
}

void PointerLockController::ProcessResult(
    ResultCallback callback,
    bool unadjusted_movement_requested,
    mojom::blink::PointerLockResult result) {
  if (result == mojom::blink::PointerLockResult::kSuccess)
    current_unadjusted_movement_setting_ = unadjusted_movement_requested;
  std::move(callback).Run(result);
}

DOMException* PointerLockController::ConvertResultToException(
    mojom::blink::PointerLockResult result) {
  switch (result) {
    case mojom::blink::PointerLockResult::kUnsupportedOptions:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError,
          "The options asked for in this request are not supported on this "
          "platform.");
    case mojom::blink::PointerLockResult::kRequiresUserGesture:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotAllowedError,
          "A user gesture is required to request Pointer Lock.");
    case mojom::blink::PointerLockResult::kAlreadyLocked:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInUseAttributeError, "Pointer is already locked.");
    case mojom::blink::PointerLockResult::kWrongDocument:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kWrongDocumentError,
          "The root document of this element is not valid for pointer lock.");
    case mojom::blink::PointerLockResult::kPermissionDenied:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The root document of this element is not valid for pointer lock.");
    case mojom::blink::PointerLockResult::kElementDestroyed:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kWrongDocumentError,
          "The element has been destroyed while making this request.");
    case mojom::blink::PointerLockResult::kUserRejected:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kSecurityError,
          "The user has exited the lock before this request was completed.");
    case mojom::blink::PointerLockResult::kSuccess:
    case mojom::blink::PointerLockResult::kUnknownError:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError,
          "If you see this error we have a bug. Please report this bug to "
          "chromium.");
  }
}

void PointerLockController::ExitPointerLock() {
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
  mouse_lock_context_.reset();
}

void PointerLockController::ElementRemoved(Element* element) {
  if (element_ == element) {
    document_of_removed_element_while_waiting_for_unlock_ =
        &element_->GetDocument();
    ExitPointerLock();
    // Set element null immediately to block any future interaction with it
    // including mouse events received before the unlock completes.
    ClearElement();
  }
}

void PointerLockController::DocumentDetached(Document* document) {
  if (element_ && element_->GetDocument() == document) {
    ExitPointerLock();
    ClearElement();
  }
}

bool PointerLockController::LockPending() const {
  return lock_pending_;
}

bool PointerLockController::IsPointerLocked() const {
  return mouse_lock_context_.is_bound();
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
    LocalFrame* focused_frame =
        frame->GetPage()->GetFocusController().FocusedFrame();
    if (focused_frame) {
      focused_frame->GetEventHandler().ReleaseMousePointerCapture();
    }

    // Mouse Lock removes the system cursor and provides all mouse motion as
    // .movementX/Y values on events all sent to a fixed target. This requires
    // content to specifically request the mode to be entered.
    // Mouse Capture is implicitly given for the duration of a drag event, and
    // sends all mouse events to the initial target of the drag.
    // If Lock is entered it supersedes any in progress Capture.
    frame->GetWidgetForLocalRoot()->MouseCaptureLost();
  }
}

void PointerLockController::DidNotAcquirePointerLock() {
  EnqueueEvent(event_type_names::kPointerlockerror, element_.Get());
  ClearElement();
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
    gfx::PointF* lock_position,
    gfx::PointF* lock_screen_position) {
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
                                                TaskType::kUserInteraction);
  }
}

void PointerLockController::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(element_);
  visitor->Trace(document_of_removed_element_while_waiting_for_unlock_);
  visitor->Trace(mouse_lock_context_);
}

// static
Element* PointerLockController::GetPointerLockedElement(LocalFrame* frame) {
  if (Page* p = frame->GetPage()) {
    if (!p->GetPointerLockController().LockPending())
      return p->GetPointerLockController().GetElement();
  }
  return nullptr;
}

}  // namespace blink

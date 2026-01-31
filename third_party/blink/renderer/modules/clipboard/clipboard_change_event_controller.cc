// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_change_event_controller.h"

#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_change_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ClipboardChangeEventController::ClipboardChangeEventController(
    Navigator& navigator,
    EventTarget* event_target)
    : Supplement<Navigator>(navigator),
      PlatformEventController(*navigator.DomWindow()),
      FocusChangedObserver(navigator.DomWindow()->GetFrame()->GetPage()),
      event_target_(event_target) {}

void ClipboardChangeEventController::FocusedFrameChanged() {
  if (fire_clipboardchange_on_focus_) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kClipboardChangeEventFiredAfterFocusGain);
    fire_clipboardchange_on_focus_ = false;
    MaybeDispatchClipboardChangeEvent();
  }
}

ExecutionContext* ClipboardChangeEventController::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

void ClipboardChangeEventController::DidUpdateData() {
  OnClipboardChanged();
}

bool ClipboardChangeEventController::HasLastData() {
  return true;
}

void ClipboardChangeEventController::RegisterWithDispatcher() {
  SystemClipboard* clipboard = GetSystemClipboard();
  if (clipboard) {
    clipboard->AddController(this, GetSupplementable()->DomWindow());
  }
}

void ClipboardChangeEventController::UnregisterWithDispatcher() {
  SystemClipboard* clipboard = GetSystemClipboard();
  if (clipboard) {
    clipboard->RemoveController(this);
  }
}

SystemClipboard* ClipboardChangeEventController::GetSystemClipboard() const {
  ExecutionContext* context = GetExecutionContext();
  LocalFrame* local_frame = To<LocalDOMWindow>(context)->GetFrame();
  return local_frame->GetSystemClipboard();
}

void ClipboardChangeEventController::Trace(Visitor* visitor) const {
  Supplement<Navigator>::Trace(visitor);
  PlatformEventController::Trace(visitor);
  FocusChangedObserver::Trace(visitor);
  visitor->Trace(event_target_);
}

void ClipboardChangeEventController::OnClipboardChanged() {
  ExecutionContext* context = GetExecutionContext();
  // TODO(roraja): revisit if this null check is really required
  if (!context) {
    return;
  }
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  CHECK(window.IsSecureContext());  // [SecureContext] in IDL

  MaybeDispatchClipboardChangeEvent();
}

void ClipboardChangeEventController::OnPermissionResult(
    mojom::blink::PermissionStatus status) {
  if (status == mojom::blink::PermissionStatus::GRANTED) {
    // Note: There's a benign race condition where if the clipboard changes
    // again while waiting for permission, and the window gains sticky
    // activation, two events may fire (one from activation, one from this
    // callback). This is acceptable because:
    // 1. Both events are valid (clipboard changed + user has access)
    // 2. The race window is very small in practice
    // 3. Apps already handle multiple clipboard change events
    // 4. Event data is fetched on-demand, so no stale types/changeID
    DispatchClipboardChangeEvent();
  }
}

void ClipboardChangeEventController::MaybeDispatchClipboardChangeEvent() {
  ExecutionContext* context = GetExecutionContext();
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);

  // Check if document has focus
  if (!window.document()->hasFocus()) {
    // Schedule a clipboardchange event when the page regains focus
    fire_clipboardchange_on_focus_ = true;
    return;
  }

  fire_clipboardchange_on_focus_ = false;

  // Check for sticky activation first
  LocalFrame* frame = window.GetFrame();
  if (frame->HasStickyUserActivation()) {
    DispatchClipboardChangeEvent();
    return;
  }

  // No sticky activation - check clipboard-read permission
  auto* permission_service = window.document()->GetPermissionService(context);
  if (!permission_service) {
    return;
  }

  auto permission_descriptor = mojom::blink::PermissionDescriptor::New();
  permission_descriptor->name = mojom::blink::PermissionName::CLIPBOARD_READ;

  permission_service->HasPermission(
      std::move(permission_descriptor),
      BindOnce(&ClipboardChangeEventController::OnPermissionResult,
               WrapWeakPersistent(this)));
}

void ClipboardChangeEventController::DispatchClipboardChangeEvent() {
  SystemClipboard* clipboard = GetSystemClipboard();
  // TODO(roraja): revisit if this null check
  if (!clipboard) {
    return;
  }
  const auto& clipboardchange_data = clipboard->GetClipboardChangeEventData();
  // This notification should never be received if the data is not
  // available.
  event_target_->DispatchEvent(*ClipboardChangeEvent::Create(
      clipboardchange_data.types, clipboardchange_data.change_id));
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kClipboardChangeEventFired);
}

}  // namespace blink

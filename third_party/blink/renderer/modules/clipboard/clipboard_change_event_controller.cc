// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_change_event_controller.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

namespace blink {

ClipboardChangeEventController::ClipboardChangeEventController(
    Navigator& navigator,
    EventTarget* event_target)
    : Supplement<Navigator>(navigator),
      PlatformEventController(*navigator.DomWindow()),
      FocusChangedObserver(navigator.DomWindow()->GetFrame()->GetPage()),
      event_target_(event_target) {}

void ClipboardChangeEventController::FocusedFrameChanged() {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    return;
  }
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  if (window.document()->hasFocus()) {
    if (fire_clipboardchange_on_focus_) {
      OnClipboardChanged();
    }
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
  if (!context) {
    return nullptr;
  }
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
  if (!context) {
    return;
  }
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  CHECK(window.IsSecureContext());  // [SecureContext] in IDL

  if (window.document()->hasFocus()) {
    fire_clipboardchange_on_focus_ = false;
    if (event_target_) {
      event_target_->DispatchEvent(
          *Event::Create(event_type_names::kClipboardchange));
    }
  } else {
    // Schedule a clipboardchange event when the page regains focus
    fire_clipboardchange_on_focus_ = true;
  }
}

}  // namespace blink

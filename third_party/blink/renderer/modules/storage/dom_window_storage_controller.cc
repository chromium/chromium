// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/dom_window_storage_controller.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/storage/dom_window_storage.h"

namespace blink {

DOMWindowStorageController::DOMWindowStorageController(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {
  window.RegisterEventListenerObserver(this);
}

void DOMWindowStorageController::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char DOMWindowStorageController::kSupplementName[] =
    "DOMWindowStorageController";

// static
DOMWindowStorageController& DOMWindowStorageController::From(
    LocalDOMWindow& window) {
  DOMWindowStorageController* controller =
      Supplement<LocalDOMWindow>::From<DOMWindowStorageController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<DOMWindowStorageController>(window);
    ProvideTo(window, controller);
  }
  return *controller;
}

void DOMWindowStorageController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type == event_type_names::kStorage) {
    // Creating these blink::Storage objects informs the system that we'd like
    // to receive notifications about storage events that might be triggered in
    // other processes. Rather than subscribe to these notifications explicitly,
    // we subscribe to them implicitly to simplify the work done by the system.
    DOMWindowStorage::From(*window).localStorage(IGNORE_EXCEPTION_FOR_TESTING);
    DOMWindowStorage::From(*window).sessionStorage(
        IGNORE_EXCEPTION_FOR_TESTING);
  }
}

}  // namespace blink

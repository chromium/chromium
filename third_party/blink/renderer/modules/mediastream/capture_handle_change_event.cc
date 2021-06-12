// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/capture_handle_change_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_capture_handle.h"

namespace blink {

CaptureHandleChangeEvent::CaptureHandleChangeEvent(
    const AtomicString& type,
    CaptureHandle* capture_handle)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      capture_handle_(capture_handle) {
  DCHECK(capture_handle_);
}

CaptureHandleChangeEvent* CaptureHandleChangeEvent::Create(
    const AtomicString& type,
    const CaptureHandleChangeEventInit* initializer) {
  return MakeGarbageCollected<CaptureHandleChangeEvent>(type, initializer);
}

CaptureHandleChangeEvent::CaptureHandleChangeEvent(
    const AtomicString& type,
    const CaptureHandleChangeEventInit* initializer)
    : Event(type, initializer), capture_handle_(initializer->captureHandle()) {
  DCHECK(capture_handle_);
}

CaptureHandleChangeEvent::~CaptureHandleChangeEvent() = default;

CaptureHandle* CaptureHandleChangeEvent::captureHandle() const {
  return capture_handle_.Get();
}

const AtomicString& CaptureHandleChangeEvent::InterfaceName() const {
  return event_interface_names::kCaptureHandleChangeEvent;
}

void CaptureHandleChangeEvent::Trace(Visitor* visitor) const {
  visitor->Trace(capture_handle_);
  Event::Trace(visitor);
}

}  // namespace blink

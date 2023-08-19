// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

PictureInPictureWindow::PictureInPictureWindow(
    ExecutionContext* execution_context,
    const gfx::Size& size)
    : ActiveScriptWrappable<PictureInPictureWindow>({}),
      ExecutionContextClient(execution_context),
      size_(size) {}

void PictureInPictureWindow::OnClose() {
  size_ = gfx::Size();
}

void PictureInPictureWindow::OnResize(const gfx::Size& size) {
  if (size_ == size)
    return;

  size_ = size;
  DispatchEvent(*Event::Create(event_type_names::kResize));
}

const AtomicString& PictureInPictureWindow::InterfaceName() const {
  return event_target_names::kPictureInPictureWindow;
}

void PictureInPictureWindow::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (event_type == event_type_names::kResize) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPictureInPictureWindowResizeEventListener);
  }

  EventTarget::AddedEventListener(event_type, registered_listener);
}

bool PictureInPictureWindow::HasPendingActivity() const {
  return GetExecutionContext() && HasEventListeners();
}

void PictureInPictureWindow::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink

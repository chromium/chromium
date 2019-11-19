// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/xr/xr.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

namespace {

class XRCanvasInputEventListener : public NativeEventListener {
 public:
  XRCanvasInputEventListener(XRCanvasInputProvider* input_provider)
      : input_provider_(input_provider) {}

  void Invoke(ExecutionContext* execution_context, Event* event) override {
    if (!input_provider_->ShouldProcessEvents())
      return;

    PointerEvent* pointer_event = ToPointerEvent(event);
    DCHECK(pointer_event);
    if (!pointer_event->isPrimary())
      return;

    if (event->type() == event_type_names::kPointerdown) {
      input_provider_->OnPointerDown(pointer_event);
    } else if (event->type() == event_type_names::kPointerup ||
               event->type() == event_type_names::kPointercancel) {
      input_provider_->OnPointerUp(pointer_event);
    }
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(input_provider_);
    EventListener::Trace(visitor);
  }

 private:
  Member<XRCanvasInputProvider> input_provider_;
};

}  // namespace

XRCanvasInputProvider::XRCanvasInputProvider(XRSession* session,
                                             HTMLCanvasElement* canvas)
    : session_(session), canvas_(canvas) {
  listener_ = MakeGarbageCollected<XRCanvasInputEventListener>(this);
  canvas->addEventListener(event_type_names::kPointerdown, listener_);
  canvas->addEventListener(event_type_names::kPointerup, listener_);
  canvas->addEventListener(event_type_names::kPointercancel, listener_);
}

XRCanvasInputProvider::~XRCanvasInputProvider() {}

void XRCanvasInputProvider::Stop() {
  if (!listener_) {
    return;
  }
  canvas_->removeEventListener(event_type_names::kPointerdown, listener_);
  canvas_->removeEventListener(event_type_names::kPointerup, listener_);
  canvas_->removeEventListener(event_type_names::kPointercancel, listener_);
  canvas_ = nullptr;
  listener_ = nullptr;
}

bool XRCanvasInputProvider::ShouldProcessEvents() {
  // Don't process canvas gestures if there's an active immersive session.
  return !(session_->xr()->frameProvider()->immersive_session());
}

void XRCanvasInputProvider::OnPointerDown(PointerEvent* event) {
  UpdateInputSource(event);
  input_source_->OnSelectStart();
}

void XRCanvasInputProvider::OnPointerUp(PointerEvent* event) {
  UpdateInputSource(event);
  input_source_->OnSelect();
  ClearInputSource();
}

XRInputSource* XRCanvasInputProvider::GetInputSource() {
  return input_source_;
}

void XRCanvasInputProvider::UpdateInputSource(PointerEvent* event) {
  if (!canvas_)
    return;

  if (!input_source_) {
    // XRSession doesn't like source ID's of 0.  We should only be processing
    // Canvas Input events in non-immersive sessions anyway, where we don't
    // expect other controllers, so this number is somewhat arbitrary anyway.
    input_source_ = MakeGarbageCollected<XRInputSource>(
        session_, 1, device::mojom::XRTargetRayMode::TAPPING);
    session_->AddTransientInputSource(input_source_);
  }

  // Get the event location relative to the canvas element.
  double element_x = event->pageX() - canvas_->OffsetLeft();
  double element_y = event->pageY() - canvas_->OffsetTop();

  // Unproject the event location into a pointer matrix. This takes the 2D
  // position of the screen interaction and shoves it backwards through the
  // projection matrix to get a 3D point in space, which is then returned in
  // matrix form so we can use it as an XRInputSource's pointerMatrix.
  XRViewData& view = session_->views()[0];
  TransformationMatrix viewer_from_pointer = view.UnprojectPointer(
      element_x, element_y, canvas_->OffsetWidth(), canvas_->OffsetHeight());

  // Update the pointer pose in input space. For screen tapping, input
  // space is equivalent to viewer space.
  input_source_->SetInputFromPointer(&viewer_from_pointer);
}

void XRCanvasInputProvider::ClearInputSource() {
  session_->RemoveTransientInputSource(input_source_);
  input_source_ = nullptr;
}

void XRCanvasInputProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(canvas_);
  visitor->Trace(listener_);
  visitor->Trace(input_source_);
}

}  // namespace blink

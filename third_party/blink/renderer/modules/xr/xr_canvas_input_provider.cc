// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_canvas_input_provider.h"

#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/xr/xr_device.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"

namespace blink {

namespace {

class XRCanvasInputEventListener : public EventListener {
 public:
  XRCanvasInputEventListener(XRCanvasInputProvider* input_provider)
      : EventListener(kCPPEventListenerType), input_provider_(input_provider) {}

  bool operator==(const EventListener& that) const override {
    return this == &that;
  }

  void handleEvent(ExecutionContext* execution_context, Event* event) override {
    if (!input_provider_->ShouldProcessEvents())
      return;

    if (event->type() == EventTypeNames::pointerdown) {
      input_provider_->OnPointerDown(ToPointerEvent(event));
    } else if (event->type() == EventTypeNames::pointerup ||
               event->type() == EventTypeNames::pointercancel) {
      input_provider_->OnPointerUp(ToPointerEvent(event));
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
  listener_ = new XRCanvasInputEventListener(this);
  canvas->addEventListener(EventTypeNames::pointerdown, listener_);
  canvas->addEventListener(EventTypeNames::pointerup, listener_);
  canvas->addEventListener(EventTypeNames::pointercancel, listener_);
}

XRCanvasInputProvider::~XRCanvasInputProvider() {
}

void XRCanvasInputProvider::Stop() {
  if (!listener_) {
    return;
  }
  canvas_->removeEventListener(EventTypeNames::pointerdown, listener_);
  canvas_->removeEventListener(EventTypeNames::pointerup, listener_);
  canvas_->removeEventListener(EventTypeNames::pointercancel, listener_);
  canvas_ = nullptr;
  listener_ = nullptr;
}

bool XRCanvasInputProvider::ShouldProcessEvents() {
  // Don't process canvas gestures if there's an active immersive session.
  return !(session_->device()->frameProvider()->immersive_session());
}

void XRCanvasInputProvider::OnPointerDown(PointerEvent* event) {
  UpdateInputSource(event);
  session_->OnSelectStart(input_source_);
}

void XRCanvasInputProvider::OnPointerUp(PointerEvent* event) {
  UpdateInputSource(event);
  session_->OnSelect(input_source_);
  ClearInputSource();
}

XRInputSource* XRCanvasInputProvider::GetInputSource() {
  return input_source_;
}

void XRCanvasInputProvider::UpdateInputSource(PointerEvent* event) {
  if (!canvas_)
    return;

  if (!input_source_) {
    input_source_ = new XRInputSource(session_, 0);
    input_source_->SetTargetRayMode(XRInputSource::kScreen);
  }

  // Get the event location relative to the canvas element.
  double element_x = event->pageX() - canvas_->OffsetLeft();
  double element_y = event->pageY() - canvas_->OffsetTop();

  // Unproject the event location into a pointer matrix. This takes the 2D
  // position of the screen interaction and shoves it backwards through the
  // projection matrix to get a 3D point in space, which is then returned in
  // matrix form so we can use it as an XRInputSource's pointerMatrix.
  XRView* view = session_->views()[0];
  std::unique_ptr<TransformationMatrix> pointer_transform_matrix =
      view->UnprojectPointer(element_x, element_y, canvas_->OffsetWidth(),
                             canvas_->OffsetHeight());

  // Update the input source's pointer matrix.
  input_source_->SetPointerTransformMatrix(std::move(pointer_transform_matrix));
}

void XRCanvasInputProvider::ClearInputSource() {
  input_source_ = nullptr;
}

void XRCanvasInputProvider::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(canvas_);
  visitor->Trace(listener_);
  visitor->Trace(input_source_);
}

}  // namespace blink

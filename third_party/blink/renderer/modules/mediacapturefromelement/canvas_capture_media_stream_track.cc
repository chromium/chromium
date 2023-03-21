// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_media_stream_track.h"

#include <memory>
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/auto_canvas_draw_listener.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/on_request_canvas_draw_listener.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/timed_canvas_draw_listener.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"

namespace blink {

HTMLCanvasElement* CanvasCaptureMediaStreamTrack::canvas() const {
  return canvas_element_.Get();
}

void CanvasCaptureMediaStreamTrack::requestFrame() {
  draw_listener_->RequestFrame();
}

CanvasCaptureMediaStreamTrack* CanvasCaptureMediaStreamTrack::clone(
    ExecutionContext* script_state) {
  MediaStreamComponent* cloned_component = Component()->Clone();
  CanvasCaptureMediaStreamTrack* cloned_track =
      MakeGarbageCollected<CanvasCaptureMediaStreamTrack>(*this,
                                                          cloned_component);

  return cloned_track;
}

void CanvasCaptureMediaStreamTrack::Trace(Visitor* visitor) const {
  visitor->Trace(canvas_element_);
  visitor->Trace(draw_listener_);
  MediaStreamTrackImpl::Trace(visitor);
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(
    const CanvasCaptureMediaStreamTrack& track,
    MediaStreamComponent* component)
    : MediaStreamTrackImpl(track.canvas_element_->GetExecutionContext(),
                           component),
      canvas_element_(track.canvas_element_),
      draw_listener_(track.draw_listener_) {
  canvas_element_->AddListener(draw_listener_.Get());
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(
    MediaStreamComponent* component,
    HTMLCanvasElement* element,
    ExecutionContext* context,
    std::unique_ptr<CanvasCaptureHandler> handler)
    : MediaStreamTrackImpl(context, component), canvas_element_(element) {
  draw_listener_ =
      MakeGarbageCollected<AutoCanvasDrawListener>(std::move(handler));
  canvas_element_->AddListener(draw_listener_.Get());
}

CanvasCaptureMediaStreamTrack::CanvasCaptureMediaStreamTrack(
    MediaStreamComponent* component,
    HTMLCanvasElement* element,
    ExecutionContext* context,
    std::unique_ptr<CanvasCaptureHandler> handler,
    double frame_rate)
    : MediaStreamTrackImpl(context, component), canvas_element_(element) {
  if (frame_rate == 0) {
    draw_listener_ =
        MakeGarbageCollected<OnRequestCanvasDrawListener>(std::move(handler));
  } else {
    draw_listener_ = TimedCanvasDrawListener::Create(std::move(handler),
                                                     frame_rate, context);
  }
  canvas_element_->AddListener(draw_listener_.Get());
}

}  // namespace blink

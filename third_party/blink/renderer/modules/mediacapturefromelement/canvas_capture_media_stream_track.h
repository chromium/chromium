// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_CANVAS_CAPTURE_MEDIA_STREAM_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_CANVAS_CAPTURE_MEDIA_STREAM_TRACK_H_

#include <memory>
#include "third_party/blink/renderer/core/html/canvas/canvas_draw_listener.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ExecutionContext;
class HTMLCanvasElement;
class CanvasCaptureHandler;

class CanvasCaptureMediaStreamTrack final : public MediaStreamTrack {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CanvasCaptureMediaStreamTrack(const CanvasCaptureMediaStreamTrack&,
                                MediaStreamComponent*);
  CanvasCaptureMediaStreamTrack(MediaStreamComponent*,
                                HTMLCanvasElement*,
                                ExecutionContext*,
                                std::unique_ptr<CanvasCaptureHandler>);
  CanvasCaptureMediaStreamTrack(MediaStreamComponent*,
                                HTMLCanvasElement*,
                                ExecutionContext*,
                                std::unique_ptr<CanvasCaptureHandler>,
                                double frame_rate);

  HTMLCanvasElement* canvas() const;
  void requestFrame();

  CanvasCaptureMediaStreamTrack* clone(ScriptState*) override;

  void Trace(blink::Visitor*) override;

 private:
  Member<HTMLCanvasElement> canvas_element_;
  Member<CanvasDrawListener> draw_listener_;
};

}  // namespace blink

#endif

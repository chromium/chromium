// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_AUTO_CANVAS_DRAW_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_AUTO_CANVAS_DRAW_LISTENER_H_

#include <memory>
#include "third_party/blink/renderer/core/html/canvas/canvas_draw_listener.h"
#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_handler.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AutoCanvasDrawListener : public GarbageCollected<AutoCanvasDrawListener>,
                               public CanvasDrawListener {
 public:
  explicit AutoCanvasDrawListener(std::unique_ptr<CanvasCaptureHandler>);
  ~AutoCanvasDrawListener() override = default;

  void SendNewFrame(
      scoped_refptr<StaticBitmapImage>,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) override;
  bool NeedsNewFrame() const final;
  void RequestFrame() final;

  void Trace(Visitor*) const override {}

 protected:
  std::unique_ptr<CanvasCaptureHandler> handler_;
  bool frame_capture_requested_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_AUTO_CANVAS_DRAW_LISTENER_H_

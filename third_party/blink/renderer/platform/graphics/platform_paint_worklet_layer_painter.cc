// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint_worklet_paint_dispatcher.h"

namespace blink {

PlatformPaintWorkletLayerPainter::PlatformPaintWorkletLayerPainter(
    std::unique_ptr<PaintWorkletPaintDispatcher> dispatcher)
    : dispatcher_(std::move(dispatcher)) {
  TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("cc"),
      "PlatformPaintWorkletLayerPainter::PlatformPaintWorkletLayerPainter");
}

PlatformPaintWorkletLayerPainter::~PlatformPaintWorkletLayerPainter() {
  TRACE_EVENT0(
      TRACE_DISABLED_BY_DEFAULT("cc"),
      "PlatformPaintWorkletLayerPainter::~PlatformPaintWorkletLayerPainter");
}

void PlatformPaintWorkletLayerPainter::DispatchWorklets(
    cc::PaintWorkletJobMap worklet_data_map,
    DoneCallback done_callback) {
  dispatcher_->DispatchWorklets(std::move(worklet_data_map),
                                std::move(done_callback));
}

bool PlatformPaintWorkletLayerPainter::HasOngoingDispatch() const {
  return dispatcher_->HasOngoingDispatch();
}

}  // namespace blink

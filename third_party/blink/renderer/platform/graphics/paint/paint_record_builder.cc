// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

PaintRecordBuilder::PaintRecordBuilder() : context_(paint_controller_) {
  paint_controller_.UpdateCurrentPaintChunkProperties(
      PropertyTreeState::Root());
}

PaintRecordBuilder::PaintRecordBuilder(GraphicsContext& containing_context)
    : PaintRecordBuilder() {
  context_.CopyConfigFrom(containing_context);
}

PaintRecord PaintRecordBuilder::EndRecording(
    const PropertyTreeState& replay_state) {
  return paint_controller_.CommitNewDisplayItems().GetPaintRecord(replay_state);
}

void PaintRecordBuilder::EndRecording(cc::PaintCanvas& canvas,
                                      const PropertyTreeState& replay_state) {
  canvas.drawPicture(EndRecording(replay_state));
}

}  // namespace blink

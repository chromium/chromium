// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

namespace blink {

PaintRecordBuilder::PaintRecordBuilder()
    : own_paint_controller_(absl::in_place, PaintController::kTransient),
      paint_controller_(&own_paint_controller_.value()),
      context_(*paint_controller_) {
  paint_controller_->UpdateCurrentPaintChunkProperties(
      PropertyTreeState::Root());
}

PaintRecordBuilder::PaintRecordBuilder(GraphicsContext& containing_context)
    : PaintRecordBuilder() {
  context_.CopyConfigFrom(containing_context);
}

PaintRecordBuilder::PaintRecordBuilder(PaintController& paint_controller)
    : paint_controller_(&paint_controller), context_(*paint_controller_) {}

PaintRecordBuilder::~PaintRecordBuilder() = default;

sk_sp<PaintRecord> PaintRecordBuilder::EndRecording(
    const PropertyTreeState& replay_state) {
  paint_controller_->CommitNewDisplayItems();
  return paint_controller_->GetPaintArtifact().GetPaintRecord(replay_state);
}

void PaintRecordBuilder::EndRecording(cc::PaintCanvas& canvas,
                                      const PropertyTreeState& replay_state) {
  canvas.drawPicture(EndRecording(replay_state));
}

}  // namespace blink

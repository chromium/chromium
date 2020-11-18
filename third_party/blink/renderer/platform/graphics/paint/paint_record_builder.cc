// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

PaintRecordBuilder::PaintRecordBuilder(
    printing::MetafileSkia* metafile,
    GraphicsContext* containing_context,
    PaintController* paint_controller,
    paint_preview::PaintPreviewTracker* tracker)
    : paint_controller_(nullptr) {
  if (paint_controller) {
    paint_controller_ = paint_controller;
  } else {
    own_paint_controller_ =
        std::make_unique<PaintController>(PaintController::kTransient);
    paint_controller_ = own_paint_controller_.get();
  }

  paint_controller_->UpdateCurrentPaintChunkProperties(
      nullptr, PropertyTreeState::Root());

  context_ =
      std::make_unique<GraphicsContext>(*paint_controller_, metafile, tracker);
  if (containing_context) {
    context_->SetDarkModeEnabled(containing_context->IsDarkModeEnabled());
    context_->SetDeviceScaleFactor(containing_context->DeviceScaleFactor());
    context_->SetPrinting(containing_context->Printing());
    context_->SetIsPaintingPreview(containing_context->IsPaintingPreview());
  }
}

PaintRecordBuilder::~PaintRecordBuilder() = default;

sk_sp<PaintRecord> PaintRecordBuilder::EndRecording(
    const PropertyTreeState& replay_state) {
  paint_controller_->CommitNewDisplayItems();
  paint_controller_->FinishCycle();
  return paint_controller_->GetPaintArtifact().GetPaintRecord(replay_state);
}

void PaintRecordBuilder::EndRecording(cc::PaintCanvas& canvas,
                                      const PropertyTreeState& replay_state) {
  canvas.drawPicture(EndRecording(replay_state));
}

}  // namespace blink

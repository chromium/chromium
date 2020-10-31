// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

#include <memory>
#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

static DisplayItemClient& StaticDummyClient() {
  DEFINE_STATIC_LOCAL(FakeDisplayItemClient, client, ());
  client.Validate();
  return client;
}

TestPaintArtifact& TestPaintArtifact::Chunk(int id) {
  Chunk(StaticDummyClient(),
        static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + id));
  // The default bounds with magic numbers make the chunks have different bounds
  // from each other, for e.g. RasterInvalidatorTest to check the tracked raster
  // invalidation rects of chunks. The actual values don't matter. If the chunk
  // has display items, we will recalculate the bounds from the display items
  // when constructing the PaintArtifact.
  Bounds(IntRect(id * 110, id * 220, id * 220 + 200, id * 110 + 200));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Chunk(DisplayItemClient& client,
                                            DisplayItem::Type type) {
  auto& display_item_list = paint_artifact_->GetDisplayItemList();
  paint_artifact_->PaintChunks().emplace_back(
      display_item_list.size(), display_item_list.size(),
      PaintChunk::Id(client, type), PropertyTreeState::Root());
  // Assume PaintController has processed this chunk.
  paint_artifact_->PaintChunks().back().client_is_just_created = false;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Properties(
    const PropertyTreeStateOrAlias& properties) {
  paint_artifact_->PaintChunks().back().properties = properties;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(const IntRect& bounds,
                                                  Color color) {
  return RectDrawing(NewClient(), bounds, color);
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    const TransformPaintPropertyNode* scroll_translation) {
  return ScrollHitTest(NewClient(), scroll_translation);
}

TestPaintArtifact& TestPaintArtifact::ForeignLayer(
    scoped_refptr<cc::Layer> layer,
    const IntPoint& offset) {
  DEFINE_STATIC_LOCAL(LiteralDebugNameClient, client, ("ForeignLayer"));
  paint_artifact_->GetDisplayItemList()
      .AllocateAndConstruct<ForeignLayerDisplayItem>(
          client, DisplayItem::kForeignLayerFirst, std::move(layer), offset);
  DidAddDisplayItem();
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(DisplayItemClient& client,
                                                  const IntRect& bounds,
                                                  Color color) {
  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording(bounds);
  if (!bounds.IsEmpty()) {
    PaintFlags flags;
    flags.setColor(color.Rgb());
    canvas->drawRect(bounds, flags);
  }
  paint_artifact_->GetDisplayItemList()
      .AllocateAndConstruct<DrawingDisplayItem>(
          client, DisplayItem::kDrawingFirst, bounds,
          recorder.finishRecordingAsPicture());
  DidAddDisplayItem();
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    DisplayItemClient& client,
    const TransformPaintPropertyNode* scroll_translation) {
  paint_artifact_->PaintChunks().back().EnsureHitTestData().scroll_translation =
      scroll_translation;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::SetRasterEffectOutset(
    RasterEffectOutset outset) {
  paint_artifact_->PaintChunks().back().raster_effect_outset = outset;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::KnownToBeOpaque() {
  paint_artifact_->PaintChunks().back().known_to_be_opaque = true;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Bounds(const IntRect& bounds) {
  auto& chunk = paint_artifact_->PaintChunks().back();
  chunk.bounds = bounds;
  chunk.drawable_bounds = bounds;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::DrawableBounds(
    const IntRect& drawable_bounds) {
  auto& chunk = paint_artifact_->PaintChunks().back();
  chunk.drawable_bounds = drawable_bounds;
  DCHECK(chunk.bounds.Contains(drawable_bounds));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Uncacheable() {
  paint_artifact_->PaintChunks().back().is_cacheable = false;
  return *this;
}

scoped_refptr<PaintArtifact> TestPaintArtifact::Build() {
  return std::move(paint_artifact_);
}

FakeDisplayItemClient& TestPaintArtifact::NewClient() {
  clients_.push_back(std::make_unique<FakeDisplayItemClient>());
  return *clients_.back();
}

FakeDisplayItemClient& TestPaintArtifact::Client(wtf_size_t i) const {
  return *clients_[i];
}

void TestPaintArtifact::DidAddDisplayItem() {
  auto& chunk = paint_artifact_->PaintChunks().back();
  DCHECK_EQ(chunk.end_index, paint_artifact_->GetDisplayItemList().size() - 1);
  const auto& item = paint_artifact_->GetDisplayItemList().back();
  chunk.bounds.Unite(item.VisualRect());
  if (item.DrawsContent())
    chunk.drawable_bounds.Unite(item.VisualRect());
  chunk.end_index++;
}

}  // namespace blink

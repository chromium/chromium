// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/test_paint_artifact.h"

#include <memory>

#include "cc/layers/layer.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scroll_paint_property_node.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

static DisplayItemClient& StaticDummyClient() {
  DEFINE_STATIC_LOCAL(Persistent<FakeDisplayItemClient>, client,
                      (MakeGarbageCollected<FakeDisplayItemClient>()));
  client->Validate();
  return *client;
}

TestPaintArtifact& TestPaintArtifact::Chunk(int id) {
  Chunk(StaticDummyClient(),
        static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + id));
  // The default bounds with magic numbers make the chunks have different bounds
  // from each other, for e.g. RasterInvalidatorTest to check the tracked raster
  // invalidation rects of chunks. The actual values don't matter. If the chunk
  // has display items, we will recalculate the bounds from the display items
  // when constructing the PaintArtifact.
  gfx::Rect bounds(id * 110, id * 220, id * 220 + 200, id * 110 + 200);
  Bounds(bounds);
  DrawableBounds(bounds);
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Chunk(const DisplayItemClient& client,
                                            DisplayItem::Type type) {
  auto& display_item_list = paint_artifact_->GetDisplayItemList();
  paint_artifact_->GetPaintChunks().emplace_back(
      display_item_list.size(), display_item_list.size(), client,
      PaintChunk::Id(client.Id(), type), PropertyTreeState::Root());
  paint_artifact_->RecordDebugInfo(client.Id(), client.DebugName(),
                                   client.OwnerNodeId());
  // Assume PaintController has processed this chunk.
  paint_artifact_->GetPaintChunks().back().client_is_just_created = false;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Properties(
    const PropertyTreeStateOrAlias& properties) {
  paint_artifact_->GetPaintChunks().back().properties = properties;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(const gfx::Rect& bounds,
                                                  Color color) {
  return RectDrawing(NewClient(), bounds, color);
}

TestPaintArtifact& TestPaintArtifact::ForeignLayer(
    scoped_refptr<cc::Layer> layer,
    const gfx::Point& offset) {
  DEFINE_STATIC_DISPLAY_ITEM_CLIENT(client, "ForeignLayer");
  paint_artifact_->GetDisplayItemList()
      .AllocateAndConstruct<ForeignLayerDisplayItem>(
          client->Id(), DisplayItem::kForeignLayerFirst, std::move(layer),
          offset, RasterEffectOutset::kNone,
          client->GetPaintInvalidationReason());
  paint_artifact_->RecordDebugInfo(client->Id(), client->DebugName(),
                                   client->OwnerNodeId());
  DidAddDisplayItem();
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(
    const DisplayItemClient& client,
    const gfx::Rect& bounds,
    Color color) {
  PaintRecorder recorder;
  cc::PaintCanvas* canvas = recorder.beginRecording();
  if (!bounds.IsEmpty()) {
    cc::PaintFlags flags;
    flags.setColor(color.toSkColor4f());
    canvas->drawRect(gfx::RectToSkRect(bounds), flags);
  }
  paint_artifact_->GetDisplayItemList()
      .AllocateAndConstruct<DrawingDisplayItem>(
          client.Id(), DisplayItem::kDrawingFirst, bounds,
          recorder.finishRecordingAsPicture(),
          client.VisualRectOutsetForRasterEffects(),
          client.GetPaintInvalidationReason());
  paint_artifact_->RecordDebugInfo(client.Id(), client.DebugName(),
                                   client.OwnerNodeId());
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  chunk.background_color.color = color.toSkColor4f();
  chunk.background_color.area = bounds.size().GetArea();
  // is_solid_color should be set explicitly with IsSolidColor().
  chunk.background_color.is_solid_color = false;
  DidAddDisplayItem();
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTestChunk(
    const DisplayItemClient& client,
    const PropertyTreeState& contents_state) {
  const auto& scroll_translation = contents_state.Transform();
  DCHECK(scroll_translation.ScrollNode());
  Chunk(client, DisplayItem::kScrollHitTest)
      .Properties(*scroll_translation.Parent(), *contents_state.Clip().Parent(),
                  contents_state.Effect());
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  chunk.hit_test_opaqueness = cc::HitTestOpaqueness::kOpaque;
  auto& hit_test_data = chunk.EnsureHitTestData();
  hit_test_data.scroll_hit_test_rect =
      scroll_translation.ScrollNode()->ContainerRect();
  hit_test_data.scroll_translation = &scroll_translation;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollingContentsChunk(
    const DisplayItemClient& client,
    const PropertyTreeState& state,
    bool opaque) {
  gfx::Rect contents_rect = state.Transform().ScrollNode()->ContentsRect();
  Chunk(client).Properties(state).Bounds(contents_rect);
  if (opaque) {
    RectKnownToBeOpaque(contents_rect);
  }
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollChunks(
    const PropertyTreeState& contents_state,
    bool contents_opaque) {
  return ScrollHitTestChunk(contents_state)
      .ScrollingContentsChunk(contents_state, contents_opaque);
}

TestPaintArtifact& TestPaintArtifact::SetRasterEffectOutset(
    RasterEffectOutset outset) {
  paint_artifact_->GetPaintChunks().back().raster_effect_outset = outset;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectKnownToBeOpaque(const gfx::Rect& r) {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  chunk.rect_known_to_be_opaque = r;
  DCHECK(chunk.bounds.Contains(r));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::TextKnownToBeOnOpaqueBackground() {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  DCHECK(chunk.has_text);
  paint_artifact_->GetPaintChunks()
      .back()
      .text_known_to_be_on_opaque_background = true;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::HasText() {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  chunk.has_text = true;
  chunk.text_known_to_be_on_opaque_background = false;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::IsSolidColor() {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  DCHECK_EQ(chunk.size(), 1u);
  chunk.background_color.is_solid_color = true;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::EffectivelyInvisible() {
  paint_artifact_->GetPaintChunks().back().effectively_invisible = true;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Bounds(const gfx::Rect& bounds) {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  chunk.bounds = bounds;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::DrawableBounds(
    const gfx::Rect& drawable_bounds) {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  chunk.drawable_bounds = drawable_bounds;
  DCHECK(chunk.bounds.Contains(drawable_bounds));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::Uncacheable() {
  paint_artifact_->GetPaintChunks().back().is_cacheable = false;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::IsMovedFromCachedSubsequence() {
  paint_artifact_->GetPaintChunks().back().is_moved_from_cached_subsequence =
      true;
  return *this;
}

const PaintArtifact& TestPaintArtifact::Build() {
  const PaintArtifact& result = *paint_artifact_;
  paint_artifact_ = nullptr;
  return result;
}

FakeDisplayItemClient& TestPaintArtifact::NewClient() {
  clients_.push_back(MakeGarbageCollected<FakeDisplayItemClient>());
  return *clients_.back();
}

FakeDisplayItemClient& TestPaintArtifact::Client(wtf_size_t i) const {
  return *clients_[i];
}

void TestPaintArtifact::DidAddDisplayItem() {
  auto& chunk = paint_artifact_->GetPaintChunks().back();
  DCHECK_EQ(chunk.end_index, paint_artifact_->GetDisplayItemList().size() - 1);
  const auto& item = paint_artifact_->GetDisplayItemList().back();
  chunk.bounds.Union(item.VisualRect());
  if (item.DrawsContent()) {
    chunk.drawable_bounds.Union(item.VisualRect());
  }
  chunk.end_index++;
}

}  // namespace blink

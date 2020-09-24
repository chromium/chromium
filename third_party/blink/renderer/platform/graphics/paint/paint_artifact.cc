// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

#include "cc/layers/layer.h"
#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

PaintArtifact::PaintArtifact() : display_item_list_(0) {}

PaintArtifact::PaintArtifact(DisplayItemList display_items,
                             Vector<PaintChunk> chunks)
    : display_item_list_(std::move(display_items)), chunks_(std::move(chunks)) {
}

PaintArtifact::~PaintArtifact() = default;

scoped_refptr<PaintArtifact> PaintArtifact::Create(
    DisplayItemList display_items,
    Vector<PaintChunk> chunks) {
  return base::AdoptRef(
      new PaintArtifact(std::move(display_items), std::move(chunks)));
}

scoped_refptr<PaintArtifact> PaintArtifact::Empty() {
  DEFINE_STATIC_REF(PaintArtifact, empty, base::AdoptRef(new PaintArtifact()));
  return empty;
}

size_t PaintArtifact::ApproximateUnsharedMemoryUsage() const {
  size_t total_size = sizeof(*this) + display_item_list_.MemoryUsageInBytes() +
                      chunks_.capacity() * sizeof(chunks_[0]);
  for (const auto& chunk : chunks_)
    total_size += chunk.MemoryUsageInBytes();
  return total_size;
}

void PaintArtifact::Replay(GraphicsContext& graphics_context,
                           const PropertyTreeState& replay_state,
                           const IntPoint& offset) const {
  Replay(*graphics_context.Canvas(), replay_state, offset);
}

void PaintArtifact::Replay(cc::PaintCanvas& canvas,
                           const PropertyTreeState& replay_state,
                           const IntPoint& offset) const {
  TRACE_EVENT0("blink,benchmark", "PaintArtifact::replay");
  canvas.drawPicture(GetPaintRecord(replay_state, offset));
}

sk_sp<PaintRecord> PaintArtifact::GetPaintRecord(
    const PropertyTreeState& replay_state,
    const IntPoint& offset) const {
  return PaintChunksToCcLayer::Convert(
             PaintChunks(), replay_state,
             gfx::Vector2dF(offset.X(), offset.Y()), GetDisplayItemList(),
             cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
      ->ReleaseAsRecord();
}

// The heuristic for picking a checkerboarding color works as follows:
//   - During paint, PaintChunker will look for background color display items,
//     and annotates the chunk with the index of the display item that paints
//     the largest area background color (ties are broken by selecting the
//     display item that paints last).
//   - After layer allocation, the paint chunks assigned to a layer are
//     examined for a background color annotation. The chunk with the largest
//     background color annotation is selected.
//   - If the area of the selected background color is at least half the size
//     of the layer, then it is set as the layer's background color.
//   - The same color is used for the layer's safe opaque background color, but
//     without the size requirement, as safe opaque background color should
//     always get a value if possible.
void PaintArtifact::UpdateBackgroundColor(
    cc::Layer* layer,
    const PaintChunkSubset& paint_chunks) const {
  SkColor color = SK_ColorTRANSPARENT;
  uint64_t area = 0;
  for (const auto& chunk : paint_chunks) {
    if (chunk.background_color != Color::kTransparent &&
        chunk.background_color_area >= area) {
      color = chunk.background_color.Rgb();
      area = chunk.background_color_area;
    }
  }

  layer->SetSafeOpaqueBackgroundColor(color);

  base::ClampedNumeric<uint64_t> layer_area = layer->bounds().width();
  layer_area *= layer->bounds().height();
  if (area < static_cast<uint64_t>(layer_area) / 2)
    color = SK_ColorTRANSPARENT;
  layer->SetBackgroundColor(color);
}

void PaintArtifact::FinishCycle() {
  for (auto& chunk : chunks_)
    chunk.client_is_just_created = false;
}

}  // namespace blink

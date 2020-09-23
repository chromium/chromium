// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

#include <ostream>

#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

String PaintChunkIndex::ToString() const {
  return String::Format("{%i, %i}", segment_index, chunk_index);
}

std::ostream& operator<<(std::ostream& os, PaintChunkIndex i) {
  return os << i.ToString();
}

String DisplayItemIndex::ToString() const {
  return String::Format("{%i, %i}", segment_index, item_index);
}

std::ostream& operator<<(std::ostream& os, DisplayItemIndex i) {
  return os << i.ToString();
}

PaintArtifact::PaintArtifact() = default;
PaintArtifact::~PaintArtifact() = default;

PaintChunkSubset PaintArtifact::Chunks() const {
  return PaintChunkSubset(this, 0, segments_.size());
}

size_t PaintArtifact::ApproximateUnsharedMemoryUsage() const {
  size_t total_size =
      sizeof(*this) + segments_.capacity() * sizeof(segments_[0]);
  for (const auto& segment : segments_) {
    total_size += segment.display_item_list.MemoryUsageInBytes();
    total_size += (segment.chunks.capacity() - segment.chunks.size()) *
                  sizeof(segment.chunks[0]);
    for (const auto& chunk : segment.chunks)
      total_size += chunk.MemoryUsageInBytes();
  }
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
             Chunks(), replay_state, gfx::Vector2dF(offset.X(), offset.Y()),
             cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
      ->ReleaseAsRecord();
}

void PaintArtifact::FinishCycle() {
  for (auto& segment : segments_) {
    for (auto& chunk : segment.chunks)
      chunk.client_is_just_created = false;
  }
}

}  // namespace blink

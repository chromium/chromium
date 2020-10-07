// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

#include "cc/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

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
             PaintChunkSubset(this), replay_state,
             gfx::Vector2dF(offset.X(), offset.Y()),
             cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
      ->ReleaseAsRecord();
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"

namespace blink {

size_t PaintArtifact::ApproximateUnsharedMemoryUsage() const {
  size_t total_size = sizeof(*this) + display_item_list_.MemoryUsageInBytes() -
                      sizeof(display_item_list_) + chunks_.CapacityInBytes();
  for (const auto& chunk : chunks_) {
    size_t chunk_size = chunk.MemoryUsageInBytes();
    DCHECK_GE(chunk_size, sizeof(chunk));
    total_size += chunk_size - sizeof(chunk);
  }
  return total_size;
}

sk_sp<PaintRecord> PaintArtifact::GetPaintRecord(
    const PropertyTreeState& replay_state) const {
  return PaintChunksToCcLayer::Convert(
             PaintChunkSubset(this), replay_state, gfx::Vector2dF(),
             cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
      ->ReleaseAsRecord();
}

}  // namespace blink

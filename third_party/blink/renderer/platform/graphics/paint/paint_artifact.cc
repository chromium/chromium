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

void PaintArtifact::RecordDebugInfo(DisplayItemClientId client_id,
                                    const String& name,
                                    DOMNodeId owner_node_id) {
  debug_info_.insert(client_id, ClientDebugInfo({name, owner_node_id}));
}

String PaintArtifact::ClientDebugName(DisplayItemClientId client_id) const {
  auto iterator = debug_info_.find(client_id);
  if (iterator == debug_info_.end())
    return "";
  return iterator->value.name;
}

DOMNodeId PaintArtifact::ClientOwnerNodeId(
    DisplayItemClientId client_id) const {
  auto iterator = debug_info_.find(client_id);
  if (iterator == debug_info_.end())
    return kInvalidDOMNodeId;
  return iterator->value.owner_node_id;
}

String PaintArtifact::IdAsString(const DisplayItem::Id& id) const {
#if DCHECK_IS_ON()
  String debug_name = ClientDebugName(id.client_id);
  if (!debug_name.IsEmpty()) {
    return String::Format(
        "%p:%s:%s:%d", reinterpret_cast<void*>(id.client_id),
        ClientDebugName(id.client_id).Utf8().c_str(),
        DisplayItem::TypeAsDebugString(id.type).Utf8().c_str(), id.fragment);
  }
#endif
  return id.ToString();
}

}  // namespace blink

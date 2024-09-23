// Copyright 2015 The Chromium Authors
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

PaintRecord PaintArtifact::GetPaintRecord(const PropertyTreeState& replay_state,
                                          const gfx::Rect* cull_rect) const {
  return PaintChunksToCcLayer::Convert(PaintChunkSubset(*this), replay_state,
                                       cull_rect);
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
  if (!debug_name.empty()) {
    return String::Format(
        "%p:%s:%s:%d", reinterpret_cast<void*>(id.client_id),
        ClientDebugName(id.client_id).Utf8().c_str(),
        DisplayItem::TypeAsDebugString(id.type).Utf8().c_str(), id.fragment);
  }
#endif
  return id.ToString();
}

std::unique_ptr<JSONArray> PaintArtifact::ToJSON() const {
  auto json = std::make_unique<JSONArray>();
  AppendChunksAsJSON(0, chunks_.size(), *json);
  return json;
}

void PaintArtifact::AppendChunksAsJSON(
    wtf_size_t start_chunk_index,
    wtf_size_t end_chunk_index,
    JSONArray& json_array,
    DisplayItemList::JsonOption option) const {
  DCHECK_GT(end_chunk_index, start_chunk_index);
  for (auto i = start_chunk_index; i < end_chunk_index; ++i) {
    const auto& chunk = chunks_[i];
    auto json_object = std::make_unique<JSONObject>();

    json_object->SetString("chunk", ClientDebugName(chunk.id.client_id) + " " +
                                        chunk.id.ToString(*this));
    json_object->SetString("state", chunk.properties.ToString());
    json_object->SetString("bounds", String(chunk.bounds.ToString()));
#if DCHECK_IS_ON()
    if (option == DisplayItemList::kShowPaintRecords) {
      json_object->SetString("chunkData", chunk.ToString(*this));
    }
    json_object->SetArray("displayItems", DisplayItemList::DisplayItemsAsJSON(
                                              *this, chunk.begin_index,
                                              DisplayItemsInChunk(i), option));
#endif
    json_array.PushObject(std::move(json_object));
  }
}

void PaintArtifact::clear() {
  display_item_list_.clear();
  chunks_.clear();
  debug_info_.clear();
}

std::ostream& operator<<(std::ostream& os, const PaintArtifact& artifact) {
  return os << artifact.ToJSON()->ToPrettyJSONString().Utf8();
}

}  // namespace blink

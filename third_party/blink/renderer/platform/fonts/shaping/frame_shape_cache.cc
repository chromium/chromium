// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/frame_shape_cache.h"

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"

namespace blink {

FrameShapeCache::FrameShapeCache() {
  // `80` and `180` were chosen for Speedometer3 Charts-chartjs.
  node_map_.ReserveCapacityForSize(80u);
  shape_map_.ReserveCapacityForSize(180u);
}

void FrameShapeCache::Trace(Visitor* visitor) const {
  visitor->Trace(node_map_);
  visitor->Trace(shape_map_);
}

void FrameShapeCache::DidSwitchFrame() {
  if (!added_new_entries_) {
    return;
  }
  added_new_entries_ = false;

  if (frame_generation_ > kInitialFrame) {
    RemoveOldEntries(node_map_, node_lru_list_);
    RemoveOldEntries(shape_map_, shape_lru_list_);
  }
  ++frame_generation_;
  if (frame_generation_ == kInitialFrame) {
    ++frame_generation_;
  }
}

template <typename E>
E* FrameShapeCache::FindOrCreateEntry(const String& text,
                                      TextDirection direction,
                                      HeapHashMap<KeyType, E>& map,
                                      LruList& lru_list) {
  auto result = map.insert(KeyType{text, direction}, E());
  if (result.is_new_entry) {
    return &result.stored_value->value;
    // We don't care about the cache size here. DidSwitchFrame() removes
    // old entries.
  }
  wtf_size_t list_index = result.stored_value->value.list_index;
  // On cache hit,
  // - Do nothing if it's created in the initial frame
  // - Do nothing if it's touched in the current frame.
  // - Otherwise, update `generation`, and move the LRU list entry to the front.
  if (list_index != kNotFound) {
    auto it = lru_list.MakeIterator(list_index);
    if (it->generation != frame_generation_) {
      it->generation = frame_generation_;
      lru_list.MoveTo(it, lru_list.cbegin());
    }
  }
  return &result.stored_value->value;
}

wtf_size_t FrameShapeCache::ListIndexForNewEntry(const String& text,
                                                 TextDirection direction,
                                                 LruList& lru_list) {
  if (frame_generation_ == kInitialFrame) {
    // Do not create an LRU list entry during the initial frame.
    return kNotFound;
  }
  lru_list.push_front(ListKey{{text, direction}, frame_generation_});
  return lru_list.begin().GetIndex();
}

template <typename E>
void FrameShapeCache::RemoveOldEntries(HeapHashMap<KeyType, E>& map,
                                       LruList& lru_list) {
  while (!lru_list.empty()) {
    auto& value = lru_list.back();
    DCHECK_NE(value.generation, kInitialFrame);
    if (value.generation == frame_generation_) {
      return;
    }
    map.erase(value.key);
    lru_list.pop_back();
  }
}

FrameShapeCache::NodeEntry* FrameShapeCache::FindOrCreateNodeEntry(
    const String& text,
    TextDirection direction) {
  NodeEntry* entry =
      FindOrCreateEntry(text, direction, node_map_, node_lru_list_);
  const PlainTextNode* node = entry->node;
  if (node && entry->list_index != WTF::kNotFound) {
    // Touch ShapeResult cache entries for words in the hit node.
    for (const auto& item : node->ItemList()) {
      ShapeEntry* shape_entry =
          FindOrCreateShapeEntry(item.Text(), item.Direction());
      if (!shape_entry->shape_result) {
        RegisterShapeEntry(item, shape_entry);
      }
    }
  }
  return entry;
}

void FrameShapeCache::RegisterNodeEntry(const String& text,
                                        TextDirection direction,
                                        PlainTextNode* node,
                                        NodeEntry* entry) {
  entry->node = node;
  entry->list_index = ListIndexForNewEntry(text, direction, node_lru_list_);
  added_new_entries_ = true;
}

FrameShapeCache::ShapeEntry* FrameShapeCache::FindOrCreateShapeEntry(
    const String& word,
    TextDirection direction) {
  return FindOrCreateEntry(word, direction, shape_map_, shape_lru_list_);
}

void FrameShapeCache::RegisterShapeEntry(const PlainTextItem& item,
                                         ShapeEntry* entry) {
  entry->shape_result = item.GetShapeResult();
  entry->ink_bounds = item.InkBounds();
  entry->list_index =
      ListIndexForNewEntry(item.Text(), item.Direction(), shape_lru_list_);
  added_new_entries_ = true;
}

void FrameShapeCache::NodeEntry::Trace(Visitor* visitor) const {
  visitor->Trace(node);
}

void FrameShapeCache::ShapeEntry::Trace(Visitor* visitor) const {
  visitor->Trace(shape_result);
}

}  // namespace blink

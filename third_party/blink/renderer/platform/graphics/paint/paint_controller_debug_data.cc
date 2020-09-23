// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

#if DCHECK_IS_ON()

namespace blink {

class PaintController::DisplayItemListAsJSON {
  STACK_ALLOCATED();

 public:
  DisplayItemListAsJSON(const DisplayItemList&,
                        const CachedSubsequenceMap&,
                        const Vector<PaintChunk>&,
                        DisplayItemList::JsonFlags);

  String ToString() {
    return ChunksAsJSONArrayRecursive(0, chunks_.size())->ToPrettyJSONString();
  }

 private:
  std::unique_ptr<JSONObject> SubsequenceAsJSONObjectRecursive();
  std::unique_ptr<JSONArray> ChunksAsJSONArrayRecursive(wtf_size_t, wtf_size_t);
  void AppendChunksAsJSON(wtf_size_t, wtf_size_t, JSONArray&);
  String ClientName(const DisplayItemClient&) const;

  struct SubsequenceInfo {
    const DisplayItemClient* client;
    wtf_size_t start_chunk_index;
    wtf_size_t end_chunk_index;
  };

  const DisplayItemList& list_;
  Vector<SubsequenceInfo> subsequences_;
  Vector<SubsequenceInfo>::const_iterator current_subsequence_;
  const Vector<PaintChunk>& chunks_;
  DisplayItemList::JsonFlags flags_;
};

PaintController::DisplayItemListAsJSON::DisplayItemListAsJSON(
    const DisplayItemList& list,
    const CachedSubsequenceMap& subsequence_map,
    const Vector<PaintChunk>& chunks,
    DisplayItemList::JsonFlags flags)
    : list_(list),
      chunks_(chunks),
      flags_(flags) {
  for (const auto& item : subsequence_map) {
    subsequences_.push_back(SubsequenceInfo{
        item.key, item.value.start_chunk_index, item.value.end_chunk_index});
  }
  std::sort(subsequences_.begin(), subsequences_.end(),
            [](const SubsequenceInfo& a, const SubsequenceInfo& b) {
              return a.start_chunk_index == b.start_chunk_index
                         ? a.end_chunk_index > b.end_chunk_index
                         : a.start_chunk_index < b.start_chunk_index;
            });

  current_subsequence_ = subsequences_.begin();
}

std::unique_ptr<JSONObject>
PaintController::DisplayItemListAsJSON::SubsequenceAsJSONObjectRecursive() {
  const auto& subsequence = *current_subsequence_;
  ++current_subsequence_;

  auto json_object = std::make_unique<JSONObject>();

  json_object->SetString("subsequence",
                         String::Format("client: %p ", subsequence.client) +
                             ClientName(*subsequence.client));
  json_object->SetArray(
      "chunks", ChunksAsJSONArrayRecursive(subsequence.start_chunk_index,
                                           subsequence.end_chunk_index));

  return json_object;
}

std::unique_ptr<JSONArray>
PaintController::DisplayItemListAsJSON::ChunksAsJSONArrayRecursive(
    wtf_size_t start_chunk_index,
    wtf_size_t end_chunk_index) {
  auto array = std::make_unique<JSONArray>();
  auto chunk_index = start_chunk_index;

  while (current_subsequence_ != subsequences_.end() &&
         current_subsequence_->start_chunk_index < end_chunk_index) {
    const auto& subsequence = *current_subsequence_;
    DCHECK_GE(subsequence.start_chunk_index, chunk_index);
    DCHECK_LE(subsequence.end_chunk_index, end_chunk_index);

    if (chunk_index < subsequence.start_chunk_index)
      AppendChunksAsJSON(chunk_index, subsequence.start_chunk_index, *array);
    array->PushObject(SubsequenceAsJSONObjectRecursive());
    chunk_index = subsequence.end_chunk_index;
  }

  if (chunk_index < end_chunk_index)
    AppendChunksAsJSON(chunk_index, end_chunk_index, *array);

  return array;
}

void PaintController::DisplayItemListAsJSON::AppendChunksAsJSON(
    wtf_size_t start_chunk_index,
    wtf_size_t end_chunk_index,
    JSONArray& json_array) {
  DCHECK_GT(end_chunk_index, start_chunk_index);
  for (auto i = start_chunk_index; i < end_chunk_index; ++i) {
    const auto& chunk = chunks_[i];
    auto json_object = std::make_unique<JSONObject>();

    json_object->SetString(
        "chunk", ClientName(chunk.id.client) + " " + chunk.id.ToString());
    json_object->SetString("state", chunk.properties.ToString());
    json_object->SetString("bounds", chunk.bounds.ToString());
    if (flags_ & DisplayItemList::kShowPaintRecords)
      json_object->SetString("chunkData", chunk.ToString());

    json_object->SetArray(
        "displayItems", DisplayItemList::DisplayItemsAsJSON(
                            DisplayItemRange(list_.begin() + chunk.begin_index,
                                             list_.begin() + chunk.end_index),
                            flags_));

    json_array.PushObject(std::move(json_object));
  }
}

String PaintController::DisplayItemListAsJSON::ClientName(
    const DisplayItemClient& client) const {
  return client.SafeDebugName(flags_ & DisplayItemList::kClientKnownToBeAlive);
}

void PaintController::ShowDebugDataInternal(
    DisplayItemList::JsonFlags flags) const {
  auto current_list_flags = flags;
  // The clients in the current list are known to be alive before FinishCycle().
  if (committed_)
    current_list_flags |= DisplayItemList::kClientKnownToBeAlive;
  LOG(INFO) << "current display item list: "
            << DisplayItemListAsJSON(
                   current_paint_artifact_->GetDisplayItemList(),
                   current_cached_subsequences_,
                   current_paint_artifact_->DeprecatedChunks(),
                   current_list_flags)
                   .ToString()
                   .Utf8();

  LOG(INFO) << "new display item list: "
            << DisplayItemListAsJSON(
                   new_display_item_list_, new_cached_subsequences_,
                   new_paint_chunks_.PaintChunks(),
                   // The clients in new_display_item_list_ are all alive.
                   flags | DisplayItemList::kClientKnownToBeAlive)
                   .ToString()
                   .Utf8();
}

void PaintController::ShowCompactDebugData() const {
  ShowDebugDataInternal(DisplayItemList::kCompact);
}

void PaintController::ShowDebugData() const {
  ShowDebugDataInternal(DisplayItemList::kDefault);
}

void PaintController::ShowDebugDataWithPaintRecords() const {
  return ShowDebugDataInternal(DisplayItemList::kShowPaintRecords);
}

}  // namespace blink

#endif  // DCHECK_IS_ON()

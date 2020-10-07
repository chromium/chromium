// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

#if DCHECK_IS_ON()

namespace blink {

class PaintController::PaintArtifactAsJSON {
  STACK_ALLOCATED();

 public:
  PaintArtifactAsJSON(const PaintArtifact&,
                      const CachedSubsequenceMap&,
                      DisplayItemList::JsonFlags);

  String ToString() {
    return ChunksAsJSONArrayRecursive(0, artifact_.PaintChunks().size())
        ->ToPrettyJSONString();
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

  const PaintArtifact& artifact_;
  Vector<SubsequenceInfo> subsequences_;
  Vector<SubsequenceInfo>::const_iterator next_subsequence_;
  DisplayItemList::JsonFlags flags_;
};

PaintController::PaintArtifactAsJSON::PaintArtifactAsJSON(
    const PaintArtifact& artifact,
    const CachedSubsequenceMap& subsequence_map,
    DisplayItemList::JsonFlags flags)
    : artifact_(artifact), flags_(flags) {
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

  next_subsequence_ = subsequences_.begin();
}

std::unique_ptr<JSONObject>
PaintController::PaintArtifactAsJSON::SubsequenceAsJSONObjectRecursive() {
  const auto& subsequence = *next_subsequence_;
  ++next_subsequence_;

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
PaintController::PaintArtifactAsJSON::ChunksAsJSONArrayRecursive(
    wtf_size_t start_chunk_index,
    wtf_size_t end_chunk_index) {
  auto array = std::make_unique<JSONArray>();
  auto chunk_index = start_chunk_index;

  while (next_subsequence_ != subsequences_.end() &&
         next_subsequence_->start_chunk_index < end_chunk_index) {
    const auto& subsequence = *next_subsequence_;
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

void PaintController::PaintArtifactAsJSON::AppendChunksAsJSON(
    wtf_size_t start_chunk_index,
    wtf_size_t end_chunk_index,
    JSONArray& json_array) {
  DCHECK_GT(end_chunk_index, start_chunk_index);
  for (auto i = start_chunk_index; i < end_chunk_index; ++i) {
    const auto& chunk = artifact_.PaintChunks()[i];
    auto json_object = std::make_unique<JSONObject>();

    json_object->SetString(
        "chunk", ClientName(chunk.id.client) + " " + chunk.id.ToString());
    json_object->SetString("state", chunk.properties.ToString());
    json_object->SetString("bounds", chunk.bounds.ToString());
    if (flags_ & DisplayItemList::kShowPaintRecords)
      json_object->SetString("chunkData", chunk.ToString());

    json_object->SetArray("displayItems",
                          DisplayItemList::DisplayItemsAsJSON(
                              artifact_.DisplayItemsInChunk(i), flags_));

    json_array.PushObject(std::move(json_object));
  }
}

String PaintController::PaintArtifactAsJSON::ClientName(
    const DisplayItemClient& client) const {
  return client.SafeDebugName(flags_ & DisplayItemList::kClientKnownToBeAlive);
}

void PaintController::ShowDebugDataInternal(
    DisplayItemList::JsonFlags flags) const {
  auto current_list_flags = flags;
  // The clients in the current list are known to be alive before FinishCycle().
  if (committed_)
    current_list_flags |= DisplayItemList::kClientKnownToBeAlive;
  LOG(INFO) << "current paint artifact: "
            << (current_paint_artifact_
                    ? PaintArtifactAsJSON(*current_paint_artifact_,
                                          current_cached_subsequences_,
                                          current_list_flags)
                          .ToString()
                          .Utf8()
                    : "null");

  LOG(INFO)
      << "new paint artifact: "
      << (new_paint_artifact_
              ? PaintArtifactAsJSON(
                    *new_paint_artifact_, new_cached_subsequences_,
                    // The clients in new_display_item_list_ are all alive.
                    flags | DisplayItemList::kClientKnownToBeAlive)
                    .ToString()
                    .Utf8()
              : "null");
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

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
    return SubsequenceAsJSONArrayRecursive(0, list_.size())
        ->ToPrettyJSONString();
  }

 private:
  std::unique_ptr<JSONObject> SubsequenceAsJSONObjectRecursive();
  std::unique_ptr<JSONArray> SubsequenceAsJSONArrayRecursive(size_t, size_t);
  void AppendSubsequenceAsJSON(size_t, size_t, JSONArray&);
  String ClientName(const DisplayItemClient&) const;

  struct SubsequenceInfo {
    SubsequenceInfo(const DisplayItemClient* client, size_t start, size_t end)
        : client(client), start(start), end(end) {}
    const DisplayItemClient* client;
    size_t start;
    size_t end;
  };

  const DisplayItemList& list_;
  Vector<SubsequenceInfo> subsequences_;
  Vector<SubsequenceInfo>::const_iterator current_subsequence_;
  const Vector<PaintChunk>& chunks_;
  Vector<PaintChunk>::const_iterator current_chunk_;
  DisplayItemList::JsonFlags flags_;
};

PaintController::DisplayItemListAsJSON::DisplayItemListAsJSON(
    const DisplayItemList& list,
    const CachedSubsequenceMap& subsequence_map,
    const Vector<PaintChunk>& chunks,
    DisplayItemList::JsonFlags flags)
    : list_(list),
      chunks_(chunks),
      current_chunk_(chunks.begin()),
      flags_(flags) {
  for (const auto& item : subsequence_map) {
    subsequences_.push_back(
        SubsequenceInfo(item.key, item.value.start, item.value.end));
  }
  std::sort(subsequences_.begin(), subsequences_.end(),
            [](const SubsequenceInfo& a, const SubsequenceInfo& b) {
              return a.start == b.start ? a.end > b.end : a.start < b.start;
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
  json_object->SetArray("chunks", SubsequenceAsJSONArrayRecursive(
                                      subsequence.start, subsequence.end));

  return json_object;
}

std::unique_ptr<JSONArray>
PaintController::DisplayItemListAsJSON::SubsequenceAsJSONArrayRecursive(
    size_t start_item,
    size_t end_item) {
  auto array = std::make_unique<JSONArray>();
  size_t item_index = start_item;

  while (current_subsequence_ != subsequences_.end() &&
         current_subsequence_->start < end_item) {
    const auto& subsequence = *current_subsequence_;
    DCHECK(subsequence.start >= item_index);
    DCHECK(subsequence.end <= end_item);

    if (item_index < subsequence.start)
      AppendSubsequenceAsJSON(item_index, subsequence.start, *array);
    array->PushObject(SubsequenceAsJSONObjectRecursive());
    item_index = subsequence.end;
  }

  if (item_index < end_item)
    AppendSubsequenceAsJSON(item_index, end_item, *array);

  return array;
}

void PaintController::DisplayItemListAsJSON::AppendSubsequenceAsJSON(
    size_t start_item,
    size_t end_item,
    JSONArray& json_array) {
  DCHECK(end_item > start_item);
  if (current_chunk_ == chunks_.end()) {
    // We are in the middle of painting with incomplete chunks.
    auto json_object = std::make_unique<JSONObject>();
    json_object->SetString("chunk", "incomplete");
    json_object->SetArray(
        "displayItems", list_.SubsequenceAsJSON(start_item, end_item, flags_));
    json_array.PushObject(std::move(json_object));
    return;
  }

  DCHECK(current_chunk_->begin_index == start_item);
  while (current_chunk_ != chunks_.end() &&
         current_chunk_->end_index <= end_item) {
    const auto& chunk = *current_chunk_;
    auto json_object = std::make_unique<JSONObject>();

    json_object->SetString(
        "chunk", ClientName(chunk.id.client) + " " + chunk.id.ToString());
    json_object->SetString("state", chunk.properties.ToString());
    if (flags_ & DisplayItemList::kShowPaintRecords)
      json_object->SetString("chunkData", chunk.ToString());

    json_object->SetArray(
        "displayItems",
        list_.SubsequenceAsJSON(chunk.begin_index, chunk.end_index, flags_));

    json_array.PushObject(std::move(json_object));
    ++current_chunk_;
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
  LOG(ERROR) << "current display item list: "
             << DisplayItemListAsJSON(
                    current_paint_artifact_->GetDisplayItemList(),
                    current_cached_subsequences_,
                    current_paint_artifact_->PaintChunks(), current_list_flags)
                    .ToString()
                    .Utf8();

  LOG(ERROR) << "new display item list: "
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

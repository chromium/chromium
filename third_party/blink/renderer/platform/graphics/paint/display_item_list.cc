// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

namespace blink {

DisplayItemList::Range<DisplayItemList::iterator>
DisplayItemList::ItemsInPaintChunk(const PaintChunk& paint_chunk) {
  return Range<iterator>(begin() + paint_chunk.begin_index,
                         begin() + paint_chunk.end_index);
}

DisplayItemList::Range<DisplayItemList::const_iterator>
DisplayItemList::ItemsInPaintChunk(const PaintChunk& paint_chunk) const {
  return Range<const_iterator>(begin() + paint_chunk.begin_index,
                               begin() + paint_chunk.end_index);
}

#if DCHECK_IS_ON()

std::unique_ptr<JSONArray> DisplayItemList::DisplayItemsAsJSON(
    wtf_size_t begin_index,
    wtf_size_t end_index,
    JsonFlags flags) const {
  auto json_array = std::make_unique<JSONArray>();
  AppendSubsequenceAsJSON(begin_index, end_index, flags, *json_array);
  return json_array;
}

void DisplayItemList::AppendSubsequenceAsJSON(wtf_size_t begin_index,
                                              wtf_size_t end_index,
                                              JsonFlags flags,
                                              JSONArray& json_array) const {
  if (flags & kCompact) {
    DCHECK(!(flags & kShowPaintRecords))
        << "kCompact cannot show paint records";
    DCHECK(!(flags & kShowOnlyDisplayItemTypes))
        << "kCompact cannot show display item types";
    for (auto i = begin_index; i < end_index; ++i) {
      const auto& item = (*this)[i];
      json_array.PushString(item.GetId().ToString());
    }
  } else {
    for (auto i = begin_index; i < end_index; ++i) {
      auto json = std::make_unique<JSONObject>();

      const auto& item = (*this)[i];
      json->SetInteger("index", i);

      if (flags & kShowOnlyDisplayItemTypes) {
        json->SetString("type", DisplayItem::TypeAsDebugString(item.GetType()));
      } else {
        json->SetString("clientDebugName", item.Client().SafeDebugName(
                                               flags & kClientKnownToBeAlive));
        if (flags & kClientKnownToBeAlive) {
          json->SetString("invalidation",
                          PaintInvalidationReasonToString(
                              item.Client().GetPaintInvalidationReason()));
        }
        item.PropertiesAsJSON(*json);
      }

      if ((flags & kShowPaintRecords) && item.IsDrawing()) {
        const auto& drawing_item = static_cast<const DrawingDisplayItem&>(item);
        if (const auto* record = drawing_item.GetPaintRecord().get())
          json->SetArray("record", RecordAsJSON(*record));
      }

      json_array.PushObject(std::move(json));
    }
  }
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

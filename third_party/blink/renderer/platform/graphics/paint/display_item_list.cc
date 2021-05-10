// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

namespace blink {

DisplayItemList::~DisplayItemList() {
  for (auto& item : *this)
    item.Destruct();
}

#if DCHECK_IS_ON()

std::unique_ptr<JSONArray> DisplayItemList::DisplayItemsAsJSON(
    wtf_size_t first_item_index,
    const DisplayItemRange& display_items,
    JsonFlags flags) {
  auto json_array = std::make_unique<JSONArray>();
  wtf_size_t i = first_item_index;
  DCHECK(!(flags & kCompact) || !(flags & kShowPaintRecords))
      << "kCompact and kShowPaintRecords are exclusive";
  for (auto& item : display_items) {
    if (flags & kCompact) {
      json_array->PushString(
          String::Format("%u: %s", i, item.IdAsString().Utf8().data()));
    } else {
      auto json = std::make_unique<JSONObject>();
      json->SetInteger("index", i);
      item.PropertiesAsJSON(*json, flags & kClientKnownToBeAlive);

      if ((flags & kShowPaintRecords) && item.IsDrawing()) {
        const auto& drawing_item = To<DrawingDisplayItem>(item);
        if (const auto* record = drawing_item.GetPaintRecord().get())
          json->SetArray("record", RecordAsJSON(*record));
      }

      json_array->PushObject(std::move(json));
    }
    i++;
  }
  return json_array;
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

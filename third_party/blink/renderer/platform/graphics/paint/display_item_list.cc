// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"

namespace blink {

#if DCHECK_IS_ON()

std::unique_ptr<JSONArray> DisplayItemList::DisplayItemsAsJSON(
    wtf_size_t first_item_index,
    const DisplayItemRange& display_items,
    JsonFlags flags) {
  auto json_array = std::make_unique<JSONArray>();
  if (flags & kCompact) {
    DCHECK(!(flags & kShowPaintRecords))
        << "kCompact cannot show paint records";
    DCHECK(!(flags & kShowOnlyDisplayItemTypes))
        << "kCompact cannot show display item types";
    for (auto& item : display_items)
      json_array->PushString(item.GetId().ToString());
  } else {
    wtf_size_t i = first_item_index;
    for (auto& item : display_items) {
      auto json = std::make_unique<JSONObject>();

      json->SetInteger("index", i++);

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

      json_array->PushObject(std::move(json));
    }
  }
  return json_array;
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

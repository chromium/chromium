// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"

#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

namespace blink {

void DisplayItemList::clear() {
  for (auto& item : *this) {
    item.Destruct();
  }
  items_.clear();
}

#if DCHECK_IS_ON()

std::unique_ptr<JSONArray> DisplayItemList::DisplayItemsAsJSON(
    const PaintArtifact& paint_artifact,
    wtf_size_t first_item_index,
    const DisplayItemRange& display_items,
    JsonOption option) {
  auto json_array = std::make_unique<JSONArray>();
  wtf_size_t i = first_item_index;
  for (auto& item : display_items) {
    if (option == kCompact) {
      json_array->PushString(String::Format(
          "%u: %s", i, item.IdAsString(paint_artifact).Utf8().c_str()));
    } else {
      auto json = std::make_unique<JSONObject>();
      json->SetInteger("index", i);
      item.PropertiesAsJSON(*json, paint_artifact);

      if (option == kShowPaintRecords) {
        if (const auto* drawing_item = DynamicTo<DrawingDisplayItem>(item)) {
          json->SetArray("record",
                         RecordAsJSON(drawing_item->GetPaintRecord()));
        }
      }

      json_array->PushObject(std::move(json));
    }
    i++;
  }
  return json_array;
}

#endif  // DCHECK_IS_ON()

}  // namespace blink

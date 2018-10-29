// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/hit_test_data.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

void HitTestData::RecordHitTestRect(GraphicsContext& context,
                                    const DisplayItemClient& client,
                                    const HitTestRect& action) {
  DCHECK(RuntimeEnabledFeatures::PaintTouchActionRectsEnabled());

  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  if (!paint_controller.UseCachedDrawingIfPossible(client,
                                                   DisplayItem::kHitTest)) {
    // A display item must be created to ensure a paint chunk exists. For
    // example, without this, an empty div with a transform will incorrectly use
    // the parent paint chunk instead of creating a new one.
    paint_controller.CreateAndAppend<DrawingDisplayItem>(
        client, DisplayItem::kHitTest, nullptr, false);
  }

  auto& chunk = paint_controller.CurrentPaintChunk();
  chunk.EnsureHitTestData().touch_action_rects.push_back(action);
}

}  // namespace blink

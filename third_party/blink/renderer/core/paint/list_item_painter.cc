// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/list_item_painter.h"

#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"

namespace blink {

void ListItemPainter::Paint(const PaintInfo& paint_info) {
  if (!layout_list_item_.LogicalHeight() &&
      layout_list_item_.IsScrollContainer())
    return;

  BlockPainter(layout_list_item_).Paint(paint_info);
}

}  // namespace blink

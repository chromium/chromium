// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/scroll_hit_test_display_item.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

ScrollHitTestDisplayItem::ScrollHitTestDisplayItem(
    const DisplayItemClient& client,
    DisplayItem::Type type,
    const TransformPaintPropertyNode* scroll_offset_node,
    const IntRect& scroll_container_bounds)
    : DisplayItem(client, type, sizeof(*this)),
      scroll_offset_node_(scroll_offset_node),
      scroll_container_bounds_(scroll_container_bounds) {
#if DCHECK_IS_ON()
  if (type == DisplayItem::Type::kResizerScrollHitTest ||
      type == DisplayItem::Type::kPluginScrollHitTest) {
    // Resizer and plugin scroll hit tests are only used to prevent composited
    // scrolling and should not have a scroll offset node.
    DCHECK(!scroll_offset_node);
  } else if (type == DisplayItem::Type::kScrollHitTest) {
    DCHECK(scroll_offset_node);
    // The scroll offset transform node should have an associated scroll node.
    DCHECK(scroll_offset_node_->ScrollNode());
  } else {
    NOTREACHED();
  }
#endif
}

ScrollHitTestDisplayItem::~ScrollHitTestDisplayItem() = default;

bool ScrollHitTestDisplayItem::Equals(const DisplayItem& other) const {
  return DisplayItem::Equals(other) &&
         scroll_offset_node() ==
             static_cast<const ScrollHitTestDisplayItem&>(other)
                 .scroll_offset_node() &&
         scroll_container_bounds() ==
             static_cast<const ScrollHitTestDisplayItem&>(other)
                 .scroll_container_bounds();
}

#if DCHECK_IS_ON()
void ScrollHitTestDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetString("scrollOffsetNode", String::Format("%p", scroll_offset_node_));
  json.SetString("scrollContainerBounds", scroll_container_bounds_.ToString());
}
#endif

void ScrollHitTestDisplayItem::Record(
    GraphicsContext& context,
    const DisplayItemClient& client,
    DisplayItem::Type type,
    const TransformPaintPropertyNode* scroll_offset_node,
    const IntRect& scroll_container_bounds) {
  PaintController& paint_controller = context.GetPaintController();

  // The scroll hit test should be in the non-scrolled transform space and
  // therefore should not be scrolled by the associated scroll offset.
  DCHECK_NE(&paint_controller.CurrentPaintChunkProperties().Transform(),
            scroll_offset_node);

  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  if (paint_controller.UseCachedItemIfPossible(client, type))
    return;

  paint_controller.CreateAndAppend<ScrollHitTestDisplayItem>(
      client, type, scroll_offset_node, scroll_container_bounds);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLL_HIT_TEST_DISPLAY_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLL_HIT_TEST_DISPLAY_ITEM_H_

#include "base/memory/ref_counted.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/transform_paint_property_node.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class GraphicsContext;

// Display item for marking a region as scrollable. This should be emitted in
// the non-scrolling paint property tree state (in other words, this item should
// not scroll). A display item is needed because hit testing is in paint order.
//
// This serves three purposes:
// 1. Creating non-fast scrollable regions for non-composited scrollers.
//   Scrollable areas create a non-fast scrollable region in the
//   non-scrolling paint property tree state. Pre-CompositeAfterPaint, we skip
//   painting these for composited scrollers. With CompositeAfterPaint, we
//   paint the non-fast item and later ignore it if the scroller was composited
//   (see: PaintArtifactCompositor::UpdateNonFastScrollableRegions).
//
// 2. Creating non-fast scrollable regions for plugins and resize handles.
//   Plugins that have blocking event handlers and resize handles both need to
//   prevent composited scrolling. A different display item type is used
//   (kPluginScrollHitTest and kResizerScrollHitTest) to disambiguate multiple
//   scroll hit test display items (e.g., if a scroller is non-composited and
//   has a resizer).
//
// 3. Creating cc::Layers marked as being scrollable.
//   (when CompositeAfterPaint is enabled).
//   A single cc::Layer must be marked as being "scrollable", and this display
//   item is used by PaintArtifactCompositor to create the scrollable cc::Layer.
class PLATFORM_EXPORT ScrollHitTestDisplayItem final : public DisplayItem {
 public:
  ScrollHitTestDisplayItem(const DisplayItemClient&,
                           DisplayItem::Type,
                           const TransformPaintPropertyNode* scroll_offset_node,
                           const IntRect& scroll_container_bounds);
  ~ScrollHitTestDisplayItem() override;

  const TransformPaintPropertyNode* scroll_offset_node() const {
    return scroll_offset_node_;
  }

  const IntRect& scroll_container_bounds() const {
    return scroll_container_bounds_;
  }

  // DisplayItem
  bool Equals(const DisplayItem&) const override;
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif

  // Create and append a ScrollHitTestDisplayItem onto the context. This is
  // similar to a recorder class (e.g., DrawingRecorder) but just emits a single
  // item. If no |scroll_offset_node| is passed in, this display item will only
  // be used for creating a non-fast-scrollable-region.
  static void Record(GraphicsContext&,
                     const DisplayItemClient&,
                     DisplayItem::Type,
                     const TransformPaintPropertyNode* scroll_offset_node,
                     const IntRect& scroll_container_bounds);

  const ScrollPaintPropertyNode* scroll_node() const {
    return scroll_offset_node_->ScrollNode();
  }

 private:
  // The scroll offset transform node if this ScrollHitTestDisplayItem could be
  // used for composited scrolling, or null if it is only used to prevent
  // composited scrolling.
  const TransformPaintPropertyNode* scroll_offset_node_;
  // The bounds of the scroll container, including scrollbars. We cannot use
  // scroll_node().container_rect() because it does not include scrollbars.
  const IntRect scroll_container_bounds_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLL_HIT_TEST_DISPLAY_ITEM_H_

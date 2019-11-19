// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLLBAR_DISPLAY_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLLBAR_DISPLAY_ITEM_H_

#include "cc/input/scrollbar.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class Layer;
}

namespace blink {

class GraphicsContext;
class TransformPaintPropertyNode;

// Represents a non-custom scrollbar in CompositeAfterPaint. During paint, we
// create a ScrollbarDisplayItem for a non-custom scrollbar. During
// PaintArtifactCompositor::Update(), we decide whether to composite the
// scrollbar and, if not composited, call Paint() to actually paint the
// scrollbar into a paint record, otherwise call CreateLayer() to create a
// cc scrollbar layer.
class PLATFORM_EXPORT ScrollbarDisplayItem final : public DisplayItem {
 public:
  ScrollbarDisplayItem(const DisplayItemClient&,
                       Type,
                       scoped_refptr<cc::Scrollbar>,
                       const IntRect& rect,
                       const TransformPaintPropertyNode* scroll_translation,
                       CompositorElementId element_id);

  cc::Scrollbar* GetScrollbar() const { return scrollbar_.get(); }
  const IntRect& GetRect() const { return rect_; }
  const TransformPaintPropertyNode* ScrollTranslation() const {
    return scroll_translation_;
  }
  CompositorElementId ElementId() const { return element_id_; }

  // Paints the scrollbar into the internal paint record, for non-composited
  // scrollbar.
  sk_sp<const PaintRecord> Paint() const;

  // Creates cc layer for composited scrollbar.
  scoped_refptr<cc::Layer> CreateLayer() const;

  // DisplayItem
  bool Equals(const DisplayItem&) const override;
#if DCHECK_IS_ON()
  void PropertiesAsJSON(JSONObject&) const override;
#endif

  // Records a scrollbar into a GraphicsContext. Must check
  // PaintController::UseCachedItem() before calling this function.
  // |rect| is the bounding box of the scrollbar in the current transform space.
  static void Record(GraphicsContext&,
                     const DisplayItemClient&,
                     DisplayItem::Type,
                     scoped_refptr<cc::Scrollbar>,
                     const IntRect& rect,
                     const TransformPaintPropertyNode* scroll_translation,
                     CompositorElementId element_id);

 private:
  scoped_refptr<cc::Scrollbar> scrollbar_;
  IntRect rect_;
  const TransformPaintPropertyNode* scroll_translation_;
  CompositorElementId element_id_;
  // This is lazily created for non-composited scrollbar.
  mutable sk_sp<const PaintRecord> record_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_SCROLLBAR_DISPLAY_ITEM_H_

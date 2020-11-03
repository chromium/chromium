// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_DISPLAY_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_DISPLAY_ITEM_H_

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// DrawingDisplayItem contains recorded painting operations which can be
// replayed to produce a rastered output.
//
// This class has two notions of the bounds around the content that was recorded
// and will be produced by the item. The first is the |record_bounds| which
// describes the bounds of all content in the |record| in the space of the
// record. The second is the |visual_rect| which should describe the same thing,
// but takes into account transforms and clips that would apply to the
// PaintRecord, and is in the space of the DisplayItemList. This allows the
// visual_rect to be compared between DrawingDisplayItems, and to give bounds
// around what the user can actually see from the PaintRecord.
class PLATFORM_EXPORT DrawingDisplayItem : public DisplayItem {
 public:
  DISABLE_CFI_PERF
  DrawingDisplayItem(const DisplayItemClient& client,
                     Type type,
                     const IntRect& visual_rect,
                     sk_sp<const PaintRecord> record);

  const sk_sp<const PaintRecord>& GetPaintRecord() const { return record_; }

  bool Equals(const DisplayItem& other) const final;

  bool KnownToBeOpaque() const {
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return false;
    if (!known_to_be_opaque_is_set_) {
      known_to_be_opaque_is_set_ = true;
      known_to_be_opaque_ = CalculateKnownToBeOpaque(record_.get());
    }
    return known_to_be_opaque_;
  }
  void SetKnownToBeOpaqueForTesting() {
    known_to_be_opaque_is_set_ = true;
    known_to_be_opaque_ = true;
  }

  SkColor BackgroundColor(float& area) const;

 private:
  bool CalculateKnownToBeOpaque(const PaintRecord*) const;

  mutable bool known_to_be_opaque_is_set_ : 1;
  mutable bool known_to_be_opaque_ : 1;
  sk_sp<const PaintRecord> record_;
};

// TODO(dcheng): Move this ctor back inline once the clang plugin is fixed.
DISABLE_CFI_PERF
inline DrawingDisplayItem::DrawingDisplayItem(const DisplayItemClient& client,
                                              Type type,
                                              const IntRect& visual_rect,
                                              sk_sp<const PaintRecord> record)
    : DisplayItem(client,
                  type,
                  sizeof(*this),
                  visual_rect,
                  /* draws_content*/ record && record->size()),
      known_to_be_opaque_is_set_(false),
      known_to_be_opaque_(false),
      record_(DrawsContent() ? std::move(record) : nullptr) {
  DCHECK(IsDrawingType(type));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_DISPLAY_ITEM_H_

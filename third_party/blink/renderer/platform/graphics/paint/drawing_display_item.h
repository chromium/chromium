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
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// DrawingDisplayItem contains recorded painting operations which can be
// replayed to produce a rastered output.
class PLATFORM_EXPORT DrawingDisplayItem : public DisplayItem {
 public:
  DISABLE_CFI_PERF
  DrawingDisplayItem(const DisplayItemClient& client,
                     Type type,
                     const IntRect& visual_rect,
                     sk_sp<const PaintRecord> record);

  const sk_sp<const PaintRecord>& GetPaintRecord() const {
    DCHECK(!IsTombstone());
    return record_;
  }

  bool KnownToBeOpaque() const {
    DCHECK(!IsTombstone());
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return false;
    if (!known_to_be_opaque_is_set_) {
      known_to_be_opaque_is_set_ = true;
      known_to_be_opaque_ = CalculateKnownToBeOpaque(record_.get());
    }
    return known_to_be_opaque_;
  }
  void SetKnownToBeOpaqueForTesting() {
    DCHECK(!IsTombstone());
    known_to_be_opaque_is_set_ = true;
    known_to_be_opaque_ = true;
  }

  SkColor BackgroundColor(float& area) const;

 private:
  friend class DisplayItem;
  bool EqualsForUnderInvalidationImpl(const DrawingDisplayItem&) const;
#if DCHECK_IS_ON()
  void PropertiesAsJSONImpl(JSONObject&) const {}
#endif

  bool CalculateKnownToBeOpaque(const PaintRecord*) const;

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
                  visual_rect,
                  /* draws_content*/ record && record->size()),
      record_(DrawsContent() ? std::move(record) : nullptr) {
  DCHECK(IsDrawing());
}

template <>
struct DowncastTraits<DrawingDisplayItem> {
  static bool AllowFrom(const DisplayItem& i) {
    return !i.IsTombstone() && i.IsDrawing();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_DRAWING_DISPLAY_ITEM_H_

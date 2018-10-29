// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BOX_PAINT_INVALIDATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class LayoutBox;
class LayoutRect;
struct PaintInvalidatorContext;

class CORE_EXPORT BoxPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BoxPaintInvalidator(const LayoutBox& box,
                      const PaintInvalidatorContext& context)
      : box_(box), context_(context) {}

  static void BoxWillBeDestroyed(const LayoutBox&);

  void InvalidatePaint();

 private:
  friend class BoxPaintInvalidatorTest;

  bool HasEffectiveBackground();
  bool BackgroundGeometryDependsOnLayoutOverflowRect();
  bool BackgroundPaintsOntoScrollingContentsLayer();
  bool BackgroundPaintsOntoMainGraphicsLayer();
  bool ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
      const LayoutRect& old_layout_overflow,
      const LayoutRect& new_layout_overflow);

  enum BackgroundInvalidationType { kNone = 0, kIncremental, kFull };
  BackgroundInvalidationType ComputeViewBackgroundInvalidation();
  BackgroundInvalidationType ComputeBackgroundInvalidation(
      bool& should_invalidate_all_layers);
  void InvalidateBackground();

  PaintInvalidationReason ComputePaintInvalidationReason();

  bool NeedsToSavePreviousContentBoxRectOrLayoutOverflowRect();
  void SavePreviousBoxGeometriesIfNeeded();

  const LayoutBox& box_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_OBJECT_PAINT_INVALIDATOR_H_

#include "base/auto_reset.h"
#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DisplayItemClient;
class LayoutObject;
struct PaintInvalidatorContext;

class CORE_EXPORT ObjectPaintInvalidator {
  STACK_ALLOCATED();

 public:
  ObjectPaintInvalidator(const LayoutObject& object) : object_(object) {}

  // This calls LayoutObject::PaintingLayer() which walks up the tree.
  // If possible, use the faster
  // PaintInvalidatorContext.painting_layer.SetNeedsRepaint() instead.
  void SlowSetPaintingLayerNeedsRepaint();

  void SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
      const DisplayItemClient& client,
      PaintInvalidationReason reason) {
    SlowSetPaintingLayerNeedsRepaint();
    InvalidateDisplayItemClient(client, reason);
  }

  void InvalidateDisplayItemClientsIncludingNonCompositingDescendants(
      PaintInvalidationReason);

  // The caller should ensure the painting layer has been SetNeedsRepaint
  // before calling this function.
  void InvalidateDisplayItemClient(const DisplayItemClient&,
                                   PaintInvalidationReason);

  void InvalidatePaintIncludingNonCompositingDescendants();
  void InvalidatePaintIncludingNonSelfPaintingLayerDescendants();

 protected:
  const LayoutObject& object_;
};

class ObjectPaintInvalidatorWithContext : public ObjectPaintInvalidator {
 public:
  ObjectPaintInvalidatorWithContext(const LayoutObject& object,
                                    const PaintInvalidatorContext& context)
      : ObjectPaintInvalidator(object), context_(context) {}

  void InvalidatePaint() {
    InvalidatePaintWithComputedReason(ComputePaintInvalidationReason());
  }

  PaintInvalidationReason ComputePaintInvalidationReason();
  void InvalidatePaintWithComputedReason(PaintInvalidationReason);

 private:
  PaintInvalidationReason InvalidateSelection(PaintInvalidationReason);
  PaintInvalidationReason InvalidatePartialRect(PaintInvalidationReason);

  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayer;
class LayoutObject;
class ComputedStyle;

class CORE_EXPORT CompositingReasonFinder {
  DISALLOW_NEW();

 public:
  static CompositingReasons PotentialCompositingReasonsFromStyle(
      const LayoutObject&);

  static CompositingReasons NonStyleDeterminedDirectReasons(const PaintLayer&);

  CompositingReasonFinder(const CompositingReasonFinder&) = delete;
  CompositingReasonFinder& operator=(const CompositingReasonFinder&) = delete;

  // Returns the direct reasons for compositing the given layer.
  static CompositingReasons DirectReasons(const PaintLayer&);

  static CompositingReasons DirectReasonsForPaintProperties(
      const LayoutObject&);

  static CompositingReasons DirectReasonsForSVGChildPaintProperties(
      const LayoutObject&);

  static CompositingReasons CompositingReasonsForAnimation(const LayoutObject&);
  static CompositingReasons CompositingReasonsForWillChange(
      const ComputedStyle&);
  // Some LayoutObject types do not support transforms (see:
  // |LayoutObject::HasTransformRelatedProperty|) so this can return reasons
  // that the LayoutObject does not end up using.
  static CompositingReasons PotentialCompositingReasonsFor3DTransform(
      const ComputedStyle&);
  static CompositingReasons CompositingReasonsFor3DTransform(
      const LayoutObject&);
  static bool RequiresCompositingForRootScroller(const PaintLayer&);

  static bool RequiresCompositingForScrollDependentPosition(const PaintLayer&);

  static bool RequiresCompositingForAffectedByOuterViewportBoundsDelta(
      const LayoutObject&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROUNDED_BORDER_GEOMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROUNDED_BORDER_GEOMETRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/box_sides.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
class FloatRoundedRect;
struct PhysicalBoxStrut;
struct PhysicalRect;

class CORE_EXPORT RoundedBorderGeometry {
  STATIC_ONLY(RoundedBorderGeometry);

 public:
  static FloatRoundedRect RoundedBorder(const ComputedStyle&,
                                        const PhysicalRect& border_rect);

  static FloatRoundedRect PixelSnappedRoundedBorder(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      PhysicalBoxSides edges_to_include = PhysicalBoxSides());

  static FloatRoundedRect RoundedInnerBorder(const ComputedStyle&,
                                             const PhysicalRect& border_rect);

  static FloatRoundedRect PixelSnappedRoundedInnerBorder(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      PhysicalBoxSides edges_to_include = PhysicalBoxSides());

  // Values in |outsets| must be either all >= 0 to expand from |border_rect|,
  // or all <= 0 to shrink from |border_rect|.
  static FloatRoundedRect PixelSnappedRoundedBorderWithOutsets(
      const ComputedStyle&,
      const PhysicalRect& border_rect,
      const PhysicalBoxStrut& outsets_from_border,
      PhysicalBoxSides edges_to_include = PhysicalBoxSides());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ROUNDED_BORDER_GEOMETRY_H_

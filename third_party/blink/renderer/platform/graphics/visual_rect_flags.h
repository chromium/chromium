// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VISUAL_RECT_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VISUAL_RECT_FLAGS_H_

namespace blink {

enum VisualRectFlags {
  // The following flags are used in both
  // LayoutObject::MapToVisualRectInAncestorSpace() and
  // GeometryMapper::LocalToAncestorVisualRect().

  kDefaultVisualRectFlags = 0,
  // Use gfx::RectF::InclusiveIntersect instead of gfx::RectF::Intersect for
  // intersection.
  kEdgeInclusive = 1 << 0,
  // Don't expand visual rect for pixel-moving filters.
  kIgnoreFilters = 1 << 1,

  // The following flags are used in
  // LayoutObject::MapToVisualRectInAncestorSpace() only.

  // Use the GeometryMapper fast-path, if possible.
  kUseGeometryMapper = 1 << 2,
  // When mapping to absolute coordinates and the main frame is remote, don't
  // apply the main frame root scroller's overflow clip.
  kDontApplyMainFrameOverflowClip = 1 << 3,
  kIgnoreLocalClipPath = 1 << 4,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_VISUAL_RECT_FLAGS_H_

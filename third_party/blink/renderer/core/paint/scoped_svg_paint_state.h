/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 Zoltan Herczeg <zherczeg@webkit.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_SVG_PAINT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_SVG_PAINT_STATE_H_

#include <optional>

#include "base/containers/enum_set.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class ObjectPaintProperties;

// Hooks up the correct paint property transform node.
class ScopedSVGTransformState {
  STACK_ALLOCATED();

 public:
  ScopedSVGTransformState(const PaintInfo& paint_info,
                          const LayoutObject& object);

  PaintInfo& ContentPaintInfo() { return content_paint_info_; }

 private:
  std::optional<SvgContextPaints> transformed_context_paints_;
  std::optional<ScopedPaintChunkProperties> transform_property_scope_;
  PaintInfo content_paint_info_;
};

class ScopedSVGPaintState {
  STACK_ALLOCATED();

 public:
  // Flags representing the components that should be painted for an SVG layout
  // object.
  enum class PaintComponent {
    // Set if the SVG object has visible content (non-visibility hidden shapes
    // or an image for leaf objects, children for containers, etc.) to paint.
    // When this flag is not set, the object has no visible content to paint
    // but may still have a reference filter. Note that this is different from
    // the spec concept of "disabled rendering" which should be handled before
    // PaintComponent usage in the paint pipeline and can also disable filter
    // painting.
    kContent,

    // Set if the SVG object may have a reference filter to paint. Note that
    // this flag can be set even if there is no filter because it's assumed
    // that any object with `has_content`, as specified in ComputePaintBehavior,
    // may also have a reference filter. When this flag is not set, either the
    // object has no reference filter or the reference filter is painted by
    // other code such as PaintLayerPainter.
    kReferenceFilter,

    kMinValue = kContent,
    kMaxValue = kReferenceFilter
  };

  using PaintBehavior = base::EnumSet<PaintComponent,
                                      PaintComponent::kMinValue,
                                      PaintComponent::kMaxValue>;

  ScopedSVGPaintState(const LayoutObject& object,
                      const PaintInfo& paint_info,
                      PaintBehavior paint_behavior);
  ScopedSVGPaintState(const LayoutObject& object,
                      const PaintInfo& paint_info,
                      const DisplayItemClient& display_item_client,
                      PaintBehavior paint_behavior);
  ~ScopedSVGPaintState();

  // Returns the PaintBehavior for the given object and paint info. Pass
  // `has_content` as true if the object has visible content (e.g. leaf object
  // with a visible shape or image, container with children). Filters applied to
  // the object do not count as content and are handled separately. Note that
  // the spec concept of "disabled rendering" is not the same as has_content ==
  // false and should be handled earlier in the painting pipeline. See
  // `PaintComponent` enum for more details.
  static PaintBehavior ComputePaintBehavior(const LayoutObject& object,
                                            const PaintInfo& paint_info,
                                            bool has_content);

 private:
  void ApplyEffects();
  void ApplyPaintPropertyState(const ObjectPaintProperties&);

  const LayoutObject& object_;
  const PaintInfo& paint_info_;
  const DisplayItemClient& display_item_client_;
  std::optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties_;
  const PaintBehavior paint_behavior_;
  bool should_paint_mask_ = false;
  bool should_paint_clip_path_as_mask_image_ = false;
#if DCHECK_IS_ON()
  bool apply_effects_called_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_SVG_PAINT_STATE_H_

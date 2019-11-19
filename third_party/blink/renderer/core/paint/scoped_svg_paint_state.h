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

#include <memory>
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/svg_filter_painter.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

class LayoutObject;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMasker;
class SVGResources;

// Hooks up the correct paint property transform node.
class ScopedSVGTransformState {
  STACK_ALLOCATED();

 public:
  ScopedSVGTransformState(const PaintInfo& paint_info,
                          const LayoutObject& object,
                          const AffineTransform& transform) {
    DCHECK(object.IsSVGChild());

    const auto* fragment = paint_info.FragmentToPaint(object);
    if (!fragment)
      return;
    const auto* properties = fragment->PaintProperties();
    if (!properties)
      return;

    if (const auto* transform_node = properties->Transform()) {
#if DCHECK_IS_ON()
      if (transform_node->IsIdentityOr2DTranslation()) {
        DCHECK_EQ(transform_node->Translation2D(),
                  transform.ToTransformationMatrix().To2DTranslation());
      } else {
        DCHECK_EQ(transform_node->Matrix(), transform.ToTransformationMatrix());
      }
#endif
      transform_property_scope_.emplace(
          paint_info.context.GetPaintController(), *transform_node, object,
          DisplayItem::PaintPhaseToSVGTransformType(paint_info.phase));
    }
  }

 private:
  base::Optional<ScopedPaintChunkProperties> transform_property_scope_;
};

class ScopedSVGPaintState {
  STACK_ALLOCATED();

 public:
  ScopedSVGPaintState(const LayoutObject& object, const PaintInfo& paint_info)
      : object_(object),
        paint_info_(paint_info),
        filter_(nullptr),
        masker_(nullptr) {}

  ~ScopedSVGPaintState();

  PaintInfo& GetPaintInfo() {
    return filter_paint_info_ ? *filter_paint_info_ : paint_info_;
  }

  // Return true if these operations aren't necessary or if they are
  // successfully applied.
  bool ApplyClipMaskAndFilterIfNecessary();

 private:
  void ApplyPaintPropertyState();
  void ApplyClipIfNecessary();

  // Return true if no masking is necessary or if the mask is successfully
  // applied.
  bool ApplyMaskIfNecessary(SVGResources*);

  // Return true if no filtering is necessary or if the filter is successfully
  // applied.
  bool ApplyFilterIfNecessary(SVGResources*);

  const LayoutObject& object_;
  PaintInfo paint_info_;
  std::unique_ptr<PaintInfo> filter_paint_info_;
  LayoutSVGResourceFilter* filter_;
  LayoutSVGResourceMasker* masker_;
  base::Optional<ClipPathClipper> clip_path_clipper_;
  std::unique_ptr<SVGFilterRecordingContext> filter_recording_context_;
  base::Optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties_;
#if DCHECK_IS_ON()
  bool apply_clip_mask_and_filter_if_necessary_called_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_SVG_PAINT_STATE_H_

/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_masker.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_mask_element.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

LayoutSVGResourceMasker::LayoutSVGResourceMasker(SVGMaskElement* node)
    : LayoutSVGResourceContainer(node) {}

LayoutSVGResourceMasker::~LayoutSVGResourceMasker() = default;

void LayoutSVGResourceMasker::RemoveAllClientsFromCache() {
  NOT_DESTROYED();
  cached_paint_record_ = std::nullopt;
  MarkAllClientsForInvalidation(kPaintPropertiesInvalidation |
                                kPaintInvalidation);
}

PaintRecord LayoutSVGResourceMasker::CreatePaintRecord() {
  NOT_DESTROYED();
  if (cached_paint_record_)
    return *cached_paint_record_;

  PaintRecordBuilder builder;
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    const LayoutObject* layout_object = child_element.GetLayoutObject();
    if (!layout_object)
      continue;
    if (DisplayLockUtilities::LockedAncestorPreventingLayout(*layout_object) ||
        layout_object->StyleRef().Display() == EDisplay::kNone)
      continue;
    SVGObjectPainter(*layout_object, nullptr)
        .PaintResourceSubtree(builder.Context(), PaintFlag::kPaintingSVGMask);
  }

  cached_paint_record_ = builder.EndRecording();
  return *cached_paint_record_;
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceMasker::MaskUnits() const {
  NOT_DESTROYED();
  return To<SVGMaskElement>(GetElement())->maskUnits()->CurrentEnumValue();
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceMasker::MaskContentUnits() const {
  NOT_DESTROYED();
  return To<SVGMaskElement>(GetElement())
      ->maskContentUnits()
      ->CurrentEnumValue();
}

gfx::RectF LayoutSVGResourceMasker::ResourceBoundingBox(
    const gfx::RectF& reference_box,
    float reference_box_zoom) {
  NOT_DESTROYED();
  DCHECK(!SelfNeedsFullLayout());
  auto* mask_element = To<SVGMaskElement>(GetElement());
  DCHECK(mask_element);

  const SVGUnitTypes::SVGUnitType mask_units = MaskUnits();
  gfx::RectF mask_boundaries = ResolveRectangle(
      mask_units, reference_box, *mask_element->x()->CurrentValue(),
      *mask_element->y()->CurrentValue(),
      *mask_element->width()->CurrentValue(),
      *mask_element->height()->CurrentValue());
  // If the mask bounds were resolved relative to the current userspace we need
  // to adjust/scale with the zoom to get to the same space as the reference
  // box.
  if (mask_units == SVGUnitTypes::kSvgUnitTypeUserspaceonuse) {
    mask_boundaries.Scale(reference_box_zoom);
  }
  return mask_boundaries;
}

}  // namespace blink

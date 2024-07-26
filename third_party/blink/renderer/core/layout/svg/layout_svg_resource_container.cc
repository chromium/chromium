/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"

#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/style/reference_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/style_mask_source_image.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

LocalSVGResource* ResourceForContainer(
    const LayoutSVGResourceContainer& resource_container) {
  const SVGElement& element = *resource_container.GetElement();
  return element.GetTreeScope()
      .EnsureSVGTreeScopedResources()
      .ExistingResourceForId(element.GetIdAttribute());
}

float ObjectBoundingBoxUnitToUserUnits(const Length& length,
                                       float ref_dimension) {
  // For "plain" percentages we resolve against the real reference dimension
  // and scale with the unit dimension to avoid losing precision for common
  // cases. In essence because of the difference between:
  //
  //   v * percentage / 100
  //
  // and:
  //
  //   v * (percentage / 100)
  //
  // for certain, common, values of v and percentage.
  float unit_dimension = 1;
  if (length.IsPercent()) {
    std::swap(unit_dimension, ref_dimension);
  }
  return FloatValueForLength(length, unit_dimension) * ref_dimension;
}

}  // namespace

LayoutSVGResourceContainer::LayoutSVGResourceContainer(SVGElement* node)
    : LayoutSVGHiddenContainer(node),
      completed_invalidations_mask_(0),
      is_invalidating_(false) {}

LayoutSVGResourceContainer::~LayoutSVGResourceContainer() = default;

SVGLayoutResult LayoutSVGResourceContainer::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();
  // TODO(fs): This is only here to clear the invalidation mask, without that
  // we wouldn't need to override LayoutSVGHiddenContainer::UpdateSVGLayout().
  DCHECK(NeedsLayout());
  ClearInvalidationMask();
  return LayoutSVGHiddenContainer::UpdateSVGLayout(layout_info);
}

gfx::RectF LayoutSVGResourceContainer::ResolveRectangle(
    const SVGViewportResolver& viewport_resolver,
    const SVGLengthConversionData& conversion_data,
    SVGUnitTypes::SVGUnitType type,
    const gfx::RectF& reference_box,
    const SVGLength& x,
    const SVGLength& y,
    const SVGLength& width,
    const SVGLength& height,
    const std::optional<gfx::SizeF>& override_viewport) {
  // Convert SVGLengths to Lengths (preserves percentages).
  const LengthPoint point(x.ConvertToLength(conversion_data),
                          y.ConvertToLength(conversion_data));
  const LengthSize size(width.ConvertToLength(conversion_data),
                        height.ConvertToLength(conversion_data));
  gfx::RectF resolved_rect;
  // If the requested unit is 'objectBoundingBox' then the resolved user units
  // are actually normalized (in bounding box units), so transform them to the
  // actual user space.
  if (type == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    // Resolve the Lengths to user units.
    resolved_rect = gfx::RectF(
        ObjectBoundingBoxUnitToUserUnits(point.X(), reference_box.width()),
        ObjectBoundingBoxUnitToUserUnits(point.Y(), reference_box.height()),
        ObjectBoundingBoxUnitToUserUnits(size.Width(), reference_box.width()),
        ObjectBoundingBoxUnitToUserUnits(size.Height(),
                                         reference_box.height()));
    resolved_rect += reference_box.OffsetFromOrigin();
  } else {
    DCHECK_EQ(type, SVGUnitTypes::kSvgUnitTypeUserspaceonuse);
    // Determine the viewport to use for resolving the Lengths to user units.
    gfx::SizeF viewport_size_for_resolve;
    if (size.Width().MayHavePercentDependence() ||
        size.Height().MayHavePercentDependence() || point.X().HasPercent() ||
        point.Y().HasPercent()) {
      viewport_size_for_resolve =
          override_viewport.value_or(viewport_resolver.ResolveViewport());
    }
    // Resolve the Lengths to user units.
    resolved_rect =
        gfx::RectF(PointForLengthPoint(point, viewport_size_for_resolve),
                   SizeForLengthSize(size, viewport_size_for_resolve));
  }
  return resolved_rect;
}

gfx::RectF LayoutSVGResourceContainer::ResolveRectangle(
    const SVGElement& context_element,
    SVGUnitTypes::SVGUnitType type,
    const gfx::RectF& reference_box,
    const SVGLength& x,
    const SVGLength& y,
    const SVGLength& width,
    const SVGLength& height,
    const std::optional<gfx::SizeF>& override_viewport) {
  const ComputedStyle* style =
      SVGLengthContext::ComputedStyleForLengthResolving(context_element);
  if (!style) {
    return gfx::RectF(0, 0, 0, 0);
  }
  const SVGViewportResolver viewport_resolver(context_element);
  const SVGLengthConversionData conversion_data(context_element, *style);
  return ResolveRectangle(viewport_resolver, conversion_data, type,
                          reference_box, x, y, width, height,
                          override_viewport);
}

gfx::RectF LayoutSVGResourceContainer::ResolveRectangle(
    SVGUnitTypes::SVGUnitType type,
    const gfx::RectF& reference_box,
    const SVGLength& x,
    const SVGLength& y,
    const SVGLength& width,
    const SVGLength& height) const {
  const SVGViewportResolver viewport_resolver(*this);
  const SVGLengthConversionData conversion_data(*this);
  return ResolveRectangle(viewport_resolver, conversion_data, type,
                          reference_box, x, y, width, height);
}

void LayoutSVGResourceContainer::InvalidateClientsIfActiveResource() {
  NOT_DESTROYED();
  // Avoid doing unnecessary work if the document is being torn down.
  if (DocumentBeingDestroyed())
    return;
  // If this is the 'active' resource (the first element with the specified 'id'
  // in tree order), notify any clients that they need to reevaluate the
  // resource's contents.
  LocalSVGResource* resource = ResourceForContainer(*this);
  if (!resource || resource->Target() != GetElement())
    return;
  GetDocument().ScheduleSVGResourceInvalidation(*resource);
}

void LayoutSVGResourceContainer::WillBeDestroyed() {
  NOT_DESTROYED();
  // The resource is being torn down.
  InvalidateClientsIfActiveResource();
  LayoutSVGHiddenContainer::WillBeDestroyed();
}

void LayoutSVGResourceContainer::StyleDidChange(
    StyleDifference diff,
    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGHiddenContainer::StyleDidChange(diff, old_style);
  if (old_style)
    return;
  // The resource has been attached.
  InvalidateClientsIfActiveResource();
}

bool LayoutSVGResourceContainer::FindCycle() const {
  NOT_DESTROYED();
  return FindCycleFromSelf();
}

static HeapVector<Member<SVGResource>> CollectResources(
    const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  HeapVector<Member<SVGResource>> resources;
  if (auto* reference_clip =
          DynamicTo<ReferenceClipPathOperation>(style.ClipPath())) {
    resources.push_back(reference_clip->Resource());
  }
  for (const auto& operation : style.Filter().Operations()) {
    if (auto* reference_operation =
            DynamicTo<ReferenceFilterOperation>(*operation))
      resources.push_back(reference_operation->Resource());
  }
  if (auto* marker = style.MarkerStartResource())
    resources.push_back(marker->Resource());
  if (auto* marker = style.MarkerMidResource())
    resources.push_back(marker->Resource());
  if (auto* marker = style.MarkerEndResource())
    resources.push_back(marker->Resource());
  if (auto* paint_resource = style.FillPaint().Resource())
    resources.push_back(paint_resource->Resource());
  if (auto* paint_resource = style.StrokePaint().Resource())
    resources.push_back(paint_resource->Resource());
  return resources;
}

bool LayoutSVGResourceContainer::FindCycleInResources(
    const LayoutObject& layout_object) {
  if (!layout_object.IsSVG() || layout_object.IsText())
    return false;
  // Without an associated client, we will not reference any resources.
  if (SVGResourceClient* client = SVGResources::GetClient(layout_object)) {
    // Fetch all the referenced resources.
    HeapVector<Member<SVGResource>> resources = CollectResources(layout_object);
    // This performs a depth-first search for a back-edge in all the
    // (potentially disjoint) graphs formed by the referenced resources.
    for (const auto& local_resource : resources) {
      // The resource can be null if the reference is external but external
      // references are not allowed.
      if (local_resource && local_resource->FindCycle(*client)) {
        return true;
      }
    }
  }
  for (const FillLayer* layer = &layout_object.StyleRef().MaskLayers(); layer;
       layer = layer->Next()) {
    const auto* mask_source =
        DynamicTo<StyleMaskSourceImage>(layer->GetImage());
    if (!mask_source) {
      continue;
    }
    const SVGResource* svg_resource = mask_source->GetSVGResource();
    SVGResourceClient* client =
        mask_source->GetSVGResourceClient(layout_object);
    if (svg_resource && svg_resource->FindCycle(*client)) {
      return true;
    }
  }
  return false;
}

bool LayoutSVGResourceContainer::FindCycleFromSelf() const {
  NOT_DESTROYED();
  // Resources don't generally apply to other resources, so require
  // the specific cases that do (like <clipPath>) to implement an
  // override.
  return FindCycleInDescendants(*this);
}

bool LayoutSVGResourceContainer::FindCycleInDescendants(
    const LayoutObject& root) {
  LayoutObject* node = root.SlowFirstChild();
  while (node) {
    // Skip subtrees which are themselves resources. (They will be
    // processed - if needed - when they are actually referenced.)
    if (node->IsSVGResourceContainer()) {
      node = node->NextInPreOrderAfterChildren(&root);
      continue;
    }
    if (FindCycleInResources(*node))
      return true;
    node = node->NextInPreOrder(&root);
  }
  return false;
}

bool LayoutSVGResourceContainer::FindCycleInSubtree(
    const LayoutObject& root) {
  if (FindCycleInResources(root))
    return true;
  return FindCycleInDescendants(root);
}

void LayoutSVGResourceContainer::MarkAllClientsForInvalidation(
    InvalidationModeMask invalidation_mask) {
  NOT_DESTROYED();
  if (is_invalidating_)
    return;
  LocalSVGResource* resource = ResourceForContainer(*this);
  if (!resource || resource->Target() != GetElement())
    return;
  // Remove modes for which invalidations have already been
  // performed. If no modes remain we are done.
  invalidation_mask &= ~completed_invalidations_mask_;
  if (invalidation_mask == 0)
    return;
  completed_invalidations_mask_ |= invalidation_mask;

  auto& document = GetDocument();
  if (document.InStyleRecalc() ||
      document.GetStyleEngine().InDetachLayoutTree()) {
    document.ScheduleSVGResourceInvalidation(*resource);
  } else {
    is_invalidating_ = true;

    // Invalidate clients registered via an SVGResource.
    resource->NotifyContentChanged();

    is_invalidating_ = false;
  }
}

void LayoutSVGResourceContainer::InvalidateCache() {
  NOT_DESTROYED();
  if (EverHadLayout()) {
    RemoveAllClientsFromCache();
  }
}

static inline void RemoveFromCacheAndInvalidateDependencies(
    LayoutObject& object,
    bool needs_layout) {
  if (!RuntimeEnabledFeatures::SvgTransformOptimizationEnabled()) {
    if (object.IsSVG()) {
      SVGResourceInvalidator(object).InvalidateEffects();
    }
  }

  LayoutSVGResourceContainer::InvalidateDependentElements(object, needs_layout);
}

void LayoutSVGResourceContainer::InvalidateDependentElements(
    LayoutObject& object,
    bool needs_layout) {
  auto* element = DynamicTo<SVGElement>(object.GetNode());
  if (!element)
    return;
  element->NotifyIncomingReferences([needs_layout](SVGElement& element) {
    DCHECK(element.GetLayoutObject());
    LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
        *element.GetLayoutObject(), needs_layout);
  });
}

void LayoutSVGResourceContainer::InvalidateAncestorChainResources(
    LayoutObject& object,
    bool needs_layout) {
  LayoutObject* current = object.Parent();
  while (current) {
    RemoveFromCacheAndInvalidateDependencies(*current, needs_layout);

    if (current->IsSVGResourceContainer()) {
      // This will process the rest of the ancestors.
      To<LayoutSVGResourceContainer>(current)->RemoveAllClientsFromCache();
      break;
    }

    current = current->Parent();
  }
}

void LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(
    LayoutObject& object,
    bool needs_layout) {
  DCHECK(object.GetNode());

  if (needs_layout && !object.DocumentBeingDestroyed()) {
    object.SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSvgResourceInvalidated);
  }

  RemoveFromCacheAndInvalidateDependencies(object, needs_layout);
  InvalidateAncestorChainResources(object, needs_layout);
}

static inline bool IsLayoutObjectOfResourceContainer(
    const LayoutObject& layout_object) {
  const LayoutObject* current = &layout_object;
  while (current) {
    if (current->IsSVGResourceContainer())
      return true;
    current = current->Parent();
  }
  return false;
}

void LayoutSVGResourceContainer::StyleChanged(LayoutObject& object,
                                              StyleDifference diff) {
  // If this LayoutObject is the child of a resource container and
  // it requires repainting because of changes to CSS properties
  // such as 'visibility', upgrade to invalidate layout.
  bool needs_layout = diff.NeedsNormalPaintInvalidation() &&
                      IsLayoutObjectOfResourceContainer(object);
  MarkForLayoutAndParentResourceInvalidation(object, needs_layout);
}

}  // namespace blink

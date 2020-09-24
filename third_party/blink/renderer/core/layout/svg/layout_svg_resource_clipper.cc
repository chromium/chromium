/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_clipper.h"

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/svg/svg_clip_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"
#include "third_party/blink/renderer/core/svg/svg_use_element.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

namespace blink {

namespace {

enum class ClipStrategy { kNone, kMask, kPath };

ClipStrategy ModifyStrategyForClipPath(const ComputedStyle& style,
                                       ClipStrategy strategy) {
  // If the shape in the clip-path gets clipped too then fallback to masking.
  if (strategy != ClipStrategy::kPath || !style.ClipPath())
    return strategy;
  return ClipStrategy::kMask;
}

ClipStrategy DetermineClipStrategy(const SVGGraphicsElement& element) {
  const LayoutObject* layout_object = element.GetLayoutObject();
  if (!layout_object)
    return ClipStrategy::kNone;
  const ComputedStyle& style = layout_object->StyleRef();
  if (style.Display() == EDisplay::kNone ||
      style.Visibility() != EVisibility::kVisible)
    return ClipStrategy::kNone;
  ClipStrategy strategy = ClipStrategy::kNone;
  // Only shapes, paths and texts are allowed for clipping.
  if (layout_object->IsSVGShape()) {
    strategy = ClipStrategy::kPath;
  } else if (layout_object->IsSVGText()) {
    // Text requires masking.
    strategy = ClipStrategy::kMask;
  }
  return ModifyStrategyForClipPath(style, strategy);
}

ClipStrategy DetermineClipStrategy(const SVGElement& element) {
  // <use> within <clipPath> have a restricted content model.
  // (https://drafts.fxtf.org/css-masking/#ClipPathElement)
  if (auto* svg_use_element = DynamicTo<SVGUseElement>(element)) {
    const LayoutObject* use_layout_object = element.GetLayoutObject();
    if (!use_layout_object ||
        use_layout_object->StyleRef().Display() == EDisplay::kNone)
      return ClipStrategy::kNone;
    const SVGGraphicsElement* shape_element =
        svg_use_element->VisibleTargetGraphicsElementForClipping();
    if (!shape_element)
      return ClipStrategy::kNone;
    ClipStrategy shape_strategy = DetermineClipStrategy(*shape_element);
    return ModifyStrategyForClipPath(use_layout_object->StyleRef(),
                                     shape_strategy);
  }
  auto* svg_graphics_element = DynamicTo<SVGGraphicsElement>(element);
  if (!svg_graphics_element)
    return ClipStrategy::kNone;
  return DetermineClipStrategy(*svg_graphics_element);
}

bool ContributesToClip(const SVGElement& element) {
  return DetermineClipStrategy(element) != ClipStrategy::kNone;
}

Path PathFromElement(const SVGElement& element) {
  if (auto* geometry_element = DynamicTo<SVGGeometryElement>(element))
    return geometry_element->ToClipPath();

  // Guaranteed by DetermineClipStrategy() above, only <use> element and
  // SVGGraphicsElement that has a LayoutSVGShape can reach here.
  return To<SVGUseElement>(element).ToClipPath();
}

}  // namespace

LayoutSVGResourceClipper::LayoutSVGResourceClipper(SVGClipPathElement* node)
    : LayoutSVGResourceContainer(node) {}

LayoutSVGResourceClipper::~LayoutSVGResourceClipper() = default;

void LayoutSVGResourceClipper::RemoveAllClientsFromCache() {
  clip_content_path_validity_ = kClipContentPathUnknown;
  clip_content_path_.Clear();
  cached_paint_record_.reset();
  local_clip_bounds_ = FloatRect();
  MarkAllClientsForInvalidation(SVGResourceClient::kLayoutInvalidation |
                                SVGResourceClient::kBoundariesInvalidation);
}

base::Optional<Path> LayoutSVGResourceClipper::AsPath() {
  if (clip_content_path_validity_ == kClipContentPathValid)
    return base::Optional<Path>(clip_content_path_);
  if (clip_content_path_validity_ == kClipContentPathInvalid)
    return base::nullopt;
  DCHECK_EQ(clip_content_path_validity_, kClipContentPathUnknown);

  clip_content_path_validity_ = kClipContentPathInvalid;
  // If the current clip-path gets clipped itself, we have to fallback to
  // masking.
  if (StyleRef().ClipPath())
    return base::nullopt;

  unsigned op_count = 0;
  base::Optional<SkOpBuilder> clip_path_builder;
  SkPath resolved_path;
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    ClipStrategy strategy = DetermineClipStrategy(child_element);
    if (strategy == ClipStrategy::kNone)
      continue;
    if (strategy == ClipStrategy::kMask)
      return base::nullopt;

    // Multiple shapes require PathOps. In some degenerate cases PathOps can
    // exhibit quadratic behavior, so we cap the number of ops to a reasonable
    // count.
    const unsigned kMaxOps = 42;
    if (++op_count > kMaxOps)
      return base::nullopt;
    if (clip_path_builder) {
      clip_path_builder->add(PathFromElement(child_element).GetSkPath(),
                             kUnion_SkPathOp);
    } else if (resolved_path.isEmpty()) {
      resolved_path = PathFromElement(child_element).GetSkPath();
    } else {
      clip_path_builder.emplace();
      clip_path_builder->add(std::move(resolved_path), kUnion_SkPathOp);
      clip_path_builder->add(PathFromElement(child_element).GetSkPath(),
                             kUnion_SkPathOp);
    }
  }

  if (clip_path_builder)
    clip_path_builder->resolve(&resolved_path);
  clip_content_path_ = std::move(resolved_path);
  clip_content_path_validity_ = kClipContentPathValid;
  return base::Optional<Path>(clip_content_path_);
}

sk_sp<const PaintRecord> LayoutSVGResourceClipper::CreatePaintRecord() {
  DCHECK(GetFrame());
  if (cached_paint_record_)
    return cached_paint_record_;

  PaintRecordBuilder builder(nullptr, nullptr);
  // Switch to a paint behavior where all children of this <clipPath> will be
  // laid out using special constraints:
  // - fill-opacity/stroke-opacity/opacity set to 1
  // - masker/filter not applied when laying out the children
  // - fill is set to the initial fill paint server (solid, black)
  // - stroke is set to the initial stroke paint server (none)
  PaintInfo info(builder.Context(), LayoutRect::InfiniteIntRect(),
                 PaintPhase::kForeground, kGlobalPaintNormalPhase,
                 kPaintLayerPaintingRenderingClipPathAsMask |
                     kPaintLayerPaintingRenderingResourceSubtree);

  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    // Use the LayoutObject of the direct child even if it is a <use>. In that
    // case, we will paint the targeted element indirectly.
    const LayoutObject* layout_object = child_element.GetLayoutObject();
    layout_object->Paint(info);
  }

  cached_paint_record_ = builder.EndRecording();
  return cached_paint_record_;
}

void LayoutSVGResourceClipper::CalculateLocalClipBounds() {
  // This is a rough heuristic to appraise the clip size and doesn't consider
  // clip on clip.
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    const LayoutObject* layout_object = child_element.GetLayoutObject();
    local_clip_bounds_.Unite(layout_object->LocalToSVGParentTransform().MapRect(
        layout_object->VisualRectInLocalSVGCoordinates()));
  }
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceClipper::ClipPathUnits() const {
  return To<SVGClipPathElement>(GetElement())
      ->clipPathUnits()
      ->CurrentEnumValue();
}

AffineTransform LayoutSVGResourceClipper::CalculateClipTransform(
    const FloatRect& reference_box) const {
  AffineTransform transform =
      To<SVGClipPathElement>(GetElement())
          ->CalculateTransform(SVGElement::kIncludeMotionTransform);
  if (ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    transform.Translate(reference_box.X(), reference_box.Y());
    transform.ScaleNonUniform(reference_box.Width(), reference_box.Height());
  }
  return transform;
}

bool LayoutSVGResourceClipper::HitTestClipContent(
    const FloatRect& object_bounding_box,
    const HitTestLocation& location) const {
  if (!SVGLayoutSupport::IntersectsClipPath(*this, object_bounding_box,
                                            location))
    return false;

  TransformedHitTestLocation local_location(
      location, CalculateClipTransform(object_bounding_box));
  if (!local_location)
    return false;

  HitTestResult result(HitTestRequest::kSVGClipContent, *local_location);
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    LayoutObject* layout_object = child_element.GetLayoutObject();

    DCHECK(!layout_object->IsBoxModelObject() ||
           !ToLayoutBoxModelObject(layout_object)->HasSelfPaintingLayer());

    if (layout_object->NodeAtPoint(result, *local_location, PhysicalOffset(),
                                   kHitTestForeground))
      return true;
  }
  return false;
}

FloatRect LayoutSVGResourceClipper::ResourceBoundingBox(
    const FloatRect& reference_box) {
  // The resource has not been layouted yet. Return the reference box.
  if (SelfNeedsLayout())
    return reference_box;

  if (local_clip_bounds_.IsEmpty())
    CalculateLocalClipBounds();

  return CalculateClipTransform(reference_box).MapRect(local_clip_bounds_);
}

void LayoutSVGResourceClipper::StyleDidChange(StyleDifference diff,
                                              const ComputedStyle* old_style) {
  LayoutSVGResourceContainer::StyleDidChange(diff, old_style);
  if (diff.TransformChanged()) {
    MarkAllClientsForInvalidation(SVGResourceClient::kBoundariesInvalidation |
                                  SVGResourceClient::kPaintInvalidation);
  }
}

void LayoutSVGResourceClipper::WillBeDestroyed() {
  MarkAllClientsForInvalidation(SVGResourceClient::kBoundariesInvalidation |
                                SVGResourceClient::kPaintInvalidation);
  LayoutSVGResourceContainer::WillBeDestroyed();
}

}  // namespace blink

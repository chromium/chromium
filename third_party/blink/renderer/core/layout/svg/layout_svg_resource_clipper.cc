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

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
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
  if (strategy != ClipStrategy::kPath || !style.HasClipPath())
    return strategy;
  return ClipStrategy::kMask;
}

ClipStrategy DetermineClipStrategy(const SVGGraphicsElement& element) {
  const LayoutObject* layout_object = element.GetLayoutObject();
  if (!layout_object)
    return ClipStrategy::kNone;
  if (DisplayLockUtilities::LockedAncestorPreventingLayout(*layout_object))
    return ClipStrategy::kNone;
  const ComputedStyle& style = layout_object->StyleRef();
  if (style.Display() == EDisplay::kNone ||
      style.UsedVisibility() != EVisibility::kVisible) {
    return ClipStrategy::kNone;
  }
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
    if (!use_layout_object)
      return ClipStrategy::kNone;
    if (DisplayLockUtilities::LockedAncestorPreventingLayout(
            *use_layout_object))
      return ClipStrategy::kNone;
    if (use_layout_object->StyleRef().Display() == EDisplay::kNone)
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
  NOT_DESTROYED();
  clip_content_path_validity_ = kClipContentPathUnknown;
  clip_content_path_.Clear();
  cached_paint_record_ = std::nullopt;
  local_clip_bounds_ = gfx::RectF();
  MarkAllClientsForInvalidation(kClipCacheInvalidation | kPaintInvalidation);
}

std::optional<Path> LayoutSVGResourceClipper::AsPath() {
  NOT_DESTROYED();
  if (clip_content_path_validity_ == kClipContentPathValid)
    return std::optional<Path>(clip_content_path_);
  if (clip_content_path_validity_ == kClipContentPathInvalid)
    return std::nullopt;
  DCHECK_EQ(clip_content_path_validity_, kClipContentPathUnknown);

  clip_content_path_validity_ = kClipContentPathInvalid;
  // If the current clip-path gets clipped itself, we have to fallback to
  // masking.
  if (StyleRef().HasClipPath())
    return std::nullopt;

  unsigned op_count = 0;
  std::optional<SkOpBuilder> clip_path_builder;
  SkPath resolved_path;
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    ClipStrategy strategy = DetermineClipStrategy(child_element);
    if (strategy == ClipStrategy::kNone)
      continue;
    if (strategy == ClipStrategy::kMask)
      return std::nullopt;

    // Multiple shapes require PathOps. In some degenerate cases PathOps can
    // exhibit quadratic behavior, so we cap the number of ops to a reasonable
    // count.
    const unsigned kMaxOps = 42;
    if (++op_count > kMaxOps)
      return std::nullopt;
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
  return std::optional<Path>(clip_content_path_);
}

PaintRecord LayoutSVGResourceClipper::CreatePaintRecord() {
  NOT_DESTROYED();
  DCHECK(GetFrame());
  if (cached_paint_record_)
    return *cached_paint_record_;

  PaintRecordBuilder builder;
  // Switch to a paint behavior where all children of this <clipPath> will be
  // laid out using special constraints:
  // - fill-opacity/stroke-opacity/opacity set to 1
  // - masker/filter not applied when laying out the children
  // - fill is set to the initial fill paint server (solid, black)
  // - stroke is set to the initial stroke paint server (none)
  PaintInfo info(
      builder.Context(), CullRect::Infinite(), PaintPhase::kForeground,
      ChildPaintBlockedByDisplayLock(),
      PaintFlag::kPaintingClipPathAsMask | PaintFlag::kPaintingResourceSubtree);

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
  return *cached_paint_record_;
}

void LayoutSVGResourceClipper::CalculateLocalClipBounds() {
  NOT_DESTROYED();
  // This is a rough heuristic to appraise the clip size and doesn't consider
  // clip on clip.
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    const LayoutObject* layout_object = child_element.GetLayoutObject();
    local_clip_bounds_.Union(layout_object->LocalToSVGParentTransform().MapRect(
        layout_object->VisualRectInLocalSVGCoordinates()));
  }
}

SVGUnitTypes::SVGUnitType LayoutSVGResourceClipper::ClipPathUnits() const {
  NOT_DESTROYED();
  return To<SVGClipPathElement>(GetElement())
      ->clipPathUnits()
      ->CurrentEnumValue();
}

AffineTransform LayoutSVGResourceClipper::CalculateClipTransform(
    const gfx::RectF& reference_box) const {
  NOT_DESTROYED();
  AffineTransform transform =
      To<SVGClipPathElement>(GetElement())
          ->CalculateTransform(SVGElement::kIncludeMotionTransform);
  if (ClipPathUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    transform.Translate(reference_box.x(), reference_box.y());
    transform.ScaleNonUniform(reference_box.width(), reference_box.height());
  }
  return transform;
}

bool LayoutSVGResourceClipper::HitTestClipContent(
    const gfx::RectF& reference_box,
    const LayoutObject& reference_box_object,
    const HitTestLocation& location) const {
  NOT_DESTROYED();
  if (HasClipPath() &&
      !ClipPathClipper::HitTest(*this, reference_box, reference_box_object,
                                location)) {
    return false;
  }

  TransformedHitTestLocation local_location(
      location, CalculateClipTransform(reference_box));
  if (!local_location)
    return false;

  HitTestResult result(HitTestRequest::kSVGClipContent, *local_location);
  for (const SVGElement& child_element :
       Traversal<SVGElement>::ChildrenOf(*GetElement())) {
    if (!ContributesToClip(child_element))
      continue;
    LayoutObject* layout_object = child_element.GetLayoutObject();

    DCHECK(!layout_object->IsBoxModelObject() ||
           !To<LayoutBoxModelObject>(layout_object)->HasSelfPaintingLayer());

    if (layout_object->NodeAtPoint(result, *local_location, PhysicalOffset(),
                                   HitTestPhase::kForeground))
      return true;
  }
  return false;
}

gfx::RectF LayoutSVGResourceClipper::ResourceBoundingBox(
    const gfx::RectF& reference_box) {
  NOT_DESTROYED();
  DCHECK(!SelfNeedsFullLayout());

  if (local_clip_bounds_.IsEmpty())
    CalculateLocalClipBounds();

  return CalculateClipTransform(reference_box).MapRect(local_clip_bounds_);
}

bool LayoutSVGResourceClipper::FindCycleFromSelf() const {
  NOT_DESTROYED();
  // Check nested clip-path.
  if (auto* reference_clip =
          DynamicTo<ReferenceClipPathOperation>(StyleRef().ClipPath())) {
    // The resource can be null if the reference is external but external
    // references are not allowed.
    if (SVGResource* resource = reference_clip->Resource()) {
      if (resource->FindCycle(*SVGResources::GetClient(*this)))
        return true;
    }
  }
  return LayoutSVGResourceContainer::FindCycleFromSelf();
}

void LayoutSVGResourceClipper::StyleDidChange(StyleDifference diff,
                                              const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGResourceContainer::StyleDidChange(diff, old_style);
  if (diff.TransformChanged())
    MarkAllClientsForInvalidation(kClipCacheInvalidation | kPaintInvalidation);
}

}  // namespace blink

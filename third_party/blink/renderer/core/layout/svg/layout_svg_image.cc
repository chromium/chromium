/*
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"

#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/svg_image_painter.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"

namespace blink {

LayoutSVGImage::LayoutSVGImage(SVGImageElement* impl)
    : LayoutSVGModelObject(impl),
      needs_transform_update_(true),
      transform_uses_reference_box_(false),
      image_resource_(MakeGarbageCollected<LayoutImageResource>()) {
  image_resource_->Initialize(this);
}

LayoutSVGImage::~LayoutSVGImage() = default;

void LayoutSVGImage::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  transform_uses_reference_box_ =
      TransformHelper::DependsOnReferenceBox(StyleRef());
  LayoutSVGModelObject::StyleDidChange(diff, old_style);
}

void LayoutSVGImage::WillBeDestroyed() {
  NOT_DESTROYED();
  image_resource_->Shutdown();

  LayoutSVGModelObject::WillBeDestroyed();
}

static float ResolveWidthForRatio(float height,
                                  const FloatSize& intrinsic_ratio) {
  return height * intrinsic_ratio.Width() / intrinsic_ratio.Height();
}

static float ResolveHeightForRatio(float width,
                                   const FloatSize& intrinsic_ratio) {
  return width * intrinsic_ratio.Height() / intrinsic_ratio.Width();
}

bool LayoutSVGImage::HasOverriddenIntrinsicSize() const {
  NOT_DESTROYED();
  if (!RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled())
    return false;
  auto* svg_image_element = DynamicTo<SVGImageElement>(GetElement());
  return svg_image_element && svg_image_element->IsDefaultIntrinsicSize();
}

FloatSize LayoutSVGImage::CalculateObjectSize() const {
  NOT_DESTROYED();
  FloatSize intrinsic_size;
  ImageResourceContent* cached_image = image_resource_->CachedImage();
  bool has_intrinsic_ratio = true;
  if (HasOverriddenIntrinsicSize()) {
    intrinsic_size = FloatSize(LayoutReplaced::kDefaultWidth,
                               LayoutReplaced::kDefaultHeight);
  } else {
    if (!cached_image || cached_image->ErrorOccurred() ||
        !cached_image->IsSizeAvailable())
      return object_bounding_box_.Size();

    RespectImageOrientationEnum respect_orientation =
        LayoutObject::ShouldRespectImageOrientation(this);
    intrinsic_size = cached_image->GetImage()->SizeAsFloat(respect_orientation);
    if (auto* svg_image = DynamicTo<SVGImage>(cached_image->GetImage())) {
      IntrinsicSizingInfo intrinsic_sizing_info;
      has_intrinsic_ratio &= svg_image->GetIntrinsicSizingInfo(intrinsic_sizing_info);
      has_intrinsic_ratio &= !intrinsic_sizing_info.aspect_ratio.IsEmpty();
    }
  }

  if (StyleRef().Width().IsAuto() && StyleRef().Height().IsAuto())
    return intrinsic_size;

  if (StyleRef().Height().IsAuto()) {
    if (has_intrinsic_ratio) {
      return FloatSize(
          object_bounding_box_.Width(),
          ResolveHeightForRatio(object_bounding_box_.Width(), intrinsic_size));
    }
    return FloatSize(object_bounding_box_.Width(), intrinsic_size.Height());
  }

  DCHECK(StyleRef().Width().IsAuto());
  if (has_intrinsic_ratio) {
    return FloatSize(
        ResolveWidthForRatio(object_bounding_box_.Height(), intrinsic_size),
        object_bounding_box_.Height());
  }

  return FloatSize(intrinsic_size.Width(), object_bounding_box_.Height());
}

bool LayoutSVGImage::UpdateBoundingBox() {
  NOT_DESTROYED();
  FloatRect old_object_bounding_box = object_bounding_box_;

  SVGLengthContext length_context(GetElement());
  const ComputedStyle& style = StyleRef();
  const SVGComputedStyle& svg_style = style.SvgStyle();
  object_bounding_box_ = FloatRect(
      length_context.ResolveLengthPair(svg_style.X(), svg_style.Y(), style),
      ToFloatSize(length_context.ResolveLengthPair(style.Width(),
                                                   style.Height(), style)));

  if (style.Width().IsAuto() || style.Height().IsAuto())
    object_bounding_box_.SetSize(CalculateObjectSize());

  return old_object_bounding_box != object_bounding_box_;
}

void LayoutSVGImage::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  const bool bbox_changed = UpdateBoundingBox();
  if (bbox_changed) {
    SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kImage);

    // Invalidate all resources of this client if our reference box changed.
    if (EverHadLayout())
      SVGResourceInvalidator(*this).InvalidateEffects();
  }

  if (!needs_transform_update_ && transform_uses_reference_box_) {
    needs_transform_update_ = CheckForImplicitTransformChange(bbox_changed);
    if (needs_transform_update_)
      SetNeedsPaintPropertyUpdate();
  }

  bool update_parent_boundaries = bbox_changed;
  if (needs_transform_update_) {
    local_transform_ = CalculateLocalTransform();
    needs_transform_update_ = false;
    update_parent_boundaries = true;
  }

  // If our bounds changed, notify the parents.
  if (update_parent_boundaries)
    LayoutSVGModelObject::SetNeedsBoundariesUpdate();

  DCHECK(!needs_transform_update_);

  if (auto* svg_image_element = DynamicTo<SVGImageElement>(GetElement())) {
    media_element_parser_helpers::CheckUnsizedMediaViolation(
        this, svg_image_element->IsDefaultIntrinsicSize());
  }
  ClearNeedsLayout();
}

void LayoutSVGImage::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  SVGImagePainter(*this).Paint(paint_info);
}

bool LayoutSVGImage::NodeAtPoint(HitTestResult& result,
                                 const HitTestLocation& hit_test_location,
                                 const PhysicalOffset& accumulated_offset,
                                 HitTestAction hit_test_action) {
  NOT_DESTROYED();
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  // We only draw in the forground phase, so we only hit-test then.
  if (hit_test_action != kHitTestForeground)
    return false;

  const ComputedStyle& style = StyleRef();
  PointerEventsHitRules hit_rules(PointerEventsHitRules::SVG_IMAGE_HITTESTING,
                                  result.GetHitTestRequest(),
                                  style.PointerEvents());
  if (hit_rules.require_visible && style.Visibility() != EVisibility::kVisible)
    return false;

  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (!SVGLayoutSupport::IntersectsClipPath(*this, object_bounding_box_,
                                            *local_location))
    return false;

  if (hit_rules.can_hit_fill || hit_rules.can_hit_bounding_box) {
    if (local_location->Intersects(object_bounding_box_)) {
      UpdateHitTestResult(result, PhysicalOffset::FromFloatPointRound(
                                      local_location->TransformedPoint()));
      if (result.AddNodeToListBasedTestResult(GetElement(), *local_location) ==
          kStopHitTesting)
        return true;
    }
  }
  return false;
}

void LayoutSVGImage::ImageChanged(WrappedImagePtr, CanDeferInvalidation defer) {
  NOT_DESTROYED();
  // Notify parent resources that we've changed. This also invalidates
  // references from resources (filters) that may have a cached
  // representation of this image/layout object.
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this,
                                                                         false);

  if (StyleRef().Width().IsAuto() || StyleRef().Height().IsAuto()) {
    if (CalculateObjectSize() != object_bounding_box_.Size())
      SetNeedsLayout(layout_invalidation_reason::kSizeChanged);
  }

  SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kImage);
}

}  // namespace blink

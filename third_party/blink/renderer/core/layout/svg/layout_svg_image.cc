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
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/svg_image_painter.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
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

void LayoutSVGImage::Trace(Visitor* visitor) const {
  visitor->Trace(image_resource_);
  LayoutSVGModelObject::Trace(visitor);
}

void LayoutSVGImage::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  TransformHelper::UpdateOffsetPath(*GetElement(), old_style);
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
                                  const gfx::SizeF& intrinsic_ratio) {
  return height * intrinsic_ratio.width() / intrinsic_ratio.height();
}

static float ResolveHeightForRatio(float width,
                                   const gfx::SizeF& intrinsic_ratio) {
  return width * intrinsic_ratio.height() / intrinsic_ratio.width();
}

bool LayoutSVGImage::HasOverriddenIntrinsicSize() const {
  NOT_DESTROYED();
  if (!RuntimeEnabledFeatures::ExperimentalPoliciesEnabled())
    return false;
  auto* svg_image_element = DynamicTo<SVGImageElement>(GetElement());
  return svg_image_element && svg_image_element->IsDefaultIntrinsicSize();
}

gfx::SizeF LayoutSVGImage::CalculateObjectSize() const {
  NOT_DESTROYED();

  gfx::Vector2dF style_size =
      SVGLengthContext(GetElement())
          .ResolveLengthPair(StyleRef().UsedWidth(), StyleRef().UsedHeight(),
                             StyleRef());
  bool width_is_auto = style_size.x() < 0 || StyleRef().UsedWidth().IsAuto();
  bool height_is_auto = style_size.y() < 0 || StyleRef().UsedHeight().IsAuto();
  if (!width_is_auto && !height_is_auto)
    return gfx::SizeF(style_size.x(), style_size.y());

  gfx::SizeF intrinsic_size;
  bool has_intrinsic_ratio = true;
  if (HasOverriddenIntrinsicSize()) {
    intrinsic_size.SetSize(LayoutReplaced::kDefaultWidth,
                           LayoutReplaced::kDefaultHeight);
  } else {
    ImageResourceContent* cached_image = image_resource_->CachedImage();
    if (!cached_image || cached_image->ErrorOccurred() ||
        !cached_image->IsSizeAvailable()) {
      return gfx::SizeF(style_size.x(), style_size.y());
    }

    RespectImageOrientationEnum respect_orientation =
        LayoutObject::ShouldRespectImageOrientation(this);
    intrinsic_size = cached_image->GetImage()->SizeAsFloat(respect_orientation);
    if (auto* svg_image = DynamicTo<SVGImage>(cached_image->GetImage())) {
      IntrinsicSizingInfo intrinsic_sizing_info;
      has_intrinsic_ratio &=
          svg_image->GetIntrinsicSizingInfo(intrinsic_sizing_info);
      has_intrinsic_ratio &= !intrinsic_sizing_info.aspect_ratio.IsEmpty();
    }
  }

  if (width_is_auto && height_is_auto)
    return intrinsic_size;

  if (height_is_auto) {
    if (has_intrinsic_ratio) {
      return gfx::SizeF(style_size.x(),
                        ResolveHeightForRatio(style_size.x(), intrinsic_size));
    }
    return gfx::SizeF(style_size.x(), intrinsic_size.height());
  }

  DCHECK(width_is_auto);
  if (has_intrinsic_ratio) {
    return gfx::SizeF(ResolveWidthForRatio(style_size.y(), intrinsic_size),
                      style_size.y());
  }
  return gfx::SizeF(intrinsic_size.width(), style_size.y());
}

bool LayoutSVGImage::UpdateBoundingBox() {
  NOT_DESTROYED();
  gfx::RectF old_object_bounding_box = object_bounding_box_;

  object_bounding_box_.set_origin(gfx::PointAtOffsetFromOrigin(
      SVGLengthContext(GetElement())
          .ResolveLengthPair(StyleRef().X(), StyleRef().Y(), StyleRef())));
  object_bounding_box_.set_size(CalculateObjectSize());

  return old_object_bounding_box != object_bounding_box_;
}

void LayoutSVGImage::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  const bool bbox_changed = UpdateBoundingBox();
  if (bbox_changed) {
    SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kSVGResource);

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
                                 HitTestPhase phase) {
  NOT_DESTROYED();
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  // We only draw in the foreground phase, so we only hit-test then.
  if (phase != HitTestPhase::kForeground)
    return false;

  const ComputedStyle& style = StyleRef();
  PointerEventsHitRules hit_rules(PointerEventsHitRules::kSvgImageHitTesting,
                                  result.GetHitTestRequest(),
                                  style.UsedPointerEvents());
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
      UpdateHitTestResult(result, PhysicalOffset::FromPointFRound(
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

  if (CalculateObjectSize() != object_bounding_box_.size())
    SetNeedsLayout(layout_invalidation_reason::kSizeChanged);

  SetShouldDoFullPaintInvalidationWithoutLayoutChange(
      PaintInvalidationReason::kImage);
}

}  // namespace blink

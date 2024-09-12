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

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intrinsic_sizing_info.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/svg_image_painter.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
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
      TransformHelper::UpdateReferenceBoxDependency(*this);
  LayoutSVGModelObject::StyleDidChange(diff, old_style);
}

void LayoutSVGImage::WillBeDestroyed() {
  NOT_DESTROYED();
  image_resource_->Shutdown();

  LayoutSVGModelObject::WillBeDestroyed();
}

gfx::SizeF LayoutSVGImage::CalculateObjectSize() const {
  NOT_DESTROYED();

  const SVGViewportResolver viewport_resolver(*this);
  gfx::Vector2dF style_size = VectorForLengthPair(
      StyleRef().Width(), StyleRef().Height(), viewport_resolver, StyleRef());
  // TODO(https://crbug.com/313072): This needs a bit of work to support
  // intrinsic keywords, calc-size(), etc. values for width and height.
  bool width_is_auto = style_size.x() < 0 || StyleRef().Width().IsAuto();
  bool height_is_auto = style_size.y() < 0 || StyleRef().Height().IsAuto();
  if (!width_is_auto && !height_is_auto)
    return gfx::SizeF(style_size.x(), style_size.y());

  const gfx::SizeF kDefaultObjectSize(LayoutReplaced::kDefaultWidth,
                                      LayoutReplaced::kDefaultHeight);
  IntrinsicSizingInfo sizing_info;
  if (!image_resource_->HasImage() || image_resource_->ErrorOccurred()) {
    return gfx::SizeF(style_size.x(), style_size.y());
  }
  sizing_info = image_resource_->GetNaturalDimensions(1);

  const gfx::SizeF concrete_object_size =
      ConcreteObjectSize(sizing_info, kDefaultObjectSize);
  if (width_is_auto && height_is_auto) {
    return concrete_object_size;
  }

  const bool has_intrinsic_ratio = !sizing_info.aspect_ratio.IsEmpty();
  if (height_is_auto) {
    if (has_intrinsic_ratio) {
      return gfx::SizeF(
          style_size.x(),
          ResolveHeightForRatio(style_size.x(), sizing_info.aspect_ratio));
    }
    return gfx::SizeF(style_size.x(), concrete_object_size.height());
  }

  DCHECK(width_is_auto);
  if (has_intrinsic_ratio) {
    return gfx::SizeF(
        ResolveWidthForRatio(style_size.y(), sizing_info.aspect_ratio),
        style_size.y());
  }
  return gfx::SizeF(concrete_object_size.width(), style_size.y());
}

bool LayoutSVGImage::UpdateBoundingBox() {
  NOT_DESTROYED();
  gfx::RectF old_object_bounding_box = object_bounding_box_;

  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = StyleRef();
  object_bounding_box_.set_origin(
      PointForLengthPair(style.X(), style.Y(), viewport_resolver, style));
  object_bounding_box_.set_size(CalculateObjectSize());

  return old_object_bounding_box != object_bounding_box_;
}

SVGLayoutResult LayoutSVGImage::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  const bool bbox_changed = UpdateBoundingBox();

  SVGLayoutResult result;
  if (bbox_changed) {
    result.bounds_changed = true;
  }
  if (UpdateAfterSVGLayout(layout_info, bbox_changed)) {
    result.bounds_changed = true;
  }

  if (result.bounds_changed) {
    DeprecatedInvalidateIntersectionObserverCachedRects();
  }

  DCHECK(!needs_transform_update_);
  ClearNeedsLayout();
  return result;
}

bool LayoutSVGImage::UpdateAfterSVGLayout(const SVGLayoutInfo& layout_info,
                                          bool bbox_changed) {
  if (bbox_changed) {
    SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kSVGResource);

    // Invalidate all resources of this client if our reference box changed.
    if (EverHadLayout())
      SVGResourceInvalidator(*this).InvalidateEffects();
  }
  if (!needs_transform_update_ && transform_uses_reference_box_) {
    needs_transform_update_ =
        CheckForImplicitTransformChange(layout_info, bbox_changed);
    if (needs_transform_update_)
      SetNeedsPaintPropertyUpdate();
  }
  if (needs_transform_update_) {
    local_transform_ =
        TransformHelper::ComputeTransformIncludingMotion(*GetElement());
    needs_transform_update_ = false;
    return true;
  }
  return false;
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
  if (hit_rules.require_visible &&
      style.UsedVisibility() != EVisibility::kVisible) {
    return false;
  }

  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (HasClipPath() && !ClipPathClipper::HitTest(*this, *local_location)) {
    return false;
  }

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

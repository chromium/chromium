/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"

#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/svg_shape_painter.h"
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

namespace {

void ClampBoundsToFinite(gfx::RectF& bounds) {
  bounds.set_x(ClampTo<float>(bounds.x()));
  bounds.set_y(ClampTo<float>(bounds.y()));
  bounds.set_width(ClampTo<float>(bounds.width()));
  bounds.set_height(ClampTo<float>(bounds.height()));
}

}  // namespace

LayoutSVGShape::LayoutSVGShape(SVGGeometryElement* node)
    : LayoutSVGModelObject(node),
      // A description (classification) of what geometric shape is represented -
      // used for computing stroke bounds more efficiently, fast-paths for
      // painting and determining if a shape is "empty".
      geometry_type_(GeometryType::kEmpty),
      // Default is false, the cached rects are empty from the beginning.
      needs_boundaries_update_(false),
      // Default is true, so we grab a Path object once from SVGGeometryElement.
      needs_shape_update_(true),
      // Default is true, so we grab a AffineTransform object once from
      // SVGGeometryElement.
      needs_transform_update_(true),
      transform_uses_reference_box_(false) {}

LayoutSVGShape::~LayoutSVGShape() = default;

void LayoutSVGShape::StyleDidChange(StyleDifference diff,
                                    const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGModelObject::StyleDidChange(diff, old_style);

  if (diff.NeedsFullLayout()) {
    SetNeedsBoundariesUpdate();
  }

  TransformHelper::UpdateOffsetPath(*GetElement(), old_style);
  transform_uses_reference_box_ =
      TransformHelper::UpdateReferenceBoxDependency(*this);
  SVGResources::UpdatePaints(*this, old_style, StyleRef());

  if (old_style) {
    const ComputedStyle& style = StyleRef();
    // Most of the stroke attributes (caps, joins, miters, width, etc.) will
    // cause a re-layout which will clear the stroke-path cache; however, there
    // are a couple of additional properties that *won't* cause a layout, but
    // are significant enough to require invalidating the cache.
    if (!diff.NeedsFullLayout() && stroke_path_cache_) {
      if (old_style->StrokeDashOffset() != style.StrokeDashOffset() ||
          *old_style->StrokeDashArray() != *style.StrokeDashArray()) {
        stroke_path_cache_.reset();
      }
    }

    if (transform_uses_reference_box_ && !needs_transform_update_) {
      if (TransformHelper::CheckReferenceBoxDependencies(*old_style, style)) {
        SetNeedsTransformUpdate();
        SetNeedsPaintPropertyUpdate();
      }
    }
  }

  SetTransformAffectsVectorEffect(HasNonScalingStroke());
}

void LayoutSVGShape::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearPaints(*this, Style());
  LayoutSVGModelObject::WillBeDestroyed();
}

void LayoutSVGShape::ClearPath() {
  NOT_DESTROYED();
  path_.reset();
  stroke_path_cache_.reset();
}

void LayoutSVGShape::CreatePath() {
  NOT_DESTROYED();
  if (!path_)
    path_ = std::make_unique<Path>();
  *path_ = To<SVGGeometryElement>(GetElement())->AsPath();

  // When the path changes, we need to ensure the stale stroke path cache is
  // cleared. Because this is done in all callsites, we can just DCHECK that it
  // has been cleared here.
  DCHECK(!stroke_path_cache_);
}

float LayoutSVGShape::DashScaleFactor() const {
  NOT_DESTROYED();
  if (!StyleRef().HasDashArray())
    return 1;
  return To<SVGGeometryElement>(*GetElement()).PathLengthScaleFactor();
}

namespace {

bool HasMiterJoinStyle(const ComputedStyle& style) {
  return style.JoinStyle() == kMiterJoin;
}
bool HasSquareCapStyle(const ComputedStyle& style) {
  return style.CapStyle() == kSquareCap;
}

bool CanUseSimpleStrokeApproximation(
    LayoutSVGShape::GeometryType geometry_type) {
  return geometry_type == LayoutSVGShape::GeometryType::kRectangle ||
         geometry_type == LayoutSVGShape::GeometryType::kRoundedRectangle ||
         geometry_type == LayoutSVGShape::GeometryType::kEllipse ||
         geometry_type == LayoutSVGShape::GeometryType::kCircle;
}

bool CanHaveMiters(LayoutSVGShape::GeometryType geometry_type) {
  DCHECK(!CanUseSimpleStrokeApproximation(geometry_type));
  return geometry_type == LayoutSVGShape::GeometryType::kPath;
}

bool CanHaveMitersOrCaps(LayoutSVGShape::GeometryType geometry_type) {
  return geometry_type == LayoutSVGShape::GeometryType::kPath ||
         geometry_type == LayoutSVGShape::GeometryType::kLine;
}

}  // namespace

gfx::RectF LayoutSVGShape::ApproximateStrokeBoundingBox(
    const gfx::RectF& shape_bounds) const {
  NOT_DESTROYED();
  gfx::RectF stroke_box = shape_bounds;

  // Implementation of
  // https://drafts.fxtf.org/css-masking/#compute-stroke-bounding-box
  // except that we ignore whether the stroke is none.

  const float stroke_width = StrokeWidth();
  if (stroke_width <= 0)
    return stroke_box;

  float delta = stroke_width / 2;
  if (CanHaveMitersOrCaps(geometry_type_)) {
    const ComputedStyle& style = StyleRef();
    if (CanHaveMiters(geometry_type_) && HasMiterJoinStyle(style)) {
      const float miter = style.StrokeMiterLimit();
      if (miter < M_SQRT2 && HasSquareCapStyle(style))
        delta *= M_SQRT2;
      else
        delta *= std::max(miter, 1.0f);
    } else if (HasSquareCapStyle(style)) {
      delta *= M_SQRT2;
    }
  }
  stroke_box.Outset(delta);
  return stroke_box;
}

gfx::RectF LayoutSVGShape::HitTestStrokeBoundingBox() const {
  NOT_DESTROYED();
  if (StyleRef().HasStroke())
    return decorated_bounding_box_;
  return ApproximateStrokeBoundingBox(fill_bounding_box_);
}

gfx::RectF LayoutSVGShape::StrokeBoundingBox() const {
  NOT_DESTROYED();
  if (!StyleRef().HasStroke() || IsShapeEmpty()) {
    return fill_bounding_box_;
  }
  // If no Path object has been created for the shape, assume that it is
  // 'simple' and thus the approximation is accurate.
  if (!HasPath()) {
    DCHECK(CanUseSimpleStrokeApproximation(geometry_type_));
    return ApproximateStrokeBoundingBox(fill_bounding_box_);
  }
  StrokeData stroke_data;
  SVGLayoutSupport::ApplyStrokeStyleToStrokeData(stroke_data, StyleRef(), *this,
                                                 DashScaleFactor());
  // Reset the dash pattern.
  //
  // "...set box to be the union of box and the tightest rectangle in
  // coordinate system space that contains the stroke shape of the element,
  // with the assumption that the element has no dash pattern."
  //
  // (https://www.w3.org/TR/SVG2/coords.html#TermStrokeBoundingBox)
  DashArray dashes;
  stroke_data.SetLineDash(dashes, 0);
  const gfx::RectF stroke_bounds = GetPath().StrokeBoundingRect(stroke_data);
  return gfx::UnionRects(fill_bounding_box_, stroke_bounds);
}

bool LayoutSVGShape::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  NOT_DESTROYED();
  if (!stroke_path_cache_) {
    const Path* path = path_.get();

    AffineTransform root_transform;
    if (HasNonScalingStroke()) {
      // Un-scale to get back to the root-transform (cheaper than re-computing
      // the root transform from scratch).
      root_transform.Scale(StyleRef().EffectiveZoom())
          .PreConcat(NonScalingStrokeTransform());

      path = &NonScalingStrokePath();
    } else {
      root_transform = ComputeRootTransform();
    }

    StrokeData stroke_data;
    SVGLayoutSupport::ApplyStrokeStyleToStrokeData(stroke_data, StyleRef(),
                                                   *this, DashScaleFactor());

    stroke_path_cache_ =
        std::make_unique<Path>(path->StrokePath(stroke_data, root_transform));
  }
  DCHECK(stroke_path_cache_);

  AffineTransform host_space_transform;
  if (HasNonScalingStroke())
    host_space_transform = NonScalingStrokeTransform();
  TransformedHitTestLocation host_space_location(
      location, host_space_transform,
      TransformedHitTestLocation::kDontComputeInverse);
  DCHECK(host_space_location);
  return host_space_location->Intersects(*stroke_path_cache_, RULE_NONZERO);
}

bool LayoutSVGShape::ShapeDependentFillContains(
    const HitTestLocation& location,
    const WindRule fill_rule) const {
  NOT_DESTROYED();
  return location.Intersects(GetPath(), fill_rule);
}

static bool HasPaintServer(const LayoutObject& object, const SVGPaint& paint) {
  if (paint.HasColor())
    return true;
  if (paint.HasUrl()) {
    SVGResourceClient* client = SVGResources::GetClient(object);
    if (GetSVGResourceAsType<LayoutSVGResourcePaintServer>(*client,
                                                           paint.Resource()))
      return true;
  }
  return false;
}

bool LayoutSVGShape::FillContains(const HitTestLocation& location,
                                  bool requires_fill,
                                  const WindRule fill_rule) {
  NOT_DESTROYED();
  if (!location.Intersects(fill_bounding_box_)) {
    return false;
  }

  if (requires_fill && !HasPaintServer(*this, StyleRef().FillPaint()))
    return false;

  return ShapeDependentFillContains(location, fill_rule);
}

bool LayoutSVGShape::StrokeContains(const HitTestLocation& location,
                                    bool requires_stroke) {
  NOT_DESTROYED();
  // "A zero value causes no stroke to be painted."
  if (StyleRef().StrokeWidth().IsZero())
    return false;

  if (requires_stroke) {
    if (!location.Intersects(DecoratedBoundingBox())) {
      return false;
    }

    if (!HasPaintServer(*this, StyleRef().StrokePaint()))
      return false;
  } else if (!location.Intersects(HitTestStrokeBoundingBox())) {
    return false;
  }
  return ShapeDependentStrokeContains(location);
}

SVGLayoutResult LayoutSVGShape::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();

  // The cached stroke may be affected by the ancestor transform, and so needs
  // to be cleared regardless of whether the shape or bounds have changed.
  stroke_path_cache_.reset();

  // Update the object bounds of the shape.
  bool bbox_changed = false;
  if (needs_shape_update_) {
    gfx::RectF new_object_bounding_box = UpdateShapeFromElement();
    ClampBoundsToFinite(new_object_bounding_box);
    bbox_changed = fill_bounding_box_ != new_object_bounding_box;
    fill_bounding_box_ = new_object_bounding_box;
    needs_shape_update_ = false;
    needs_boundaries_update_ = true;
  }

  SVGLayoutResult result;
  if (UpdateAfterSVGLayout(layout_info, bbox_changed)) {
    result.bounds_changed = true;
  }

  if (needs_boundaries_update_) {
    if (!IsShapeEmpty()) {
      decorated_bounding_box_ = CalculateStrokeBoundingBox();
      UpdateMarkerBounds();
    } else {
      decorated_bounding_box_ = fill_bounding_box_;
    }
    needs_boundaries_update_ = false;
    result.bounds_changed = true;
  }

  if (result.bounds_changed) {
    DeprecatedInvalidateIntersectionObserverCachedRects();
  }

  DCHECK(!needs_shape_update_);
  DCHECK(!needs_boundaries_update_);
  DCHECK(!needs_transform_update_);
  ClearNeedsLayout();
  return result;
}

bool LayoutSVGShape::UpdateAfterSVGLayout(const SVGLayoutInfo& layout_info,
                                          bool bbox_changed) {
  if (bbox_changed) {
    SetShouldDoFullPaintInvalidation();

    // Invalidate all resources of this client if our reference box changed.
    if (EverHadLayout()) {
      SVGResourceInvalidator resource_invalidator(*this);
      resource_invalidator.InvalidateEffects();
      resource_invalidator.InvalidatePaints();
    }
  }
  if (!needs_transform_update_ && transform_uses_reference_box_) {
    needs_transform_update_ =
        CheckForImplicitTransformChange(layout_info, bbox_changed);
    if (needs_transform_update_)
      SetNeedsPaintPropertyUpdate();
  }
  bool updated_transform = false;
  if (needs_transform_update_) {
    local_transform_ =
        TransformHelper::ComputeTransformIncludingMotion(*GetElement());
    needs_transform_update_ = false;
    updated_transform = true;
  }
  // The non-scaling-stroke transform depends on the local transform,
  // which in turn may depend on the object bounding box, thus we
  // can't update the non-scaling-stroke data before any of those have
  // been computed.
  //
  // We always do this because the non-scaling-stroke transform
  // depends on ancestor transforms. For the same reason we'll also
  // need to update the (stroke) bounds as a result.
  if (HasNonScalingStroke() && !IsShapeEmpty()) {
    UpdateNonScalingStrokeData();
    needs_boundaries_update_ = true;
    return true;
  }
  return updated_transform;
}

AffineTransform LayoutSVGShape::ComputeRootTransform() const {
  NOT_DESTROYED();
  const LayoutObject* root = this;
  while (root && !root->IsSVGRoot())
    root = root->Parent();
  return AffineTransform::FromTransform(
      LocalToAncestorTransform(To<LayoutSVGRoot>(root)));
}

AffineTransform LayoutSVGShape::ComputeNonScalingStrokeTransform() const {
  NOT_DESTROYED();
  // Compute the CTM to the SVG root. This should probably be the CTM all the
  // way to the "canvas" of the page ("host" coordinate system), but with our
  // current approach of applying/painting non-scaling-stroke, that can break in
  // unpleasant ways (see crbug.com/747708 for an example.) Maybe it would be
  // better to apply this effect during rasterization?
  AffineTransform host_transform;
  host_transform.Scale(1 / StyleRef().EffectiveZoom())
      .PreConcat(ComputeRootTransform());

  // Width of non-scaling stroke is independent of translation, so zero it out
  // here.
  host_transform.SetE(0);
  host_transform.SetF(0);
  return host_transform;
}

void LayoutSVGShape::UpdateNonScalingStrokeData() {
  NOT_DESTROYED();
  DCHECK(HasNonScalingStroke());

  const AffineTransform transform = ComputeNonScalingStrokeTransform();
  auto& rare_data = EnsureRareData();
  if (rare_data.non_scaling_stroke_transform_ != transform) {
    SetShouldDoFullPaintInvalidation();
    rare_data.non_scaling_stroke_transform_ = transform;
  }

  // For non-scaling-stroke we need to have a Path representation, so
  // create one here if needed.
  rare_data.non_scaling_stroke_path_ = EnsurePath();
  rare_data.non_scaling_stroke_path_.Transform(transform);
}

void LayoutSVGShape::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  SVGShapePainter(*this).Paint(paint_info);
}

bool LayoutSVGShape::NodeAtPoint(HitTestResult& result,
                                 const HitTestLocation& hit_test_location,
                                 const PhysicalOffset& accumulated_offset,
                                 HitTestPhase phase) {
  NOT_DESTROYED();
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  // We only draw in the foreground phase, so we only hit-test then.
  if (phase != HitTestPhase::kForeground)
    return false;
  if (IsShapeEmpty())
    return false;
  const ComputedStyle& style = StyleRef();
  const PointerEventsHitRules hit_rules(
      PointerEventsHitRules::kSvgGeometryHitTesting, result.GetHitTestRequest(),
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

  if (HitTestShape(result.GetHitTestRequest(), *local_location, hit_rules)) {
    UpdateHitTestResult(result, PhysicalOffset::FromPointFRound(
                                    local_location->TransformedPoint()));
    if (result.AddNodeToListBasedTestResult(GetElement(), *local_location) ==
        kStopHitTesting)
      return true;
  }

  return false;
}

bool LayoutSVGShape::HitTestShape(const HitTestRequest& request,
                                  const HitTestLocation& local_location,
                                  PointerEventsHitRules hit_rules) {
  NOT_DESTROYED();
  if (hit_rules.can_hit_bounding_box &&
      local_location.Intersects(ObjectBoundingBox()))
    return true;

  // TODO(chrishtr): support rect-based intersections in the cases below.
  const ComputedStyle& style = StyleRef();
  if (hit_rules.can_hit_stroke &&
      (style.HasStroke() || !hit_rules.require_stroke) &&
      StrokeContains(local_location, hit_rules.require_stroke))
    return true;
  WindRule fill_rule = style.FillRule();
  if (request.SvgClipContent())
    fill_rule = style.ClipRule();
  if (hit_rules.can_hit_fill && (style.HasFill() || !hit_rules.require_fill) &&
      FillContains(local_location, hit_rules.require_fill, fill_rule))
    return true;
  return false;
}

gfx::RectF LayoutSVGShape::CalculateStrokeBoundingBox() const {
  NOT_DESTROYED();
  if (!StyleRef().HasStroke()) {
    return fill_bounding_box_;
  }
  if (HasNonScalingStroke())
    return CalculateNonScalingStrokeBoundingBox();
  return ApproximateStrokeBoundingBox(fill_bounding_box_);
}

gfx::RectF LayoutSVGShape::CalculateNonScalingStrokeBoundingBox() const {
  NOT_DESTROYED();
  DCHECK(path_);
  DCHECK(StyleRef().HasStroke());
  DCHECK(HasNonScalingStroke());

  gfx::RectF stroke_bounding_box = fill_bounding_box_;
  const auto& non_scaling_transform = NonScalingStrokeTransform();
  if (non_scaling_transform.IsInvertible()) {
    gfx::RectF stroke_bounding_rect =
        ApproximateStrokeBoundingBox(NonScalingStrokePath().BoundingRect());
    stroke_bounding_rect =
        non_scaling_transform.Inverse().MapRect(stroke_bounding_rect);
    stroke_bounding_box.Union(stroke_bounding_rect);
  }
  return stroke_bounding_box;
}

float LayoutSVGShape::StrokeWidth() const {
  NOT_DESTROYED();
  const SVGViewportResolver viewport_resolver(*this);
  return ValueForLength(StyleRef().StrokeWidth(), viewport_resolver);
}

float LayoutSVGShape::StrokeWidthForMarkerUnits() const {
  NOT_DESTROYED();
  float stroke_width = StrokeWidth();
  if (HasNonScalingStroke()) {
    const auto& non_scaling_transform = NonScalingStrokeTransform();
    if (!non_scaling_transform.IsInvertible())
      return 0;
    float scale_factor =
        ClampTo<float>(sqrt((non_scaling_transform.XScaleSquared() +
                             non_scaling_transform.YScaleSquared()) /
                            2));
    stroke_width /= scale_factor;
  }
  return stroke_width;
}

LayoutSVGShapeRareData& LayoutSVGShape::EnsureRareData() const {
  NOT_DESTROYED();
  if (!rare_data_)
    rare_data_ = std::make_unique<LayoutSVGShapeRareData>();
  return *rare_data_.get();
}

RasterEffectOutset LayoutSVGShape::VisualRectOutsetForRasterEffects() const {
  NOT_DESTROYED();
  // Account for raster expansions due to SVG stroke hairline raster effects.
  const ComputedStyle& style = StyleRef();
  if (style.HasVisibleStroke()) {
    if (style.CapStyle() != kButtCap)
      return RasterEffectOutset::kWholePixel;
    return RasterEffectOutset::kHalfPixel;
  }
  return RasterEffectOutset::kNone;
}

}  // namespace blink

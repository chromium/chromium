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

#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_paint_server.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/svg_shape_painter.h"
#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

LayoutSVGShape::LayoutSVGShape(SVGGeometryElement* node,
                               StrokeGeometryClass geometry_class)
    : LayoutSVGModelObject(node),
      // Geometry classification - used to compute stroke bounds more
      // efficiently.
      geometry_class_(geometry_class),
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
  transform_uses_reference_box_ =
      TransformHelper::DependsOnReferenceBox(StyleRef());
  LayoutSVGModelObject::StyleDidChange(diff, old_style);
  SVGResources::UpdatePaints(*GetElement(), old_style, StyleRef());
}

void LayoutSVGShape::WillBeDestroyed() {
  SVGResources::ClearPaints(*GetElement(), Style());
  LayoutSVGModelObject::WillBeDestroyed();
}

void LayoutSVGShape::CreatePath() {
  if (!path_)
    path_ = std::make_unique<Path>();
  *path_ = ToSVGGeometryElement(GetElement())->AsPath();
}

float LayoutSVGShape::DashScaleFactor() const {
  if (StyleRef().SvgStyle().StrokeDashArray()->data.IsEmpty())
    return 1;
  return ToSVGGeometryElement(*GetElement()).PathLengthScaleFactor();
}

void LayoutSVGShape::UpdateShapeFromElement() {
  CreatePath();
  fill_bounding_box_ = GetPath().BoundingRect();

  if (HasNonScalingStroke()) {
    // NonScalingStrokeTransform may depend on LocalTransform which in turn may
    // depend on ObjectBoundingBox, thus we need to call them in this order.
    local_transform_ = CalculateLocalTransform();
    UpdateNonScalingStrokeData();
  }

  stroke_bounding_box_ = CalculateStrokeBoundingBox();
}

namespace {

bool HasMiterJoinStyle(const SVGComputedStyle& svg_style) {
  return svg_style.JoinStyle() == kMiterJoin;
}
bool HasSquareCapStyle(const SVGComputedStyle& svg_style) {
  return svg_style.CapStyle() == kSquareCap;
}

}  // namespace

FloatRect LayoutSVGShape::ApproximateStrokeBoundingBox(
    const FloatRect& shape_bounds) const {
  FloatRect stroke_box = shape_bounds;

  // Implementation of
  // https://drafts.fxtf.org/css-masking/#compute-stroke-bounding-box
  // except that we ignore whether the stroke is none.

  const float stroke_width = StrokeWidth();
  if (stroke_width <= 0)
    return stroke_box;

  float delta = stroke_width / 2;
  if (geometry_class_ != kSimple) {
    const SVGComputedStyle& svg_style = StyleRef().SvgStyle();
    if (geometry_class_ != kNoMiters && HasMiterJoinStyle(svg_style)) {
      const float miter = svg_style.StrokeMiterLimit();
      if (miter < M_SQRT2 && HasSquareCapStyle(svg_style))
        delta *= M_SQRT2;
      else
        delta *= std::max(miter, 1.0f);
    } else if (HasSquareCapStyle(svg_style)) {
      delta *= M_SQRT2;
    }
  }
  stroke_box.Inflate(delta);
  return stroke_box;
}

FloatRect LayoutSVGShape::HitTestStrokeBoundingBox() const {
  if (StyleRef().SvgStyle().HasStroke())
    return stroke_bounding_box_;
  return ApproximateStrokeBoundingBox(fill_bounding_box_);
}

bool LayoutSVGShape::ShapeDependentStrokeContains(
    const HitTestLocation& location) {
  // In case the subclass didn't create path during UpdateShapeFromElement()
  // for optimization but still calls this method.
  if (!HasPath())
    CreatePath();

  StrokeData stroke_data;
  SVGLayoutSupport::ApplyStrokeStyleToStrokeData(stroke_data, StyleRef(), *this,
                                                 DashScaleFactor());

  if (HasNonScalingStroke()) {
    // The reason is similar to the above code about HasPath().
    if (!rare_data_)
      UpdateNonScalingStrokeData();
    return NonScalingStrokePath().StrokeContains(
        NonScalingStrokeTransform().MapPoint(location.TransformedPoint()),
        stroke_data);
  }
  return path_->StrokeContains(location.TransformedPoint(), stroke_data);
}

bool LayoutSVGShape::ShapeDependentFillContains(
    const HitTestLocation& location,
    const WindRule fill_rule) const {
  return GetPath().Contains(location.TransformedPoint(), fill_rule);
}

bool LayoutSVGShape::FillContains(const HitTestLocation& location,
                                  bool requires_fill,
                                  const WindRule fill_rule) {
  if (!fill_bounding_box_.Contains(location.TransformedPoint()))
    return false;

  if (requires_fill && !SVGPaintServer::ExistsForLayoutObject(*this, StyleRef(),
                                                              kApplyToFillMode))
    return false;

  return ShapeDependentFillContains(location, fill_rule);
}

bool LayoutSVGShape::StrokeContains(const HitTestLocation& location,
                                    bool requires_stroke) {
  // "A zero value causes no stroke to be painted."
  if (StyleRef().SvgStyle().StrokeWidth().IsZero())
    return false;

  if (requires_stroke) {
    if (!StrokeBoundingBox().Contains(location.TransformedPoint()))
      return false;

    if (!SVGPaintServer::ExistsForLayoutObject(*this, StyleRef(),
                                               kApplyToStrokeMode))
      return false;
  } else {
    if (!HitTestStrokeBoundingBox().Contains(location.TransformedPoint()))
      return false;
  }

  return ShapeDependentStrokeContains(location);
}

void LayoutSVGShape::UpdateLayout() {
  LayoutAnalyzer::Scope analyzer(*this);

  // Invalidate all resources of this client if our layout changed.
  if (EverHadLayout() && SelfNeedsLayout())
    SVGResourcesCache::ClientLayoutChanged(*this);

  bool update_parent_boundaries = false;
  bool bbox_changed = false;
  // UpdateShapeFromElement() also updates the object & stroke bounds - which
  // feeds into the visual rect - so we need to call it for both the
  // shape-update and the bounds-update flag.
  // We also need to update stroke bounds if HasNonScalingStroke() because the
  // shape may be affected by ancestor transforms.
  if (needs_shape_update_ || needs_boundaries_update_ ||
      HasNonScalingStroke()) {
    FloatRect old_object_bounding_box = ObjectBoundingBox();
    UpdateShapeFromElement();
    if (old_object_bounding_box != ObjectBoundingBox()) {
      GetElement()->SetNeedsResizeObserverUpdate();
      SetShouldDoFullPaintInvalidation();
      bbox_changed = true;
    }
    needs_shape_update_ = false;

    local_visual_rect_ = StrokeBoundingBox();
    SVGLayoutSupport::AdjustVisualRectWithResources(*this, ObjectBoundingBox(),
                                                    local_visual_rect_);
    needs_boundaries_update_ = false;

    update_parent_boundaries = true;
  }

  if (!needs_transform_update_ && transform_uses_reference_box_) {
    needs_transform_update_ = CheckForImplicitTransformChange(bbox_changed);
    if (needs_transform_update_)
      SetNeedsPaintPropertyUpdate();
  }

  if (needs_transform_update_) {
    local_transform_ = CalculateLocalTransform();
    needs_transform_update_ = false;
    update_parent_boundaries = true;
  }

  // If our bounds changed, notify the parents.
  if (update_parent_boundaries)
    LayoutSVGModelObject::SetNeedsBoundariesUpdate();

  DCHECK(!needs_shape_update_);
  DCHECK(!needs_boundaries_update_);
  DCHECK(!needs_transform_update_);
  ClearNeedsLayout();
}

AffineTransform LayoutSVGShape::ComputeNonScalingStrokeTransform() const {
  // Compute the CTM to the SVG root. This should probably be the CTM all the
  // way to the "canvas" of the page ("host" coordinate system), but with our
  // current approach of applying/painting non-scaling-stroke, that can break in
  // unpleasant ways (see crbug.com/747708 for an example.) Maybe it would be
  // better to apply this effect during rasterization?
  const LayoutObject* root = this;
  while (root && !root->IsSVGRoot())
    root = root->Parent();
  AffineTransform host_transform;
  host_transform.Scale(1 / StyleRef().EffectiveZoom())
      .Multiply(
          LocalToAncestorTransform(ToLayoutSVGRoot(root)).ToAffineTransform());
  // Width of non-scaling stroke is independent of translation, so zero it out
  // here.
  host_transform.SetE(0);
  host_transform.SetF(0);
  return host_transform;
}

void LayoutSVGShape::UpdateNonScalingStrokeData() {
  DCHECK(HasNonScalingStroke());

  const AffineTransform transform = ComputeNonScalingStrokeTransform();
  auto& rare_data = EnsureRareData();
  if (rare_data.non_scaling_stroke_transform_ != transform) {
    SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kStyle);
    rare_data.non_scaling_stroke_transform_ = transform;
  }

  rare_data.non_scaling_stroke_path_ = *path_;
  rare_data.non_scaling_stroke_path_.Transform(transform);
}

void LayoutSVGShape::Paint(const PaintInfo& paint_info) const {
  SVGShapePainter(*this).Paint(paint_info);
}

bool LayoutSVGShape::NodeAtPoint(HitTestResult& result,
                                 const HitTestLocation& hit_test_location,
                                 const PhysicalOffset& accumulated_offset,
                                 HitTestAction hit_test_action) {
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  // We only draw in the foreground phase, so we only hit-test then.
  if (hit_test_action != kHitTestForeground)
    return false;
  if (IsShapeEmpty())
    return false;
  const ComputedStyle& style = StyleRef();
  const PointerEventsHitRules hit_rules(
      PointerEventsHitRules::SVG_GEOMETRY_HITTESTING,
      result.GetHitTestRequest(), style.PointerEvents());
  if (hit_rules.require_visible && style.Visibility() != EVisibility::kVisible)
    return false;

  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (!SVGLayoutSupport::IntersectsClipPath(*this, fill_bounding_box_,
                                            *local_location))
    return false;

  if (HitTestShape(result.GetHitTestRequest(), *local_location, hit_rules)) {
    UpdateHitTestResult(result, PhysicalOffset::FromFloatPointRound(
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
  if (hit_rules.can_hit_bounding_box &&
      local_location.Intersects(ObjectBoundingBox()))
    return true;

  // TODO(chrishtr): support rect-based intersections in the cases below.
  const SVGComputedStyle& svg_style = StyleRef().SvgStyle();
  if (hit_rules.can_hit_stroke &&
      (svg_style.HasStroke() || !hit_rules.require_stroke) &&
      StrokeContains(local_location, hit_rules.require_stroke))
    return true;
  WindRule fill_rule = svg_style.FillRule();
  if (request.SvgClipContent())
    fill_rule = svg_style.ClipRule();
  if (hit_rules.can_hit_fill &&
      (svg_style.HasFill() || !hit_rules.require_fill) &&
      FillContains(local_location, hit_rules.require_fill, fill_rule))
    return true;
  return false;
}

FloatRect LayoutSVGShape::CalculateStrokeBoundingBox() const {
  if (!StyleRef().SvgStyle().HasStroke() || IsShapeEmpty())
    return fill_bounding_box_;
  if (HasNonScalingStroke())
    return CalculateNonScalingStrokeBoundingBox();
  return ApproximateStrokeBoundingBox(fill_bounding_box_);
}

FloatRect LayoutSVGShape::CalculateNonScalingStrokeBoundingBox() const {
  DCHECK(path_);
  DCHECK(StyleRef().SvgStyle().HasStroke());
  DCHECK(HasNonScalingStroke());

  FloatRect stroke_bounding_box = fill_bounding_box_;
  const auto& non_scaling_transform = NonScalingStrokeTransform();
  if (non_scaling_transform.IsInvertible()) {
    const auto& non_scaling_stroke = NonScalingStrokePath();
    FloatRect stroke_bounding_rect =
        ApproximateStrokeBoundingBox(non_scaling_stroke.BoundingRect());
    stroke_bounding_rect =
        non_scaling_transform.Inverse().MapRect(stroke_bounding_rect);
    stroke_bounding_box.Unite(stroke_bounding_rect);
  }
  return stroke_bounding_box;
}

float LayoutSVGShape::StrokeWidth() const {
  SVGLengthContext length_context(GetElement());
  return length_context.ValueForLength(StyleRef().SvgStyle().StrokeWidth());
}

LayoutSVGShapeRareData& LayoutSVGShape::EnsureRareData() const {
  if (!rare_data_)
    rare_data_ = std::make_unique<LayoutSVGShapeRareData>();
  return *rare_data_.get();
}

float LayoutSVGShape::VisualRectOutsetForRasterEffects() const {
  // Account for raster expansions due to SVG stroke hairline raster effects.
  if (StyleRef().SvgStyle().HasVisibleStroke()) {
    float outset = 0.5f;
    if (StyleRef().SvgStyle().CapStyle() != kButtCap)
      outset += 0.5f;
    return outset;
  }
  return 0;
}

}  // namespace blink

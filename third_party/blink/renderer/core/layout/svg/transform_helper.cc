// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/reference_offset_path_operation.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

bool StrokeBoundingBoxMayHaveChanged(const ComputedStyle& old_style,
                                     const ComputedStyle& style) {
  return old_style.StrokeWidth() != style.StrokeWidth() ||
         old_style.CapStyle() != style.CapStyle() ||
         old_style.StrokeMiterLimit() != style.StrokeMiterLimit() ||
         old_style.JoinStyle() != style.JoinStyle();
}

}  // namespace

static inline bool TransformOriginIsFixed(const ComputedStyle& style) {
  // If the transform box is view-box and the transform origin is absolute,
  // then is does not depend on the reference box. For fill-box, the origin
  // will always move with the bounding box.
  return style.TransformBox() == ETransformBox::kViewBox &&
         style.GetTransformOrigin().X().IsFixed() &&
         style.GetTransformOrigin().Y().IsFixed();
}

// static
void TransformHelper::UpdateOffsetPath(SVGElement& element,
                                       const ComputedStyle* old_style) {
  const ComputedStyle& new_style = element.ComputedStyleRef();
  OffsetPathOperation* new_offset = new_style.OffsetPath();
  OffsetPathOperation* old_offset =
      old_style ? old_style->OffsetPath() : nullptr;
  if (!new_offset && !old_offset) {
    return;
  }
  const bool had_resource_info = element.GetSVGResourceClient();
  if (auto* reference_offset =
          DynamicTo<ReferenceOffsetPathOperation>(new_offset)) {
    reference_offset->AddClient(element.EnsureSVGResourceClient());
  }
  if (had_resource_info) {
    if (auto* old_reference_offset =
            DynamicTo<ReferenceOffsetPathOperation>(old_offset)) {
      old_reference_offset->RemoveClient(*element.GetSVGResourceClient());
    }
  }
}

bool TransformHelper::DependsOnReferenceBox(const ComputedStyle& style) {
  // We're passing kExcludeMotionPath here because we're checking that
  // explicitly later.
  if (!TransformOriginIsFixed(style) &&
      style.RequireTransformOrigin(ComputedStyle::kIncludeTransformOrigin,
                                   ComputedStyle::kExcludeMotionPath))
    return true;
  if (style.Transform().BoxSizeDependencies())
    return true;
  if (style.Translate() && style.Translate()->BoxSizeDependencies())
    return true;
  if (style.HasOffset())
    return true;
  return false;
}

bool TransformHelper::UpdateReferenceBoxDependency(
    LayoutObject& layout_object) {
  const bool transform_uses_reference_box =
      DependsOnReferenceBox(layout_object.StyleRef());
  UpdateReferenceBoxDependency(layout_object, transform_uses_reference_box);
  return transform_uses_reference_box;
}

void TransformHelper::UpdateReferenceBoxDependency(
    LayoutObject& layout_object,
    bool transform_uses_reference_box) {
  if (transform_uses_reference_box &&
      layout_object.StyleRef().TransformBox() == ETransformBox::kViewBox) {
    layout_object.SetSVGSelfOrDescendantHasViewportDependency();
  } else {
    layout_object.ClearSVGSelfOrDescendantHasViewportDependency();
  }
}

bool TransformHelper::CheckReferenceBoxDependencies(
    const ComputedStyle& old_style,
    const ComputedStyle& style) {
  const ETransformBox transform_box =
      style.UsedTransformBox(ComputedStyle::TransformBoxContext::kSvg);
  // Changes to fill-box and view-box are handled by the
  // `CheckForImplicitTransformChange()` implementations.
  if (transform_box != ETransformBox::kStrokeBox) {
    return false;
  }
  return StrokeBoundingBoxMayHaveChanged(old_style, style);
}

gfx::RectF TransformHelper::ComputeReferenceBox(
    const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  gfx::RectF reference_box;
  switch (style.UsedTransformBox(ComputedStyle::TransformBoxContext::kSvg)) {
    case ETransformBox::kFillBox:
      reference_box = layout_object.ObjectBoundingBox();
      break;
    case ETransformBox::kStrokeBox:
      reference_box = layout_object.StrokeBoundingBox();
      break;
    case ETransformBox::kViewBox: {
      const SVGViewportResolver viewport_resolver(layout_object);
      reference_box.set_size(viewport_resolver.ResolveViewport());
      break;
    }
    case ETransformBox::kContentBox:
    case ETransformBox::kBorderBox:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  const float zoom = style.EffectiveZoom();
  if (zoom != 1)
    reference_box.Scale(zoom);
  return reference_box;
}

AffineTransform TransformHelper::ComputeTransform(
    UseCounter& use_counter,
    const ComputedStyle& style,
    const gfx::RectF& reference_box,
    ComputedStyle::ApplyTransformOrigin apply_transform_origin) {
  if (DependsOnReferenceBox(style)) {
    UseCounter::Count(use_counter, WebFeature::kTransformUsesBoxSizeOnSVG);
  }

  // CSS transforms operate with pre-scaled lengths. To make this work with SVG
  // (which applies the zoom factor globally, at the root level) we
  //
  //  * pre-scale the reference box (to bring it into the same space as the
  //    other CSS values) (Handled by ComputeSVGTransformReferenceBox)
  //  * invert the zoom factor (to effectively compute the CSS transform under
  //    a 1.0 zoom)
  //
  // Note: objectBoundingBox is an empty rect for elements like pattern or
  // clipPath. See
  // https://svgwg.org/svg2-draft/coords.html#ObjectBoundingBoxUnits
  gfx::Transform transform;
  style.ApplyTransform(transform, nullptr, reference_box,
                       ComputedStyle::kIncludeTransformOperations,
                       apply_transform_origin,
                       ComputedStyle::kIncludeMotionPath,
                       ComputedStyle::kIncludeIndependentTransformProperties);
  const float zoom = style.EffectiveZoom();
  if (zoom != 1)
    transform.Zoom(1 / zoom);
  // Flatten any 3D transform.
  return AffineTransform::FromTransform(transform);
}

AffineTransform TransformHelper::ComputeTransformIncludingMotion(
    const SVGElement& element,
    const gfx::RectF& reference_box) {
  const LayoutObject& layout_object = *element.GetLayoutObject();
  if (layout_object.HasTransform() || element.HasMotionTransform()) {
    AffineTransform matrix =
        ComputeTransform(element.GetDocument(), layout_object.StyleRef(),
                         reference_box, ComputedStyle::kIncludeTransformOrigin);
    element.ApplyMotionTransform(matrix);
    return matrix;
  }
  return AffineTransform();
}

AffineTransform TransformHelper::ComputeTransformIncludingMotion(
    const SVGElement& element) {
  const LayoutObject& layout_object = *element.GetLayoutObject();
  const gfx::RectF reference_box = ComputeReferenceBox(layout_object);
  return ComputeTransformIncludingMotion(element, reference_box);
}

gfx::PointF TransformHelper::ComputeTransformOrigin(
    const ComputedStyle& style,
    const gfx::RectF& reference_box) {
  gfx::PointF origin(FloatValueForLength(style.GetTransformOrigin().X(),
                                         reference_box.width()) +
                         reference_box.x(),
                     FloatValueForLength(style.GetTransformOrigin().Y(),
                                         reference_box.height()) +
                         reference_box.y());
  // See the comment in ComputeTransform() for the reason of scaling by 1/zoom.
  return gfx::ScalePoint(origin, style.EffectiveZoom());
}

}  // namespace blink

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

static inline bool TransformOriginIsFixed(const ComputedStyle& style) {
  // If the transform box is view-box and the transform origin is absolute,
  // then is does not depend on the reference box. For fill-box, the origin
  // will always move with the bounding box.
  return style.TransformBox() == ETransformBox::kViewBox &&
         style.TransformOriginX().IsFixed() &&
         style.TransformOriginY().IsFixed();
}

bool TransformHelper::DependsOnReferenceBox(const ComputedStyle& style) {
  // We're passing kExcludeMotionPath here because we're checking that
  // explicitly later.
  if (!TransformOriginIsFixed(style) &&
      style.RequireTransformOrigin(ComputedStyle::kIncludeTransformOrigin,
                                   ComputedStyle::kExcludeMotionPath))
    return true;
  if (style.Transform().DependsOnBoxSize())
    return true;
  if (style.Translate() && style.Translate()->DependsOnBoxSize())
    return true;
  if (style.HasOffset())
    return true;
  return false;
}

FloatRect TransformHelper::ComputeReferenceBox(
    const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  FloatRect reference_box;
  if (style.TransformBox() == ETransformBox::kFillBox) {
    reference_box = layout_object.ObjectBoundingBox();
  } else {
    DCHECK_EQ(style.TransformBox(), ETransformBox::kViewBox);
    SVGLengthContext length_context(
        DynamicTo<SVGElement>(layout_object.GetNode()));
    FloatSize viewport_size;
    length_context.DetermineViewport(viewport_size);
    reference_box.SetSize(viewport_size);
  }
  const float zoom = style.EffectiveZoom();
  if (zoom != 1)
    reference_box.Scale(zoom);
  return reference_box;
}

AffineTransform TransformHelper::ComputeTransform(
    const LayoutObject& layout_object) {
  const ComputedStyle& style = layout_object.StyleRef();
  if (DependsOnReferenceBox(style)) {
    UseCounter::Count(layout_object.GetDocument(),
                      WebFeature::kTransformUsesBoxSizeOnSVG);
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
  TransformationMatrix transform;
  FloatRect reference_box = ComputeReferenceBox(layout_object);
  style.ApplyTransform(transform, reference_box,
                       ComputedStyle::kIncludeTransformOrigin,
                       ComputedStyle::kIncludeMotionPath,
                       ComputedStyle::kIncludeIndependentTransformProperties);
  const float zoom = style.EffectiveZoom();
  if (zoom != 1)
    transform.Zoom(1 / zoom);
  // Flatten any 3D transform.
  return transform.ToAffineTransform();
}

}  // namespace blink

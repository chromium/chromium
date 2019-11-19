/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/svg/svg_geometry_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_path.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_shape.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class SVGAnimatedPathLength final : public SVGAnimatedNumber {
 public:
  explicit SVGAnimatedPathLength(SVGGeometryElement* context_element)
      : SVGAnimatedNumber(context_element,
                          svg_names::kPathLengthAttr,
                          MakeGarbageCollected<SVGNumber>()) {}

  SVGParsingError AttributeChanged(const String& value) override {
    SVGParsingError parse_status = SVGAnimatedNumber::AttributeChanged(value);
    if (parse_status == SVGParseStatus::kNoError && BaseValue()->Value() < 0)
      parse_status = SVGParseStatus::kNegativeValue;
    return parse_status;
  }
};

SVGGeometryElement::SVGGeometryElement(const QualifiedName& tag_name,
                                       Document& document,
                                       ConstructionType construction_type)
    : SVGGraphicsElement(tag_name, document, construction_type),
      path_length_(MakeGarbageCollected<SVGAnimatedPathLength>(this)) {
  AddToPropertyMap(path_length_);
}

void SVGGeometryElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kPathLengthAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    if (LayoutObject* layout_object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*layout_object);
    return;
  }

  SVGGraphicsElement::SvgAttributeChanged(attr_name);
}

void SVGGeometryElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(path_length_);
  SVGGraphicsElement::Trace(visitor);
}

bool SVGGeometryElement::isPointInFill(SVGPointTearOff* point) const {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  // FIXME: Eventually we should support isPointInFill for display:none
  // elements.
  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return false;

  // Path::Contains will reject points with a non-finite component.
  WindRule fill_rule = layout_object->StyleRef().SvgStyle().FillRule();
  return AsPath().Contains(point->Target()->Value(), fill_rule);
}

bool SVGGeometryElement::isPointInStroke(SVGPointTearOff* point) const {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  // FIXME: Eventually we should support isPointInStroke for display:none
  // elements.
  const LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return false;
  const LayoutSVGShape& layout_shape = ToLayoutSVGShape(*layout_object);

  StrokeData stroke_data;
  SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
      stroke_data, layout_shape.StyleRef(), layout_shape,
      PathLengthScaleFactor());

  Path path = AsPath();
  FloatPoint local_point(point->Target()->Value());
  if (layout_shape.HasNonScalingStroke()) {
    const AffineTransform transform =
        layout_shape.ComputeNonScalingStrokeTransform();
    path.Transform(transform);
    local_point = transform.MapPoint(local_point);
  }
  // Path::StrokeContains will reject points with a non-finite component.
  return path.StrokeContains(local_point, stroke_data);
}

Path SVGGeometryElement::ToClipPath() const {
  Path path = AsPath();
  path.Transform(CalculateTransform(SVGElement::kIncludeMotionTransform));

  DCHECK(GetLayoutObject());
  DCHECK(GetLayoutObject()->Style());
  path.SetWindRule(GetLayoutObject()->StyleRef().SvgStyle().ClipRule());
  return path;
}

float SVGGeometryElement::getTotalLength(ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  if (!GetLayoutObject()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "This element is non-rendered element.");
    return 0;
  }

  return AsPath().length();
}

SVGPointTearOff* SVGGeometryElement::getPointAtLength(float length) {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  FloatPoint point;
  if (GetLayoutObject()) {
    const Path& path = AsPath();
    if (length < 0) {
      length = 0;
    } else {
      float computed_length = path.length();
      if (length > computed_length)
        length = computed_length;
    }
    point = path.PointAtLength(length);
  }
  return SVGPointTearOff::CreateDetached(point);
}

float SVGGeometryElement::ComputePathLength() const {
  return AsPath().length();
}

float SVGGeometryElement::AuthorPathLength() const {
  if (!pathLength()->IsSpecified())
    return std::numeric_limits<float>::quiet_NaN();
  float author_path_length = pathLength()->CurrentValue()->Value();
  // https://svgwg.org/svg2-draft/paths.html#PathLengthAttribute
  // "A negative value is an error"
  if (author_path_length < 0)
    return std::numeric_limits<float>::quiet_NaN();
  return author_path_length;
}

float SVGGeometryElement::PathLengthScaleFactor() const {
  float author_path_length = AuthorPathLength();
  if (std::isnan(author_path_length))
    return 1;
  DCHECK(GetLayoutObject());
  return PathLengthScaleFactor(ComputePathLength(), author_path_length);
}

float SVGGeometryElement::PathLengthScaleFactor(float computed_path_length,
                                                float author_path_length) {
  DCHECK(!std::isnan(author_path_length));
  // If the computed path length is zero, then the scale factor will
  // always be zero except if the author path length is also zero - in
  // which case performing the division would yield a NaN. Avoid the
  // division in this case and always return zero.
  if (!computed_path_length)
    return 0;
  // "A value of zero is valid and must be treated as a scaling factor
  //  of infinity. A value of zero scaled infinitely must remain zero,
  //  while any value greater than zero must become +Infinity."
  // However, since 0 * Infinity is not zero (but rather NaN) per
  // IEEE, we need to make sure to clamp the result below - avoiding
  // the actual Infinity (and using max()) instead.
  return clampTo<float>(computed_path_length / author_path_length);
}

void SVGGeometryElement::GeometryPresentationAttributeChanged(
    const QualifiedName& attr_name) {
  InvalidateSVGPresentationAttributeStyle();
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::FromAttribute(attr_name));
  GeometryAttributeChanged();
}

void SVGGeometryElement::GeometryAttributeChanged() {
  SVGElement::InvalidationGuard invalidation_guard(this);
  if (LayoutSVGShape* layout_object = ToLayoutSVGShape(GetLayoutObject())) {
    layout_object->SetNeedsShapeUpdate();
    MarkForLayoutAndParentResourceInvalidation(*layout_object);
  }
}

LayoutObject* SVGGeometryElement::CreateLayoutObject(const ComputedStyle&,
                                                     LegacyLayout) {
  // By default, any subclass is expected to do path-based drawing.
  return new LayoutSVGPath(this);
}

}  // namespace blink

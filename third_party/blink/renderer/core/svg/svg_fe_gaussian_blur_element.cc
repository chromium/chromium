/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_fe_gaussian_blur_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_optional_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEGaussianBlurTag,
                                           document),
      std_deviation_(MakeGarbageCollected<SVGAnimatedNumberOptionalNumber>(
          this,
          svg_names::kStdDeviationAttr,
          0.0f)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {}

void SVGFEGaussianBlurElement::setStdDeviation(float x, float y) {
  stdDeviationX()->BaseValue()->SetValue(x);
  stdDeviationY()->BaseValue()->SetValue(y);
  Invalidate();
}

SVGAnimatedNumber* SVGFEGaussianBlurElement::stdDeviationX() {
  return std_deviation_->FirstNumber();
}

SVGAnimatedNumber* SVGFEGaussianBlurElement::stdDeviationY() {
  return std_deviation_->SecondNumber();
}

void SVGFEGaussianBlurElement::Trace(Visitor* visitor) const {
  visitor->Trace(std_deviation_);
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

void SVGFEGaussianBlurElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kInAttr ||
      attr_name == svg_names::kStdDeviationAttr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
}

FilterEffect* SVGFEGaussianBlurElement::Build(SVGFilterBuilder* filter_builder,
                                              Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  // "A negative value or a value of zero disables the effect of the given
  // filter primitive (i.e., the result is the filter input image)."
  // (https://drafts.fxtf.org/filter-effects/#element-attrdef-fegaussianblur-stddeviation)
  //
  // => Clamp to non-negative.
  float std_dev_x = std::max(0.0f, stdDeviationX()->CurrentValue()->Value());
  float std_dev_y = std::max(0.0f, stdDeviationY()->CurrentValue()->Value());
  auto* effect =
      MakeGarbageCollected<FEGaussianBlur>(filter, std_dev_x, std_dev_y);
  effect->InputEffects().push_back(input1);
  return effect;
}

SVGAnimatedPropertyBase* SVGFEGaussianBlurElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kStdDeviationAttr) {
    return std_deviation_.Get();
  } else if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFEGaussianBlurElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{std_deviation_.Get(), in1_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink

/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_fe_drop_shadow_element.h"

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number_optional_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_drop_shadow.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGFEDropShadowElement::SVGFEDropShadowElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEDropShadowTag,
                                           document),
      dx_(MakeGarbageCollected<SVGAnimatedNumber>(this, svg_names::kDxAttr, 2)),
      dy_(MakeGarbageCollected<SVGAnimatedNumber>(this, svg_names::kDyAttr, 2)),
      std_deviation_(MakeGarbageCollected<SVGAnimatedNumberOptionalNumber>(
          this,
          svg_names::kStdDeviationAttr,
          2)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)) {}

SVGAnimatedNumber* SVGFEDropShadowElement::stdDeviationX() {
  return std_deviation_->FirstNumber();
}

SVGAnimatedNumber* SVGFEDropShadowElement::stdDeviationY() {
  return std_deviation_->SecondNumber();
}

void SVGFEDropShadowElement::Trace(Visitor* visitor) const {
  visitor->Trace(dx_);
  visitor->Trace(dy_);
  visitor->Trace(std_deviation_);
  visitor->Trace(in1_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

void SVGFEDropShadowElement::setStdDeviation(float x, float y) {
  stdDeviationX()->BaseValue()->SetValue(x);
  stdDeviationY()->BaseValue()->SetValue(y);
  Invalidate();
}

bool SVGFEDropShadowElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  const ComputedStyle& style = ComputedStyleRef();

  FEDropShadow* drop_shadow = static_cast<FEDropShadow*>(effect);
  if (attr_name == svg_names::kFloodColorAttr) {
    drop_shadow->SetShadowColor(
        style.VisitedDependentColor(GetCSSPropertyFloodColor()));
    return true;
  }
  if (attr_name == svg_names::kFloodOpacityAttr) {
    drop_shadow->SetShadowOpacity(style.FloodOpacity());
    return true;
  }
  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEDropShadowElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kInAttr ||
      attr_name == svg_names::kStdDeviationAttr ||
      attr_name == svg_names::kDxAttr || attr_name == svg_names::kDyAttr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
}

FilterEffect* SVGFEDropShadowElement::Build(SVGFilterBuilder* filter_builder,
                                            Filter* filter) {
  const ComputedStyle* style = GetComputedStyle();
  if (!style)
    return nullptr;

  Color color = style->VisitedDependentColor(GetCSSPropertyFloodColor());
  float opacity = style->FloodOpacity();

  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  DCHECK(input1);

  // Clamp std.dev. to non-negative. (See SVGFEGaussianBlurElement::build)
  float std_dev_x = std::max(0.0f, stdDeviationX()->CurrentValue()->Value());
  float std_dev_y = std::max(0.0f, stdDeviationY()->CurrentValue()->Value());
  auto* effect = MakeGarbageCollected<FEDropShadow>(
      filter, std_dev_x, std_dev_y, dx_->CurrentValue()->Value(),
      dy_->CurrentValue()->Value(), color, opacity);
  effect->InputEffects().push_back(input1);
  return effect;
}

bool SVGFEDropShadowElement::TaintsOrigin() const {
  const ComputedStyle* style = GetComputedStyle();
  // TaintsOrigin() is only called after a successful call to Build()
  // (see above), so we should have a ComputedStyle here.
  DCHECK(style);
  return style->FloodColor().IsCurrentColor();
}

SVGAnimatedPropertyBase* SVGFEDropShadowElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kDxAttr) {
    return dx_.Get();
  } else if (attribute_name == svg_names::kDyAttr) {
    return dy_.Get();
  } else if (attribute_name == svg_names::kStdDeviationAttr) {
    return std_deviation_.Get();
  } else if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFEDropShadowElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{dx_.Get(), dy_.Get(), std_deviation_.Get(),
                                   in1_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink

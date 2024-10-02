/*
 * Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
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

#include "third_party/blink/renderer/core/svg/svg_fe_displacement_map_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_string.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<ChannelSelectorType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {CHANNEL_R, "R"},
      {CHANNEL_G, "G"},
      {CHANNEL_B, "B"},
      {CHANNEL_A, "A"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGFEDisplacementMapElement::SVGFEDisplacementMapElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEDisplacementMapTag,
                                           document),
      scale_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                     svg_names::kScaleAttr,
                                                     0.0f)),
      in1_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kInAttr)),
      in2_(MakeGarbageCollected<SVGAnimatedString>(this, svg_names::kIn2Attr)),
      x_channel_selector_(
          MakeGarbageCollected<SVGAnimatedEnumeration<ChannelSelectorType>>(
              this,
              svg_names::kXChannelSelectorAttr,
              CHANNEL_A)),
      y_channel_selector_(
          MakeGarbageCollected<SVGAnimatedEnumeration<ChannelSelectorType>>(
              this,
              svg_names::kYChannelSelectorAttr,
              CHANNEL_A)) {}

void SVGFEDisplacementMapElement::Trace(Visitor* visitor) const {
  visitor->Trace(scale_);
  visitor->Trace(in1_);
  visitor->Trace(in2_);
  visitor->Trace(x_channel_selector_);
  visitor->Trace(y_channel_selector_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFEDisplacementMapElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEDisplacementMap* displacement_map = static_cast<FEDisplacementMap*>(effect);
  if (attr_name == svg_names::kXChannelSelectorAttr) {
    return displacement_map->SetXChannelSelector(
        x_channel_selector_->CurrentEnumValue());
  }
  if (attr_name == svg_names::kYChannelSelectorAttr) {
    return displacement_map->SetYChannelSelector(
        y_channel_selector_->CurrentEnumValue());
  }
  if (attr_name == svg_names::kScaleAttr)
    return displacement_map->SetScale(scale_->CurrentValue()->Value());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEDisplacementMapElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kXChannelSelectorAttr ||
      attr_name == svg_names::kYChannelSelectorAttr ||
      attr_name == svg_names::kScaleAttr) {
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr || attr_name == svg_names::kIn2Attr) {
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
}

FilterEffect* SVGFEDisplacementMapElement::Build(
    SVGFilterBuilder* filter_builder,
    Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  FilterEffect* input2 = filter_builder->GetEffectById(
      AtomicString(in2_->CurrentValue()->Value()));
  DCHECK(input1);
  DCHECK(input2);

  auto* effect = MakeGarbageCollected<FEDisplacementMap>(
      filter, x_channel_selector_->CurrentEnumValue(),
      y_channel_selector_->CurrentEnumValue(), scale_->CurrentValue()->Value());
  FilterEffectVector& input_effects = effect->InputEffects();
  input_effects.reserve(2);
  input_effects.push_back(input1);
  input_effects.push_back(input2);
  return effect;
}

SVGAnimatedPropertyBase* SVGFEDisplacementMapElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kScaleAttr) {
    return scale_.Get();
  } else if (attribute_name == svg_names::kInAttr) {
    return in1_.Get();
  } else if (attribute_name == svg_names::kIn2Attr) {
    return in2_.Get();
  } else if (attribute_name == svg_names::kXChannelSelectorAttr) {
    return x_channel_selector_.Get();
  } else if (attribute_name == svg_names::kYChannelSelectorAttr) {
    return y_channel_selector_.Get();
  } else {
    return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
        attribute_name);
  }
}

void SVGFEDisplacementMapElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{scale_.Get(), in1_.Get(), in2_.Get(),
                                   x_channel_selector_.Get(),
                                   y_channel_selector_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink

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

#include "third_party/blink/renderer/core/svg/svg_fe_blend_element.h"

#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_blend.h"

namespace blink {

static BlendMode ToBlendMode(SVGFEBlendElement::Mode mode) {
#define MAP_BLEND_MODE(MODENAME)           \
  case SVGFEBlendElement::kMode##MODENAME: \
    return BlendMode::k##MODENAME

  switch (mode) {
    MAP_BLEND_MODE(Normal);
    MAP_BLEND_MODE(Multiply);
    MAP_BLEND_MODE(Screen);
    MAP_BLEND_MODE(Darken);
    MAP_BLEND_MODE(Lighten);
    MAP_BLEND_MODE(Overlay);
    MAP_BLEND_MODE(ColorDodge);
    MAP_BLEND_MODE(ColorBurn);
    MAP_BLEND_MODE(HardLight);
    MAP_BLEND_MODE(SoftLight);
    MAP_BLEND_MODE(Difference);
    MAP_BLEND_MODE(Exclusion);
    MAP_BLEND_MODE(Hue);
    MAP_BLEND_MODE(Saturation);
    MAP_BLEND_MODE(Color);
    MAP_BLEND_MODE(Luminosity);
    default:
      NOTREACHED();
      return BlendMode::kNormal;
  }
#undef MAP_BLEND_MODE
}

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGFEBlendElement::Mode>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(SVGFEBlendElement::kModeNormal, "normal"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeMultiply, "multiply"));
    entries.push_back(std::make_pair(SVGFEBlendElement::kModeScreen, "screen"));
    entries.push_back(std::make_pair(SVGFEBlendElement::kModeDarken, "darken"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeLighten, "lighten"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeOverlay, "overlay"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeColorDodge, "color-dodge"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeColorBurn, "color-burn"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeHardLight, "hard-light"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeSoftLight, "soft-light"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeDifference, "difference"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeExclusion, "exclusion"));
    entries.push_back(std::make_pair(SVGFEBlendElement::kModeHue, "hue"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeSaturation, "saturation"));
    entries.push_back(std::make_pair(SVGFEBlendElement::kModeColor, "color"));
    entries.push_back(
        std::make_pair(SVGFEBlendElement::kModeLuminosity, "luminosity"));
  }
  return entries;
}

template <>
unsigned short GetMaxExposedEnumValue<SVGFEBlendElement::Mode>() {
  return SVGFEBlendElement::kModeLighten;
}

inline SVGFEBlendElement::SVGFEBlendElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEBlendTag, document),
      in1_(SVGAnimatedString::Create(this, svg_names::kInAttr)),
      in2_(SVGAnimatedString::Create(this, svg_names::kIn2Attr)),
      mode_(SVGAnimatedEnumeration<Mode>::Create(
          this,
          svg_names::kModeAttr,
          SVGFEBlendElement::kModeNormal)) {
  AddToPropertyMap(in1_);
  AddToPropertyMap(in2_);
  AddToPropertyMap(mode_);
}

void SVGFEBlendElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(in1_);
  visitor->Trace(in2_);
  visitor->Trace(mode_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGFEBlendElement)

bool SVGFEBlendElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FEBlend* blend = static_cast<FEBlend*>(effect);
  if (attr_name == svg_names::kModeAttr)
    return blend->SetBlendMode(ToBlendMode(mode_->CurrentValue()->EnumValue()));

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFEBlendElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kModeAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  if (attr_name == svg_names::kInAttr || attr_name == svg_names::kIn2Attr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    Invalidate();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFEBlendElement::Build(SVGFilterBuilder* filter_builder,
                                       Filter* filter) {
  FilterEffect* input1 = filter_builder->GetEffectById(
      AtomicString(in1_->CurrentValue()->Value()));
  FilterEffect* input2 = filter_builder->GetEffectById(
      AtomicString(in2_->CurrentValue()->Value()));
  DCHECK(input1);
  DCHECK(input2);

  FilterEffect* effect =
      FEBlend::Create(filter, ToBlendMode(mode_->CurrentValue()->EnumValue()));
  FilterEffectVector& input_effects = effect->InputEffects();
  input_effects.ReserveCapacity(2);
  input_effects.push_back(input1);
  input_effects.push_back(input2);
  return effect;
}

}  // namespace blink

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

#include "third_party/blink/renderer/core/svg/svg_fe_turbulence_element.h"

#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<SVGStitchOptions>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {kSvgStitchtypeStitch, "stitch"}, {kSvgStitchtypeNostitch, "noStitch"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

template <>
const SVGEnumerationMap& GetEnumerationMap<TurbulenceType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {FETURBULENCE_TYPE_FRACTALNOISE, "fractalNoise"},
      {FETURBULENCE_TYPE_TURBULENCE, "turbulence"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGFETurbulenceElement::SVGFETurbulenceElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFETurbulenceTag,
                                           document),
      base_frequency_(MakeGarbageCollected<SVGAnimatedNumberOptionalNumber>(
          this,
          svg_names::kBaseFrequencyAttr,
          0.0f)),
      seed_(MakeGarbageCollected<SVGAnimatedNumber>(this,
                                                    svg_names::kSeedAttr,
                                                    0.0f)),
      stitch_tiles_(
          MakeGarbageCollected<SVGAnimatedEnumeration<SVGStitchOptions>>(
              this,
              svg_names::kStitchTilesAttr,
              kSvgStitchtypeNostitch)),
      type_(MakeGarbageCollected<SVGAnimatedEnumeration<TurbulenceType>>(
          this,
          svg_names::kTypeAttr,
          FETURBULENCE_TYPE_TURBULENCE)),
      num_octaves_(
          MakeGarbageCollected<SVGAnimatedInteger>(this,
                                                   svg_names::kNumOctavesAttr,
                                                   1)) {
  AddToPropertyMap(base_frequency_);
  AddToPropertyMap(seed_);
  AddToPropertyMap(stitch_tiles_);
  AddToPropertyMap(type_);
  AddToPropertyMap(num_octaves_);
}

void SVGFETurbulenceElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(base_frequency_);
  visitor->Trace(seed_);
  visitor->Trace(stitch_tiles_);
  visitor->Trace(type_);
  visitor->Trace(num_octaves_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
}

bool SVGFETurbulenceElement::SetFilterEffectAttribute(
    FilterEffect* effect,
    const QualifiedName& attr_name) {
  FETurbulence* turbulence = static_cast<FETurbulence*>(effect);
  if (attr_name == svg_names::kTypeAttr)
    return turbulence->SetType(type_->CurrentValue()->EnumValue());
  if (attr_name == svg_names::kStitchTilesAttr)
    return turbulence->SetStitchTiles(
        stitch_tiles_->CurrentValue()->EnumValue() == kSvgStitchtypeStitch);
  if (attr_name == svg_names::kBaseFrequencyAttr) {
    bool base_frequency_x_changed = turbulence->SetBaseFrequencyX(
        baseFrequencyX()->CurrentValue()->Value());
    bool base_frequency_y_changed = turbulence->SetBaseFrequencyY(
        baseFrequencyY()->CurrentValue()->Value());
    return (base_frequency_x_changed || base_frequency_y_changed);
  }
  if (attr_name == svg_names::kSeedAttr)
    return turbulence->SetSeed(seed_->CurrentValue()->Value());
  if (attr_name == svg_names::kNumOctavesAttr)
    return turbulence->SetNumOctaves(num_octaves_->CurrentValue()->Value());

  return SVGFilterPrimitiveStandardAttributes::SetFilterEffectAttribute(
      effect, attr_name);
}

void SVGFETurbulenceElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kBaseFrequencyAttr ||
      attr_name == svg_names::kNumOctavesAttr ||
      attr_name == svg_names::kSeedAttr ||
      attr_name == svg_names::kStitchTilesAttr ||
      attr_name == svg_names::kTypeAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    PrimitiveAttributeChanged(attr_name);
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(attr_name);
}

FilterEffect* SVGFETurbulenceElement::Build(SVGFilterBuilder*, Filter* filter) {
  return MakeGarbageCollected<FETurbulence>(
      filter, type_->CurrentValue()->EnumValue(),
      baseFrequencyX()->CurrentValue()->Value(),
      baseFrequencyY()->CurrentValue()->Value(),
      num_octaves_->CurrentValue()->Value(), seed_->CurrentValue()->Value(),
      stitch_tiles_->CurrentValue()->EnumValue() == kSvgStitchtypeStitch);
}

}  // namespace blink

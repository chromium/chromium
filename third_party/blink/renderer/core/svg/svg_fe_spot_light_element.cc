/*
 * Copyright (C) 2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>
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

#include "third_party/blink/renderer/core/svg/svg_fe_spot_light_element.h"

#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/spot_light_source.h"

namespace blink {

SVGFESpotLightElement::SVGFESpotLightElement(Document& document)
    : SVGFELightElement(svg_names::kFESpotLightTag, document) {}

scoped_refptr<LightSource> SVGFESpotLightElement::GetLightSource(
    Filter* filter) const {
  return SpotLightSource::Create(filter->Resolve3dPoint(GetPosition()),
                                 filter->Resolve3dPoint(PointsAt()),
                                 specularExponent()->CurrentValue()->Value(),
                                 limitingConeAngle()->CurrentValue()->Value());
}

}  // namespace blink

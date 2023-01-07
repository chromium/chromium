/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_specular_lighting.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/graphics/filters/light_source.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

FESpecularLighting::FESpecularLighting(Filter* filter,
                                       const Color& lighting_color,
                                       float surface_scale,
                                       float specular_constant,
                                       float specular_exponent,
                                       scoped_refptr<LightSource> light_source)
    : FELighting(filter,
                 kSpecularLighting,
                 lighting_color,
                 surface_scale,
                 0,
                 specular_constant,
                 specular_exponent,
                 std::move(light_source)) {}

FESpecularLighting::~FESpecularLighting() = default;

float FESpecularLighting::SpecularConstant() const {
  return specular_constant_;
}

bool FESpecularLighting::SetSpecularConstant(float specular_constant) {
  specular_constant = std::max(specular_constant, 0.0f);
  if (specular_constant_ == specular_constant)
    return false;
  specular_constant_ = specular_constant;
  return true;
}

float FESpecularLighting::SpecularExponent() const {
  return specular_exponent_;
}

bool FESpecularLighting::SetSpecularExponent(float specular_exponent) {
  specular_exponent = ClampTo(specular_exponent, 1.0f, 128.0f);
  if (specular_exponent_ == specular_exponent)
    return false;
  specular_exponent_ = specular_exponent;
  return true;
}

WTF::TextStream& FESpecularLighting::ExternalRepresentation(WTF::TextStream& ts,
                                                            int indent) const {
  WriteIndent(ts, indent);
  ts << "[feSpecularLighting";
  FilterEffect::ExternalRepresentation(ts);
  ts << " surfaceScale=\"" << surface_scale_ << "\" "
     << "specualConstant=\"" << specular_constant_ << "\" "
     << "specularExponent=\"" << specular_exponent_ << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink

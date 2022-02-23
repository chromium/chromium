/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/filters/spot_light_source.h"

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

bool SpotLightSource::SetPosition(const gfx::Point3F& position) {
  if (position_ == position)
    return false;
  position_ = position;
  return true;
}

bool SpotLightSource::SetPointsAt(const gfx::Point3F& points_at) {
  if (points_at_ == points_at)
    return false;
  points_at_ = points_at;
  return true;
}

bool SpotLightSource::SetSpecularExponent(float specular_exponent) {
  specular_exponent = ClampTo(specular_exponent, 1.0f, 128.0f);
  if (specular_exponent_ == specular_exponent)
    return false;
  specular_exponent_ = specular_exponent;
  return true;
}

bool SpotLightSource::SetLimitingConeAngle(float limiting_cone_angle) {
  if (limiting_cone_angle_ == limiting_cone_angle)
    return false;
  limiting_cone_angle_ = limiting_cone_angle;
  return true;
}

WTF::TextStream& SpotLightSource::ExternalRepresentation(
    WTF::TextStream& ts) const {
  ts << "[type=SPOT-LIGHT] ";
  ts << "[position=\"" << GetPosition().ToString() << "\"]";
  ts << "[pointsAt=\"" << PointsAt().ToString() << "\"]";
  ts << "[specularExponent=\"" << SpecularExponent() << "\"]";
  ts << "[limitingConeAngle=\"" << LimitingConeAngle() << "\"]";
  return ts;
}

}  // namespace blink

/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/audio/cone_effect.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace blink {

ConeEffect::ConeEffect()
    : inner_angle_(360.0), outer_angle_(360.0), outer_gain_(0.0) {}

double ConeEffect::Gain(gfx::Point3F source_position,
                        gfx::Vector3dF source_orientation,
                        gfx::Point3F listener_position) {
  if (source_orientation.IsZero() ||
      ((inner_angle_ == 360.0) && (outer_angle_ == 360.0))) {
    return 1.0;  // no cone specified - unity gain
  }

  // Source-listener vector
  gfx::Vector3dF source_to_listener = listener_position - source_position;

  // Angle between the source orientation vector and the source-listener vector
  double angle =
      gfx::AngleBetweenVectorsInDegrees(source_to_listener, source_orientation);
  double abs_angle = fabs(angle);

  // Divide by 2.0 here since API is entire angle (not half-angle)
  double abs_inner_angle = fabs(inner_angle_) / 2.0;
  double abs_outer_angle = fabs(outer_angle_) / 2.0;
  double gain = 1.0;

  if (abs_angle <= abs_inner_angle) {
    // No attenuation
    gain = 1.0;
  } else if (abs_angle >= abs_outer_angle) {
    // Max attenuation
    gain = outer_gain_;
  } else {
    // Between inner and outer cones
    // inner -> outer, x goes from 0 -> 1
    double x =
        (abs_angle - abs_inner_angle) / (abs_outer_angle - abs_inner_angle);
    gain = (1.0 - x) + outer_gain_ * x;
  }

  return gain;
}

}  // namespace blink

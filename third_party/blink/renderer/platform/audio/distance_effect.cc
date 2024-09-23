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

#include "third_party/blink/renderer/platform/audio/distance_effect.h"

#include <math.h>
#include <algorithm>
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/fdlibm/ieee754.h"

namespace blink {

DistanceEffect::DistanceEffect()
    : model_(kModelInverse),
      ref_distance_(1.0),
      max_distance_(10000.0),
      rolloff_factor_(1.0) {}

double DistanceEffect::Gain(double distance) {
  switch (model_) {
    case kModelLinear:
      return LinearGain(distance);
    case kModelInverse:
      return InverseGain(distance);
    case kModelExponential:
      return ExponentialGain(distance);
  }
  NOTREACHED_IN_MIGRATION();
  return 0.0;
}

double DistanceEffect::LinearGain(double distance) {
  // Clamp refDistance and distance according to the spec.
  double dref = std::min(ref_distance_, max_distance_);
  double dmax = std::max(ref_distance_, max_distance_);
  distance = ClampTo(distance, dref, dmax);

  if (dref == dmax) {
    return 1 - rolloff_factor_;
  }

  // We want a gain that decreases linearly from m_refDistance to
  // m_maxDistance. The gain is 1 at m_refDistance.
  return (1.0 - ClampTo(rolloff_factor_, 0.0, 1.0) * (distance - dref) /
                    (dmax - dref));
}

double DistanceEffect::InverseGain(double distance) {
  if (ref_distance_ == 0) {
    return 0;
  }

  // Clamp distance according to spec
  distance = ClampTo(distance, ref_distance_);

  return ref_distance_ / (ref_distance_ + ClampTo(rolloff_factor_, 0.0) *
                                              (distance - ref_distance_));
}

double DistanceEffect::ExponentialGain(double distance) {
  if (ref_distance_ == 0) {
    return 0;
  }

  // Clamp distance according to spec
  distance = ClampTo(distance, ref_distance_);

  return fdlibm::pow(distance / ref_distance_, -ClampTo(rolloff_factor_, 0.0));
}

}  // namespace blink

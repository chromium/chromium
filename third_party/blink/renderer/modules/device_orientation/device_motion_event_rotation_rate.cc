/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_rotation_rate.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_motion_event_rotation_rate_init.h"

namespace blink {

DeviceMotionEventRotationRate*
DeviceMotionEventRotationRate::Create(double alpha, double beta, double gamma) {
  return MakeGarbageCollected<DeviceMotionEventRotationRate>(alpha, beta,
                                                             gamma);
}

DeviceMotionEventRotationRate* DeviceMotionEventRotationRate::Create(
    const DeviceMotionEventRotationRateInit* init) {
  double alpha = init->hasAlphaNonNull() ? init->alphaNonNull() : NAN;
  double beta = init->hasBetaNonNull() ? init->betaNonNull() : NAN;
  double gamma = init->hasGammaNonNull() ? init->gammaNonNull() : NAN;
  return DeviceMotionEventRotationRate::Create(alpha, beta, gamma);
}

DeviceMotionEventRotationRate::DeviceMotionEventRotationRate(double alpha,
                                                             double beta,
                                                             double gamma)
    : alpha_(alpha), beta_(beta), gamma_(gamma) {}

bool DeviceMotionEventRotationRate::HasRotationData() const {
  return !std::isnan(alpha_) || !std::isnan(beta_) || !std::isnan(gamma_);
}

std::optional<double> DeviceMotionEventRotationRate::alpha() const {
  if (std::isnan(alpha_))
    return std::nullopt;
  return alpha_;
}

std::optional<double> DeviceMotionEventRotationRate::beta() const {
  if (std::isnan(beta_))
    return std::nullopt;
  return beta_;
}

std::optional<double> DeviceMotionEventRotationRate::gamma() const {
  if (std::isnan(gamma_))
    return std::nullopt;
  return gamma_;
}

}  // namespace blink

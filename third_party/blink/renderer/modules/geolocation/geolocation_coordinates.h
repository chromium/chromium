/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEOLOCATION_COORDINATES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEOLOCATION_COORDINATES_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class GeolocationCoordinates : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  GeolocationCoordinates(double latitude,
                         double longitude,
                         bool provides_altitude,
                         double altitude,
                         double accuracy,
                         bool provides_altitude_accuracy,
                         double altitude_accuracy,
                         bool provides_heading,
                         double heading,
                         bool provides_speed,
                         double speed)
      : latitude_(latitude),
        longitude_(longitude),
        altitude_(altitude),
        accuracy_(accuracy),
        altitude_accuracy_(altitude_accuracy),
        heading_(heading),
        speed_(speed),
        can_provide_altitude_(provides_altitude),
        can_provide_altitude_accuracy_(provides_altitude_accuracy),
        can_provide_heading_(provides_heading),
        can_provide_speed_(provides_speed) {}

  double latitude() const { return latitude_; }
  double longitude() const { return longitude_; }
  double altitude(bool& is_null) const;
  double accuracy() const { return accuracy_; }
  double altitudeAccuracy(bool& is_null) const;
  double heading(bool& is_null) const;
  double speed(bool& is_null) const;

 private:
  double latitude_;
  double longitude_;
  double altitude_;
  double accuracy_;
  double altitude_accuracy_;
  double heading_;
  double speed_;

  bool can_provide_altitude_;
  bool can_provide_altitude_accuracy_;
  bool can_provide_heading_;
  bool can_provide_speed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GEOLOCATION_GEOLOCATION_COORDINATES_H_

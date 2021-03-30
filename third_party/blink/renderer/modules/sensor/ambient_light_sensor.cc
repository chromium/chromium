// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/ambient_light_sensor.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

using device::mojom::blink::SensorType;

namespace blink {

namespace {

// Even though the underlying value has changed, for ALS we provide readouts to
// JS to the nearest 50 Lux.
constexpr int kAlsRoundingThreshold = 50;

// Decrease precision of ALS readouts.
// Round off to the nearest kAlsRoundingThreshold.
double RoundIlluminance(double value) {
  return kAlsRoundingThreshold * std::round(value / kAlsRoundingThreshold);
}

// Value will have to vary by at least half the rounding threshold before it has
// an effect on the output.
bool IsSignificantlyDifferent(double als_old, double als_new) {
  return std::fabs(als_old - als_new) >= kAlsRoundingThreshold / 2;
}

}  // namespace

// static
AmbientLightSensor* AmbientLightSensor::Create(
    ExecutionContext* execution_context,
    const SensorOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<AmbientLightSensor>(execution_context, options,
                                                  exception_state);
}

// static
AmbientLightSensor* AmbientLightSensor::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return Create(execution_context, SensorOptions::Create(), exception_state);
}

AmbientLightSensor::AmbientLightSensor(ExecutionContext* execution_context,
                                       const SensorOptions* options,
                                       ExceptionState& exception_state)
    : Sensor(execution_context,
             options,
             exception_state,
             SensorType::AMBIENT_LIGHT,
             {mojom::blink::PermissionsPolicyFeature::kAmbientLightSensor}) {}

bool AmbientLightSensor::hasReading() const {
  return latest_reading_.has_value() && Sensor::hasReading();
}

base::Optional<double> AmbientLightSensor::illuminance() const {
  if (hasReading()) {
    DCHECK(latest_reading_.has_value());
    return RoundIlluminance(latest_reading_.value());
  }
  return base::nullopt;
}

// When the reading we get does not differ significantly from our current
// value, we discard this reading and do not emit any events. This is a privacy
// measure to avoid giving readings that are too specific.
void AmbientLightSensor::OnSensorReadingChanged() {
  // The platform sensor may start sending readings before the sensor is fully
  // activated on the Blink side. In this case, bail out early, otherwise we
  // will set |latest_reading_| and not send a "reading" event.
  if (!activated())
    return;

  const double new_reading = GetReading().als.value;
  if (latest_reading_.has_value() &&
      !IsSignificantlyDifferent(*latest_reading_, new_reading)) {
    return;
  }

  latest_reading_ = new_reading;
  Sensor::OnSensorReadingChanged();
}

}  // namespace blink

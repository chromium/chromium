// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/ambient_light_sensor.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"

using device::mojom::blink::SensorType;

namespace blink {

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

std::optional<double> AmbientLightSensor::illuminance() const {
  if (hasReading())
    return GetReading().als.value;
  return std::nullopt;
}

}  // namespace blink

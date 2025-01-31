// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/linear_acceleration_sensor.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"

using device::mojom::blink::SensorType;

namespace blink {

LinearAccelerationSensor* LinearAccelerationSensor::Create(
    ExecutionContext* execution_context,
    const SpatialSensorOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<LinearAccelerationSensor>(
      execution_context, options, exception_state);
}

// static
LinearAccelerationSensor* LinearAccelerationSensor::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return Create(execution_context, SpatialSensorOptions::Create(),
                exception_state);
}

LinearAccelerationSensor::LinearAccelerationSensor(
    ExecutionContext* execution_context,
    const SpatialSensorOptions* options,
    ExceptionState& exception_state)
    : Accelerometer(execution_context,
                    options,
                    exception_state,
                    SensorType::LINEAR_ACCELERATION,
                    {mojom::blink::PermissionsPolicyFeature::kAccelerometer}) {}

void LinearAccelerationSensor::Trace(Visitor* visitor) const {
  Accelerometer::Trace(visitor);
}

}  // namespace blink

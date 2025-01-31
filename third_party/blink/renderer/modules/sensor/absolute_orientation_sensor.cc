// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/absolute_orientation_sensor.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"

using device::mojom::blink::SensorType;

namespace blink {

AbsoluteOrientationSensor* AbsoluteOrientationSensor::Create(
    ExecutionContext* execution_context,
    const SpatialSensorOptions* options,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<AbsoluteOrientationSensor>(
      execution_context, options, exception_state);
}

// static
AbsoluteOrientationSensor* AbsoluteOrientationSensor::Create(
    ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  return Create(execution_context, SpatialSensorOptions::Create(),
                exception_state);
}

AbsoluteOrientationSensor::AbsoluteOrientationSensor(
    ExecutionContext* execution_context,
    const SpatialSensorOptions* options,
    ExceptionState& exception_state)
    : OrientationSensor(
          execution_context,
          options,
          exception_state,
          SensorType::ABSOLUTE_ORIENTATION_QUATERNION,
          {mojom::blink::PermissionsPolicyFeature::kAccelerometer,
           mojom::blink::PermissionsPolicyFeature::kGyroscope,
           mojom::blink::PermissionsPolicyFeature::kMagnetometer}) {}

void AbsoluteOrientationSensor::Trace(Visitor* visitor) const {
  OrientationSensor::Trace(visitor);
}

}  // namespace blink

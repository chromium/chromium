// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/gyroscope.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"

using device::mojom::blink::SensorType;

namespace blink {

Gyroscope* Gyroscope::Create(ExecutionContext* execution_context,
                             const SpatialSensorOptions* options,
                             ExceptionState& exception_state) {
  return MakeGarbageCollected<Gyroscope>(execution_context, options,
                                         exception_state);
}

// static
Gyroscope* Gyroscope::Create(ExecutionContext* execution_context,
                             ExceptionState& exception_state) {
  return Create(execution_context, SpatialSensorOptions::Create(),
                exception_state);
}

Gyroscope::Gyroscope(ExecutionContext* execution_context,
                     const SpatialSensorOptions* options,
                     ExceptionState& exception_state)
    : Sensor(execution_context,
             options,
             exception_state,
             SensorType::GYROSCOPE,
             {mojom::blink::PermissionsPolicyFeature::kGyroscope}) {}

std::optional<double> Gyroscope::x() const {
  if (hasReading())
    return GetReading().gyro.x;
  return std::nullopt;
}

std::optional<double> Gyroscope::y() const {
  if (hasReading())
    return GetReading().gyro.y;
  return std::nullopt;
}

std::optional<double> Gyroscope::z() const {
  if (hasReading())
    return GetReading().gyro.z;
  return std::nullopt;
}

void Gyroscope::Trace(Visitor* visitor) const {
  Sensor::Trace(visitor);
}

}  // namespace blink

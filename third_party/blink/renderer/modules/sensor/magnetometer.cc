// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/magnetometer.h"

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
Magnetometer* Magnetometer::Create(ExecutionContext* execution_context,
                                   const SpatialSensorOptions* options,
                                   ExceptionState& exception_state) {
  return MakeGarbageCollected<Magnetometer>(execution_context, options,
                                            exception_state);
}

// static
Magnetometer* Magnetometer::Create(ExecutionContext* execution_context,
                                   ExceptionState& exception_state) {
  return Create(execution_context, SpatialSensorOptions::Create(),
                exception_state);
}

Magnetometer::Magnetometer(ExecutionContext* execution_context,
                           const SpatialSensorOptions* options,
                           ExceptionState& exception_state)
    : Sensor(execution_context,
             options,
             exception_state,
             SensorType::MAGNETOMETER,
             {mojom::blink::PermissionsPolicyFeature::kMagnetometer}) {}

std::optional<double> Magnetometer::x() const {
  if (hasReading())
    return GetReading().magn.x;
  return std::nullopt;
}

std::optional<double> Magnetometer::y() const {
  if (hasReading())
    return GetReading().magn.y;
  return std::nullopt;
}

std::optional<double> Magnetometer::z() const {
  if (hasReading())
    return GetReading().magn.z;
  return std::nullopt;
}

void Magnetometer::Trace(Visitor* visitor) const {
  Sensor::Trace(visitor);
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/accelerometer.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"

using device::mojom::blink::SensorType;

namespace blink {

Accelerometer* Accelerometer::Create(ExecutionContext* execution_context,
                                     const SpatialSensorOptions* options,
                                     ExceptionState& exception_state) {
  const Vector<mojom::FeaturePolicyFeature> features(
      {mojom::FeaturePolicyFeature::kAccelerometer});
  return MakeGarbageCollected<Accelerometer>(
      execution_context, options, exception_state, SensorType::ACCELEROMETER,
      features);
}

// static
Accelerometer* Accelerometer::Create(ExecutionContext* execution_context,
                                     ExceptionState& exception_state) {
  return Create(execution_context, SpatialSensorOptions::Create(),
                exception_state);
}

Accelerometer::Accelerometer(
    ExecutionContext* execution_context,
    const SpatialSensorOptions* options,
    ExceptionState& exception_state,
    SensorType sensor_type,
    const Vector<mojom::FeaturePolicyFeature>& features)
    : Sensor(execution_context,
             options,
             exception_state,
             sensor_type,
             features) {}

double Accelerometer::x(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return GetReading().accel.x;
}

double Accelerometer::y(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return GetReading().accel.y;
}

double Accelerometer::z(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return GetReading().accel.z;
}

void Accelerometer::Trace(blink::Visitor* visitor) {
  Sensor::Trace(visitor);
}

}  // namespace blink

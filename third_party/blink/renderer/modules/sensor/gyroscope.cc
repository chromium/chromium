// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sensor/gyroscope.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"

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
             {mojom::FeaturePolicyFeature::kGyroscope}) {}

double Gyroscope::x(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return GetReading().gyro.x;
}

double Gyroscope::y(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return GetReading().gyro.y;
}

double Gyroscope::z(bool& is_null) const {
  INIT_IS_NULL_AND_RETURN(is_null, 0.0);
  return GetReading().gyro.z;
}

void Gyroscope::Trace(blink::Visitor* visitor) {
  Sensor::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/sensor/orientation_sensor.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_dommatrix_float32array_float64array.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

using device::mojom::blink::SensorType;

namespace blink {

std::optional<Vector<double>> OrientationSensor::quaternion() {
  reading_dirty_ = false;
  if (!hasReading())
    return std::nullopt;
  const auto& quat = GetReading().orientation_quat;
  return Vector<double>({quat.x, quat.y, quat.z, quat.w});
}

template <typename T>
void DoPopulateMatrix(T* target_matrix,
                      double x,
                      double y,
                      double z,
                      double w) {
  auto out = target_matrix->Data();
  out[0] = 1.0 - 2 * (y * y + z * z);
  out[1] = 2 * (x * y - z * w);
  out[2] = 2 * (x * z + y * w);
  out[3] = 0.0;
  out[4] = 2 * (x * y + z * w);
  out[5] = 1.0 - 2 * (x * x + z * z);
  out[6] = 2 * (y * z - x * w);
  out[7] = 0.0;
  out[8] = 2 * (x * z - y * w);
  out[9] = 2 * (y * z + x * w);
  out[10] = 1.0 - 2 * (x * x + y * y);
  out[11] = 0.0;
  out[12] = 0.0;
  out[13] = 0.0;
  out[14] = 0.0;
  out[15] = 1.0;
}

template <>
void DoPopulateMatrix(DOMMatrix* target_matrix,
                      double x,
                      double y,
                      double z,
                      double w) {
  target_matrix->setM11(1.0 - 2 * (y * y + z * z));
  target_matrix->setM12(2 * (x * y - z * w));
  target_matrix->setM13(2 * (x * z + y * w));
  target_matrix->setM14(0.0);
  target_matrix->setM21(2 * (x * y + z * w));
  target_matrix->setM22(1.0 - 2 * (x * x + z * z));
  target_matrix->setM23(2 * y * z - 2 * x * w);
  target_matrix->setM24(0.0);
  target_matrix->setM31(2 * (x * z - y * w));
  target_matrix->setM32(2 * (y * z + x * w));
  target_matrix->setM33(1.0 - 2 * (x * x + y * y));
  target_matrix->setM34(0.0);
  target_matrix->setM41(0.0);
  target_matrix->setM42(0.0);
  target_matrix->setM43(0.0);
  target_matrix->setM44(1.0);
}

template <typename T>
bool CheckBufferLength(T* buffer) {
  return buffer->length() >= 16;
}

template <>
bool CheckBufferLength(DOMMatrix*) {
  return true;
}

template <typename Matrix>
void OrientationSensor::PopulateMatrixInternal(
    Matrix* target_matrix,
    ExceptionState& exception_state) {
  if (!CheckBufferLength(target_matrix)) {
    exception_state.ThrowTypeError(
        "Target buffer must have at least 16 elements.");
    return;
  }
  if (!hasReading()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                      "Sensor data is not available.");
    return;
  }

  const auto& quat = GetReading().orientation_quat;

  DoPopulateMatrix(target_matrix, quat.x, quat.y, quat.z, quat.w);
}

void OrientationSensor::populateMatrix(
    const V8RotationMatrixType* target_buffer,
    ExceptionState& exception_state) {
  switch (target_buffer->GetContentType()) {
    case V8RotationMatrixType::ContentType::kDOMMatrix:
      PopulateMatrixInternal(target_buffer->GetAsDOMMatrix(), exception_state);
      break;
    case V8RotationMatrixType::ContentType::kFloat32Array:
      PopulateMatrixInternal(target_buffer->GetAsFloat32Array().Get(),
                             exception_state);
      break;
    case V8RotationMatrixType::ContentType::kFloat64Array:
      PopulateMatrixInternal(target_buffer->GetAsFloat64Array().Get(),
                             exception_state);
      break;
  }
}

bool OrientationSensor::isReadingDirty() const {
  return reading_dirty_ || !hasReading();
}

OrientationSensor::OrientationSensor(
    ExecutionContext* execution_context,
    const SpatialSensorOptions* options,
    ExceptionState& exception_state,
    device::mojom::blink::SensorType type,
    const Vector<mojom::blink::PermissionsPolicyFeature>& features)
    : Sensor(execution_context, options, exception_state, type, features),
      reading_dirty_(true) {}

void OrientationSensor::OnSensorReadingChanged() {
  reading_dirty_ = true;
  Sensor::OnSensorReadingChanged();
}

void OrientationSensor::Trace(Visitor* visitor) const {
  Sensor::Trace(visitor);
}

}  // namespace blink

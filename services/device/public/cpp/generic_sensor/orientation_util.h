// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_ORIENTATION_UTIL_H_
#define SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_ORIENTATION_UTIL_H_

#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

// Sets |out_reading|'s |orientation_quat| member to a quaternion corresponding
// to the set of intrinsic Tait-Bryan Euler angles passed in the parameters.
//
// Returns false if |alpha|, |beta|, or |gamma| are outside the ranges expected
// by the Device Orientation API specification.
//
// Note: The timestamp should be set by the caller.
bool ComputeQuaternionFromEulerAngles(double alpha,
                                      double beta,
                                      double gamma,
                                      SensorReading* out_reading);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_GENERIC_SENSOR_ORIENTATION_UTIL_H_

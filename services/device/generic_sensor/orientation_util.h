// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_UTIL_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_UTIL_H_

#include <vector>

namespace device {

void ComputeOrientationEulerAnglesFromRotationMatrix(
    const std::vector<double>& r,
    double* alpha_in_degrees,
    double* beta_in_degrees,
    double* gamma_in_degrees);

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_UTIL_H_

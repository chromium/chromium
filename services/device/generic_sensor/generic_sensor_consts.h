// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_GENERIC_SENSOR_CONSTS_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_GENERIC_SENSOR_CONSTS_H_

namespace device {

// If two doubles differ by less than this amount, we can consider them
// to be effectively equal.
constexpr double kEpsilon = 1e-8;

// Required for conversion from Gauss to uT.
constexpr double kMicroteslaInGauss = 100.0;

// Required for conversion from Milligauss to Microtesla.
constexpr double kMicroteslaInMilligauss = 0.1;

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_GENERIC_SENSOR_CONSTS_H_

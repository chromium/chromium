// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_TEST_DATA_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_TEST_DATA_H_

#include <cmath>
#include <vector>

namespace device {

namespace {

const double kRootTwoOverTwo = std::sqrt(2.0) / 2.0;

}  // namespace

// The quaternion representation of an entry in
// |euler_angles_in_degrees_test_values| is stored in the same index in
// |quaternions_test_values|, and vice-versa.

// The values in each three-element entry are: alpha, beta, gamma.
const std::vector<std::vector<double>> euler_angles_in_degrees_test_values = {
    {0.0, -180.0, -90.0},   {0.0, -180.0, 0.0},   {0.0, -90.0, 0.0},
    {0.0, 0.0, -90.0},      {0.0, 0.0, 0.0},      {0.0, 90.0, 0.0},
    {90.0, -180.0, -90.0},  {90.0, -180.0, 0.0},  {90.0, -90.0, 0.0},
    {90.0, 0.0, -90.0},     {90.0, 0.0, 0.0},     {90.0, 90.0, 0.0},
    {180.0, -180.0, -90.0}, {180.0, -180.0, 0.0}, {180.0, -90.0, 0.0},
    {180.0, 0.0, -90.0},    {180.0, 0.0, 0.0},    {180.0, 90.0, 0.0},
    {270.0, -180.0, -90.0}, {270.0, -180.0, 0.0}, {270.0, -90.0, 0.0},
    {270.0, 0.0, -90.0},    {270.0, 0.0, 0.0},    {270.0, 90.0, 0.0}};

// The values in each four-element entry are: x, y, z, w.
const std::vector<std::vector<double>> quaternions_test_values = {
    {-kRootTwoOverTwo, 0.0, kRootTwoOverTwo, 0.0},
    {-1, 0.0, 0.0, 0.0},
    {-kRootTwoOverTwo, 0.0, 0.0, kRootTwoOverTwo},
    {0.0, -kRootTwoOverTwo, 0.0, kRootTwoOverTwo},
    {0.0, 0.0, 0.0, 1.0},
    {kRootTwoOverTwo, 0.0, 0.0, kRootTwoOverTwo},
    {-0.5, -0.5, 0.5, -0.5},
    {-kRootTwoOverTwo, -kRootTwoOverTwo, 0.0, 0.0},
    {-0.5, -0.5, 0.5, 0.5},
    {0.5, -0.5, 0.5, 0.5},
    {0.0, 0.0, kRootTwoOverTwo, kRootTwoOverTwo},
    {0.5, 0.5, 0.5, 0.5},
    {0.0, -kRootTwoOverTwo, 0.0, -kRootTwoOverTwo},
    {0.0, -1.0, 0.0, 0.0},
    {0.0, -kRootTwoOverTwo, kRootTwoOverTwo, 0.0},
    {kRootTwoOverTwo, 0.0, kRootTwoOverTwo, 0.0},
    {0.0, 0.0, 1.0, 0.0},
    {0.0, kRootTwoOverTwo, kRootTwoOverTwo, 0.0},
    {0.5, -0.5, -0.5, -0.5},
    {kRootTwoOverTwo, -kRootTwoOverTwo, 0.0, 0.0},
    {0.5, -0.5, 0.5, -0.5},
    {0.5, 0.5, 0.5, -0.5},
    {0.0, 0.0, kRootTwoOverTwo, -kRootTwoOverTwo},
    {-0.5, 0.5, 0.5, -0.5}};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_ORIENTATION_TEST_DATA_H_

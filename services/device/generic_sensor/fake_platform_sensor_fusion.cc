// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/fake_platform_sensor_fusion.h"

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

FakePlatformSensorFusion::FakePlatformSensorFusion(
    std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm)
    : PlatformSensorFusion(nullptr,
                           nullptr,
                           std::move(fusion_algorithm),
                           SourcesMap()) {}

bool FakePlatformSensorFusion::GetSourceReading(mojom::SensorType type,
                                                SensorReading* result) {
  auto it = sensor_readings_.find(type);
  EXPECT_TRUE(it != sensor_readings_.end());

  if (it == sensor_readings_.end())
    return false;

  if (!it->second.second)
    return false;

  *result = it->second.first;
  return true;
}

void FakePlatformSensorFusion::SetSensorReading(mojom::SensorType type,
                                                SensorReading reading,
                                                bool sensor_reading_success) {
  sensor_readings_[type] = std::make_pair(reading, sensor_reading_success);
}

FakePlatformSensorFusion::~FakePlatformSensorFusion() = default;

}  // namespace device

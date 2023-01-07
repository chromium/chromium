// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_FUSION_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_FUSION_H_

#include <utility>

#include "base/containers/flat_map.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"

namespace device {

class FakePlatformSensorFusion : public PlatformSensorFusion {
 public:
  explicit FakePlatformSensorFusion(
      std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm);

  FakePlatformSensorFusion(const FakePlatformSensorFusion&) = delete;
  FakePlatformSensorFusion& operator=(const FakePlatformSensorFusion&) = delete;

  // PlatformSensorFusion:
  bool GetSourceReading(mojom::SensorType type, SensorReading* result) override;

  void SetSensorReading(mojom::SensorType type,
                        SensorReading reading,
                        bool sensor_reading_success);

 protected:
  ~FakePlatformSensorFusion() override;

 private:
  base::flat_map<mojom::SensorType, std::pair<SensorReading, bool>>
      sensor_readings_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_FAKE_PLATFORM_SENSOR_FUSION_H_

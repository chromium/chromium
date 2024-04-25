// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "services/device/generic_sensor/platform_sensor.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

class PlatformSensorFusionAlgorithm;

// Implementation of a platform sensor using sensor fusion. There will be a
// instance of this fusion sensor per browser process which is created by
// the PlatformSensorProvider. If there are no clients, this instance is not
// created.
//
// This class implements the generic concept of sensor fusion. It implements
// a new sensor using data from one or more existing sensors. For example,
// it can implement a *_EULER_ANGLES orientation sensor using a
// *_QUATERNION orientation sensor, or vice-versa.
//
// It can also implement an orientation sensor using an ACCELEROMETER, etc.
class PlatformSensorFusion : public PlatformSensor,
                             public PlatformSensor::Client {
 public:
  // Construct a platform fusion sensor of type |fusion_sensor_type| using
  // one or more sensors whose sensor types are |source_sensor_types|, given
  // a buffer |mapping| where readings will be written.
  // The result of this method is passed asynchronously through the given
  // |callback| call: it can be either newly created object on success or
  // nullptr on failure.
  static void Create(
      base::WeakPtr<PlatformSensorProvider> provider,
      std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
      PlatformSensorProvider::CreateSensorCallback callback);

  PlatformSensorFusion(const PlatformSensorFusion&) = delete;
  PlatformSensorFusion& operator=(const PlatformSensorFusion&) = delete;

  // PlatformSensor:
  mojom::ReportingMode GetReportingMode() override;
  PlatformSensorConfiguration GetDefaultConfiguration() override;
  bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) override;
  double GetMaximumSupportedFrequency() override;
  double GetMinimumSupportedFrequency() override;

  // PlatformSensor::Client:
  void OnSensorReadingChanged(mojom::SensorType type) override;
  void OnSensorError() override;
  bool IsSuspended() override;

  virtual bool GetSourceReading(mojom::SensorType type, SensorReading* result);
  bool IsSignificantlyDifferent(const SensorReading& reading1,
                                const SensorReading& reading2,
                                mojom::SensorType sensor_type) override;

 protected:
  class Factory;
  using SourcesMap =
      base::flat_map<mojom::SensorType, scoped_refptr<PlatformSensor>>;
  PlatformSensorFusion(
      SensorReadingSharedBuffer* reading_buffer,
      base::WeakPtr<PlatformSensorProvider> provider,
      std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
      SourcesMap sources);
  ~PlatformSensorFusion() override;
  bool StartSensor(const PlatformSensorConfiguration& configuration) override;
  void StopSensor() override;

  PlatformSensorFusionAlgorithm* fusion_algorithm() const {
    return fusion_algorithm_.get();
  }

  FRIEND_TEST_ALL_PREFIXES(PlatformSensorFusionTest, OnSensorReadingChanged);
  FRIEND_TEST_ALL_PREFIXES(PlatformSensorFusionTest,
                           FusionIsSignificantlyDifferent);

 private:
  std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm_;
  SourcesMap source_sensors_;
  mojom::ReportingMode reporting_mode_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_H_

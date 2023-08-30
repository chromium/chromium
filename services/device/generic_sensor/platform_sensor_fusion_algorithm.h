// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_ALGORITHM_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_ALGORITHM_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom-shared.h"

namespace device {

class PlatformSensorFusion;

// Base class for platform sensor fusion algorithm.
class PlatformSensorFusionAlgorithm {
 public:
  PlatformSensorFusionAlgorithm(const PlatformSensorFusionAlgorithm&) = delete;
  PlatformSensorFusionAlgorithm& operator=(
      const PlatformSensorFusionAlgorithm&) = delete;

  virtual ~PlatformSensorFusionAlgorithm();

  void set_threshold(double threshold) { threshold_ = threshold; }
  double threshold() const { return threshold_; }

  void set_fusion_sensor(PlatformSensorFusion* fusion_sensor) {
    fusion_sensor_ = fusion_sensor;
  }

  const base::flat_set<mojom::SensorType>& source_types() const {
    return source_types_;
  }

  mojom::SensorType fused_type() const { return fused_type_; }

  bool GetFusedData(mojom::SensorType which_sensor_changed,
                    SensorReading* fused_reading);

  // Sets frequency at which data is expected to be obtained from the platform.
  // This might be needed e.g., to calculate cut-off frequency of low/high-pass
  // filters.
  virtual void SetFrequency(double frequency);

  // Algorithms that use statistical data (moving average, Kalman filter, etc),
  // might need to be reset when sensor is stopped.
  virtual void Reset();

 protected:
  PlatformSensorFusionAlgorithm(
      mojom::SensorType fused_type,
      const base::flat_set<mojom::SensorType>& source_types);

  virtual bool GetFusedDataInternal(mojom::SensorType which_sensor_changed,
                                    SensorReading* fused_reading) = 0;

  // This raw pointer is safe because |fusion_sensor_| owns this object.
  raw_ptr<PlatformSensorFusion> fusion_sensor_ = nullptr;

 private:
  // Default threshold for comparing SensorReading values. If a
  // different threshold is better for a certain sensor type, set_threshold()
  // should be used to change it.
  double threshold_ = 0.1;

  mojom::SensorType fused_type_;
  base::flat_set<mojom::SensorType> source_types_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_FUSION_ALGORITHM_H_

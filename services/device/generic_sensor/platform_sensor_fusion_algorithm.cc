// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"

#include <cmath>
#include "base/containers/contains.h"

namespace device {

PlatformSensorFusionAlgorithm::PlatformSensorFusionAlgorithm(
    mojom::SensorType fused_type,
    const base::flat_set<mojom::SensorType>& source_types)
    : fused_type_(fused_type), source_types_(source_types) {
  DCHECK(!source_types_.empty());
}

PlatformSensorFusionAlgorithm::~PlatformSensorFusionAlgorithm() = default;

bool PlatformSensorFusionAlgorithm::GetFusedData(
    mojom::SensorType which_sensor_changed,
    SensorReading* fused_reading) {
  DCHECK(base::Contains(source_types_, which_sensor_changed));
  return GetFusedDataInternal(which_sensor_changed, fused_reading);
}

void PlatformSensorFusionAlgorithm::Reset() {}

void PlatformSensorFusionAlgorithm::SetFrequency(double) {}

}  // namespace device

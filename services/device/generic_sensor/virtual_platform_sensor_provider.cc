// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/virtual_platform_sensor_provider.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "services/device/generic_sensor/virtual_platform_sensor.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

VirtualPlatformSensorProvider::VirtualPlatformSensorProvider() = default;
VirtualPlatformSensorProvider::~VirtualPlatformSensorProvider() = default;

bool VirtualPlatformSensorProvider::AddSensorOverride(
    mojom::SensorType type,
    mojom::VirtualSensorMetadataPtr metadata) {
  return type_metadata_.try_emplace(type, std::move(metadata)).second;
}

void VirtualPlatformSensorProvider::RemoveSensorOverride(
    mojom::SensorType type) {
  if (type_metadata_.erase(type) == 0U) {
    return;
  }

  if (auto sensor = GetSensor(type)) {
    static_cast<VirtualPlatformSensor*>(sensor.get())->SimulateSensorRemoval();
  }
}

bool VirtualPlatformSensorProvider::IsOverridingSensor(
    mojom::SensorType type) const {
  return type_metadata_.contains(type);
}

void VirtualPlatformSensorProvider::CreateSensorInternal(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    CreateSensorCallback callback) {
  if (!IsOverridingSensor(type)) {
    NOTREACHED_NORETURN()
        << "CreateSensorInternal() was called but "
           "VirtualPlatformSensorProvider is not overriding sensor type "
        << type;
  }

  const auto& metadata = type_metadata_[type];

  if (!metadata->available) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto sensor =
      base::MakeRefCounted<VirtualPlatformSensor>(type, reading_buffer, this);
  if (!metadata->minimum_frequency.is_null()) {
    sensor->set_minimum_supported_frequency(metadata->minimum_frequency->value);
  }
  if (!metadata->maximum_frequency.is_null()) {
    sensor->set_maximum_supported_frequency(metadata->maximum_frequency->value);
  }
  if (!metadata->reporting_mode.is_null()) {
    sensor->set_reporting_mode(metadata->reporting_mode->value);
  }
  std::move(callback).Run(std::move(sensor));
}

}  // namespace device

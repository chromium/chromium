// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/virtual_platform_sensor_provider.h"

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/virtual_platform_sensor.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

struct VirtualPlatformSensorProvider::TypeMetadata {
  mojom::VirtualSensorMetadataPtr mojo_metadata;
  std::optional<SensorReading> pending_reading;
};

VirtualPlatformSensorProvider::VirtualPlatformSensorProvider() = default;
VirtualPlatformSensorProvider::~VirtualPlatformSensorProvider() = default;

base::WeakPtr<PlatformSensorProvider>
VirtualPlatformSensorProvider::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool VirtualPlatformSensorProvider::AddSensorOverride(
    mojom::SensorType type,
    mojom::VirtualSensorMetadataPtr metadata) {
  auto type_metadata = std::make_unique<TypeMetadata>();
  type_metadata->mojo_metadata = std::move(metadata);
  return type_metadata_.try_emplace(type, std::move(type_metadata)).second;
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
  // The *_EULER_ANGLES sensor types are special-cased. They support the legacy
  // Device Orientation API and the virtual sensors are fusion sensors instead
  // of VirtualPlatformSensor instances.
  if (type == mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES) {
    return IsOverridingSensor(
        mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION);
  } else if (type == mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES) {
    return IsOverridingSensor(
        mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION);
  }
  return type_metadata_.contains(type);
}

void VirtualPlatformSensorProvider::AddReading(mojom::SensorType type,
                                               const SensorReading& reading) {
  if (!IsOverridingSensor(type)) {
    NOTREACHED()
        << "AddReading() was called but "
           "VirtualPlatformSensorProvider is not overriding sensor type "
        << type;
  }

  if (auto virtual_sensor = GetSensor(type)) {
    static_cast<VirtualPlatformSensor*>(virtual_sensor.get())
        ->AddReading(reading);
  } else {
    type_metadata_[type]->pending_reading = reading;
  }
}

void VirtualPlatformSensorProvider::CreateSensorInternal(
    mojom::SensorType type,
    CreateSensorCallback callback) {
  if (!IsOverridingSensor(type)) {
    NOTREACHED()
        << "CreateSensorInternal() was called but "
           "VirtualPlatformSensorProvider is not overriding sensor type "
        << type;
  }

  // The *_EULER_ANGLES sensor types are special-cased. They support the legacy
  // Device Orientation API and the virtual sensors are fusion sensors instead
  // of VirtualPlatformSensor instances.
  if (type == mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES ||
      type == mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES) {
    const bool is_absolute =
        type == mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES;
    auto fusion_algorithm =
        std::make_unique<OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            /*absolute=*/is_absolute);
    // If this PlatformSensorFusion object is successfully initialized,
    // |callback| will be run with a reference to this object.
    PlatformSensorFusion::Create(AsWeakPtr(), std::move(fusion_algorithm),
                                 std::move(callback));
    return;
  }

  const auto& metadata = type_metadata_[type];

  if (!metadata->mojo_metadata->available) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto sensor = base::MakeRefCounted<VirtualPlatformSensor>(
      type, GetSensorReadingSharedBufferForType(type), AsWeakPtr(),
      metadata->pending_reading, *metadata->mojo_metadata);
  std::move(callback).Run(std::move(sensor));
}

}  // namespace device

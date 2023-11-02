// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_base.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

namespace {

const uint64_t kReadingBufferSize = sizeof(SensorReadingSharedBuffer);
const uint64_t kSharedBufferSizeInBytes =
    kReadingBufferSize *
    (static_cast<uint64_t>(mojom::SensorType::kMaxValue) + 1);

}  // namespace

PlatformSensorProviderBase::PlatformSensorProviderBase() = default;

PlatformSensorProviderBase::~PlatformSensorProviderBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PlatformSensorProviderBase::CreateSensor(mojom::SensorType type,
                                              CreateSensorCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!CreateSharedBufferIfNeeded()) {
    std::move(callback).Run(nullptr);
    return;
  }

  SensorReadingSharedBuffer* reading_buffer =
      GetSensorReadingSharedBufferForType(type);
  if (!reading_buffer) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto& requests = requests_map_[type];
  const bool callback_queue_was_empty = requests.empty();
  requests.push_back(std::move(callback));
  if (callback_queue_was_empty) {
    // This is the first CreateSensor call.
    CreateSensorInternal(
        type, reading_buffer,
        base::BindOnce(&PlatformSensorProviderBase::NotifySensorCreated,
                       base::Unretained(this), type));
  }
}

scoped_refptr<PlatformSensor> PlatformSensorProviderBase::GetSensor(
    mojom::SensorType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = sensor_map_.find(type);
  if (it != sensor_map_.end())
    return it->second;
  return nullptr;
}

bool PlatformSensorProviderBase::CreateSharedBufferIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (mapped_region_.IsValid())
    return true;

  mapped_region_ =
      base::ReadOnlySharedMemoryRegion::Create(kSharedBufferSizeInBytes);

  return mapped_region_.IsValid();
}

void PlatformSensorProviderBase::FreeResourcesIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sensor_map_.empty() && requests_map_.empty()) {
    FreeResources();
    mapped_region_ = {};
  }
}

void PlatformSensorProviderBase::RemoveSensor(mojom::SensorType type,
                                              PlatformSensor* sensor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = sensor_map_.find(type);
  if (it == sensor_map_.end()) {
    // It is possible on PlatformSensorFusion creation failure since the
    // PlatformSensorFusion object is not added to the |sensor_map_|, but
    // its base class destructor PlatformSensor::~PlatformSensor() calls this
    // RemoveSensor() function with the PlatformSensorFusion type.
    // It is also possible on PlatformSensorProviderChromeOS as late present
    // sensors makes the previous sensor calls this RemoveSensor() function
    // twice.
    return;
  }

  if (sensor != it->second) {
    // It is possible on PlatformSensorProviderChromeOS as late present sensors
    // may change the devices chosen on specific types.
    return;
  }

  sensor_map_.erase(type);
  FreeResourcesIfNeeded();
}

base::ReadOnlySharedMemoryRegion
PlatformSensorProviderBase::CloneSharedMemoryRegion() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CreateSharedBufferIfNeeded();
  return mapped_region_.region.Duplicate();
}

bool PlatformSensorProviderBase::HasSensors() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return !sensor_map_.empty();
}

void PlatformSensorProviderBase::NotifySensorCreated(
    mojom::SensorType type,
    scoped_refptr<PlatformSensor> sensor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!base::Contains(sensor_map_, type));
  DCHECK(base::Contains(requests_map_, type));

  if (sensor)
    sensor_map_[type] = sensor.get();

  auto it = requests_map_.find(type);
  CallbackQueue callback_queue = std::move(it->second);
  requests_map_.erase(it);

  FreeResourcesIfNeeded();

  // Inform subscribers about the sensor.
  // |sensor| can be nullptr here.
  for (auto& callback : callback_queue)
    std::move(callback).Run(sensor);
}

std::vector<mojom::SensorType>
PlatformSensorProviderBase::GetPendingRequestTypes() {
  std::vector<mojom::SensorType> request_types;
  for (auto const& entry : requests_map_)
    request_types.push_back(entry.first);
  return request_types;
}

SensorReadingSharedBuffer*
PlatformSensorProviderBase::GetSensorReadingSharedBufferForType(
    mojom::SensorType type) {
  auto* ptr = static_cast<char*>(mapped_region_.mapping.memory());
  if (!ptr)
    return nullptr;

  ptr += SensorReadingSharedBuffer::GetOffset(type);
  memset(ptr, 0, kReadingBufferSize);
  return reinterpret_cast<SensorReadingSharedBuffer*>(ptr);
}

}  // namespace device

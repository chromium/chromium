// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_base.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
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

void PlatformSensorProviderBase::CreateSensor(
    mojom::SensorType type,
    const CreateSensorCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!CreateSharedBufferIfNeeded()) {
    callback.Run(nullptr);
    return;
  }

  SensorReadingSharedBuffer* reading_buffer =
      GetSensorReadingSharedBufferForType(type);
  if (!reading_buffer) {
    callback.Run(nullptr);
    return;
  }

  auto it = requests_map_.find(type);
  if (it != requests_map_.end()) {
    it->second.push_back(callback);
  } else {  // This is the first CreateSensor call.
    requests_map_[type] = CallbackQueue({callback});

    CreateSensorInternal(
        type, reading_buffer,
        base::Bind(&PlatformSensorProviderBase::NotifySensorCreated,
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
  if (shared_buffer_mapping_.get())
    return true;

  if (!shared_buffer_handle_.is_valid()) {
    shared_buffer_handle_ =
        mojo::SharedBufferHandle::Create(kSharedBufferSizeInBytes);
    if (!shared_buffer_handle_.is_valid())
      return false;
  }

  // Create a writable mapping for the buffer as soon as possible, that will be
  // used by all platform sensor implementations that want to update it. Note
  // that on Android, cloning the shared memory handle readonly (as performed
  // by CloneSharedBufferHandle()) will seal the region read-only, preventing
  // future writable mappings to be created (but this one will survive).
  shared_buffer_mapping_ = shared_buffer_handle_->Map(kSharedBufferSizeInBytes);
  return shared_buffer_mapping_.get() != nullptr;
}

void PlatformSensorProviderBase::FreeResourcesIfNeeded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sensor_map_.empty() && requests_map_.empty()) {
    FreeResources();
    shared_buffer_mapping_.reset();
    shared_buffer_handle_.reset();
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
    return;
  }

  if (sensor != it->second) {
    NOTREACHED()
        << "not expecting to track more than one sensor of the same type";
    return;
  }

  sensor_map_.erase(type);
  FreeResourcesIfNeeded();
}

mojo::ScopedSharedBufferHandle
PlatformSensorProviderBase::CloneSharedBufferHandle() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CreateSharedBufferIfNeeded();
  return shared_buffer_handle_->Clone(
      mojo::SharedBufferHandle::AccessMode::READ_ONLY);
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
  CallbackQueue callback_queue = it->second;
  requests_map_.erase(type);

  FreeResourcesIfNeeded();

  // Inform subscribers about the sensor.
  // |sensor| can be nullptr here.
  for (auto& callback : callback_queue)
    callback.Run(sensor);
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
  auto* ptr = static_cast<char*>(shared_buffer_mapping_.get());
  if (!ptr)
    return nullptr;

  ptr += SensorReadingSharedBuffer::GetOffset(type);
  memset(ptr, 0, kReadingBufferSize);
  return reinterpret_cast<SensorReadingSharedBuffer*>(ptr);
}

}  // namespace device

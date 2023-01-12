// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_linux.h"

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/platform_sensor_linux.h"
#include "services/device/generic_sensor/platform_sensor_reader_linux.h"

namespace device {
namespace {

constexpr base::TaskTraits kBlockingTaskRunnerTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

}  // namespace

PlatformSensorProviderLinux::PlatformSensorProviderLinux()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          kBlockingTaskRunnerTraits)),
      sensor_device_manager_(nullptr,
                             base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  sensor_device_manager_.reset(
      new SensorDeviceManager(weak_ptr_factory_.GetWeakPtr()));
}

PlatformSensorProviderLinux::~PlatformSensorProviderLinux() = default;

void PlatformSensorProviderLinux::CreateSensorInternal(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    CreateSensorCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (enumeration_status_) {
    case SensorEnumerationState::kNotEnumerated: {
      // Unretained() is safe because the deletion of |sensor_device_manager_|
      // is scheduled on |blocking_task_runner_| when
      // PlatformSensorProviderLinux is deleted.
      const bool will_run = blocking_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SensorDeviceManager::Start,
                         base::Unretained(sensor_device_manager_.get())));
      if (will_run)
        enumeration_status_ = SensorEnumerationState::kEnumerationStarted;

      [[fallthrough]];
    }
    case SensorEnumerationState::kEnumerationStarted:
      return;
    case SensorEnumerationState::kEnumerationFinished:
      if (IsFusionSensorType(type)) {
        CreateFusionSensor(type, reading_buffer, std::move(callback));
        return;
      }

      SensorInfoLinux* sensor_device = GetSensorDevice(type);
      if (!sensor_device) {
        std::move(callback).Run(nullptr);
        return;
      }

      std::move(callback).Run(base::MakeRefCounted<PlatformSensorLinux>(
          type, reading_buffer, this, sensor_device));

      break;
  }
}

void PlatformSensorProviderLinux::FreeResources() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool PlatformSensorProviderLinux::IsSensorTypeAvailable(
    mojom::SensorType type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetSensorDevice(type);
}

SensorInfoLinux* PlatformSensorProviderLinux::GetSensorDevice(
    mojom::SensorType type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const auto sensor = sensor_devices_by_type_.find(type);
  if (sensor == sensor_devices_by_type_.end())
    return nullptr;
  return sensor->second.get();
}

void PlatformSensorProviderLinux::SetSensorDeviceManagerForTesting(
    std::unique_ptr<SensorDeviceManager> sensor_device_manager) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sensor_device_manager_.reset(sensor_device_manager.release());
}

void PlatformSensorProviderLinux::ProcessStoredRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<mojom::SensorType> request_types = GetPendingRequestTypes();
  if (request_types.empty())
    return;

  for (auto const& type : request_types) {
    if (IsFusionSensorType(type)) {
      SensorReadingSharedBuffer* reading_buffer =
          GetSensorReadingSharedBufferForType(type);
      CreateFusionSensor(
          type, reading_buffer,
          base::BindOnce(&PlatformSensorProviderLinux::NotifySensorCreated,
                         base::Unretained(this), type));
      continue;
    }

    SensorInfoLinux* device = nullptr;
    auto device_entry = sensor_devices_by_type_.find(type);
    if (device_entry != sensor_devices_by_type_.end())
      device = device_entry->second.get();
    CreateSensorAndNotify(type, device);
  }
}

void PlatformSensorProviderLinux::CreateSensorAndNotify(
    mojom::SensorType type,
    SensorInfoLinux* sensor_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<PlatformSensorLinux> sensor;
  SensorReadingSharedBuffer* reading_buffer =
      GetSensorReadingSharedBufferForType(type);
  if (sensor_device && reading_buffer) {
    sensor = new PlatformSensorLinux(type, reading_buffer, this, sensor_device);
  }
  NotifySensorCreated(type, sensor);
}

void PlatformSensorProviderLinux::OnSensorNodesEnumerated() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(enumeration_status_, SensorEnumerationState::kEnumerationStarted);
  enumeration_status_ = SensorEnumerationState::kEnumerationFinished;
  ProcessStoredRequests();
}

void PlatformSensorProviderLinux::OnDeviceAdded(
    mojom::SensorType type,
    std::unique_ptr<SensorInfoLinux> sensor_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // At the moment, we support only one device per type.
  if (base::Contains(sensor_devices_by_type_, type)) {
    DVLOG(1) << "Sensor ignored. Type " << type
             << ". Node: " << sensor_device->device_node;
    return;
  }
  sensor_devices_by_type_[type] = std::move(sensor_device);
}

void PlatformSensorProviderLinux::OnDeviceRemoved(
    mojom::SensorType type,
    const std::string& device_node) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = sensor_devices_by_type_.find(type);
  if (it != sensor_devices_by_type_.end() &&
      it->second->device_node == device_node) {
    sensor_devices_by_type_.erase(it);
  }
}

}  // namespace device

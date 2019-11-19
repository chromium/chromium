// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_linux.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "services/device/generic_sensor/absolute_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_magnetometer.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/orientation_quaternion_fusion_algorithm_using_euler_angles.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/platform_sensor_linux.h"
#include "services/device/generic_sensor/platform_sensor_reader_linux.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/relative_orientation_euler_angles_fusion_algorithm_using_accelerometer_and_gyroscope.h"

namespace device {
namespace {

constexpr base::TaskTraits kBlockingTaskRunnerTraits = {
    base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

bool IsFusionSensorType(mojom::SensorType type) {
  switch (type) {
    case mojom::SensorType::LINEAR_ACCELERATION:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return true;
    default:
      return false;
  }
}
}  // namespace

PlatformSensorProviderLinux::PlatformSensorProviderLinux()
    : sensor_nodes_enumerated_(false),
      sensor_nodes_enumeration_started_(false),
      blocking_task_runner_(
          base::CreateSequencedTaskRunner(kBlockingTaskRunnerTraits)),
      sensor_device_manager_(nullptr,
                             base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  sensor_device_manager_.reset(
      new SensorDeviceManager(weak_ptr_factory_.GetWeakPtr()));
}

PlatformSensorProviderLinux::~PlatformSensorProviderLinux() = default;

void PlatformSensorProviderLinux::CreateSensorInternal(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const CreateSensorCallback& callback) {
  if (!sensor_nodes_enumerated_) {
    if (!sensor_nodes_enumeration_started_) {
      // Unretained() is safe because the deletion of |sensor_device_manager_|
      // is scheduled on |blocking_task_runner_| when
      // PlatformSensorProviderLinux is deleted.
      sensor_nodes_enumeration_started_ = blocking_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SensorDeviceManager::Start,
                         base::Unretained(sensor_device_manager_.get())));
    }
    return;
  }

  if (IsFusionSensorType(type)) {
    CreateFusionSensor(type, reading_buffer, callback);
    return;
  }

  SensorInfoLinux* sensor_device = GetSensorDevice(type);
  if (!sensor_device) {
    callback.Run(nullptr);
    return;
  }

  SensorDeviceFound(type, reading_buffer, callback, sensor_device);
}

void PlatformSensorProviderLinux::SensorDeviceFound(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const PlatformSensorProviderBase::CreateSensorCallback& callback,
    const SensorInfoLinux* sensor_device) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sensor_device);

  scoped_refptr<PlatformSensorLinux> sensor =
      new PlatformSensorLinux(type, reading_buffer, this, sensor_device);
  callback.Run(sensor);
}

void PlatformSensorProviderLinux::FreeResources() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SensorInfoLinux* PlatformSensorProviderLinux::GetSensorDevice(
    mojom::SensorType type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto sensor = sensor_devices_by_type_.find(type);
  if (sensor == sensor_devices_by_type_.end())
    return nullptr;
  return sensor->second.get();
}

void PlatformSensorProviderLinux::GetAllSensorDevices() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(maksims): implement this method once we have discovery API.
  NOTIMPLEMENTED();
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
          base::Bind(&PlatformSensorProviderLinux::NotifySensorCreated,
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
  DCHECK(!sensor_nodes_enumerated_);
  sensor_nodes_enumerated_ = true;
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

void PlatformSensorProviderLinux::CreateFusionSensor(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const CreateSensorCallback& callback) {
  DCHECK(IsFusionSensorType(type));
  std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm;
  switch (type) {
    case mojom::SensorType::LINEAR_ACCELERATION:
      fusion_algorithm = std::make_unique<
          LinearAccelerationFusionAlgorithmUsingAccelerometer>();
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      fusion_algorithm = std::make_unique<
          AbsoluteOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndMagnetometer>();
      break;
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      fusion_algorithm = std::make_unique<
          OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
          true /* absolute */);
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
      if (GetSensorDevice(mojom::SensorType::GYROSCOPE)) {
        fusion_algorithm = std::make_unique<
            RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometerAndGyroscope>();
      } else {
        fusion_algorithm = std::make_unique<
            RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer>();
      }
      break;
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
      fusion_algorithm = std::make_unique<
          OrientationQuaternionFusionAlgorithmUsingEulerAngles>(
          false /* absolute */);
      break;
    default:
      NOTREACHED();
  }

  DCHECK(fusion_algorithm);
  PlatformSensorFusion::Create(reading_buffer, this,
                               std::move(fusion_algorithm), callback);
}

}  // namespace device

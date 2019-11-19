// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/linux/sensor_device_manager.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

namespace {

std::string StringOrEmptyIfNull(const char* value) {
  return value ? value : std::string();
}

}  // namespace

SensorDeviceManager::SensorDeviceManager(base::WeakPtr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      delegate_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SensorDeviceManager::~SensorDeviceManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SensorDeviceManager::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!udev_watcher_);

  udev_watcher_ = UdevWatcher::StartWatching(this);
  if (!udev_watcher_)
    return;

  // OnDeviceAdded() will be called synchronously for every device found in the
  // enumeration.
  udev_watcher_->EnumerateExistingDevices();

  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SensorDeviceManager::Delegate::OnSensorNodesEnumerated,
                     delegate_));
}

std::string SensorDeviceManager::GetUdevDeviceGetSubsystem(udev_device* dev) {
  return StringOrEmptyIfNull(udev_device_get_subsystem(dev));
}

std::string SensorDeviceManager::GetUdevDeviceGetSyspath(udev_device* dev) {
  return StringOrEmptyIfNull(udev_device_get_syspath(dev));
}

std::string SensorDeviceManager::GetUdevDeviceGetSysattrValue(
    udev_device* dev,
    const std::string& attribute) {
  return StringOrEmptyIfNull(
      udev_device_get_sysattr_value(dev, attribute.c_str()));
}

std::string SensorDeviceManager::GetUdevDeviceGetDevnode(udev_device* dev) {
  return StringOrEmptyIfNull(udev_device_get_devnode(dev));
}

void SensorDeviceManager::OnDeviceAdded(ScopedUdevDevicePtr dev) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string subsystem = GetUdevDeviceGetSubsystem(dev.get());
  if (subsystem.empty() || subsystem.compare("iio") != 0)
    return;

  const std::string sysfs_path = GetUdevDeviceGetSyspath(dev.get());
  if (sysfs_path.empty())
    return;

  const std::string device_node = GetUdevDeviceGetDevnode(dev.get());
  if (device_node.empty())
    return;

  const uint32_t first = static_cast<uint32_t>(mojom::SensorType::kMinValue);
  const uint32_t last = static_cast<uint32_t>(mojom::SensorType::kMaxValue);
  for (uint32_t i = first; i <= last; ++i) {
    SensorPathsLinux data;
    mojom::SensorType type = static_cast<mojom::SensorType>(i);
    if (!InitSensorData(type, &data))
      continue;

    std::vector<base::FilePath> sensor_file_names;
    for (const std::vector<std::string>& names : data.sensor_file_names) {
      for (const auto& name : names) {
        const std::string value =
            GetUdevDeviceGetSysattrValue(dev.get(), name.c_str());
        if (value.empty())
          continue;
        base::FilePath full_path = base::FilePath(sysfs_path).Append(name);
        sensor_file_names.push_back(full_path);
        break;
      }
    }

    if (sensor_file_names.empty() ||
        sensor_file_names.size() > SensorReadingRaw::kValuesCount) {
      continue;
    }

    const std::string scaling_value =
        GetUdevDeviceGetSysattrValue(dev.get(), data.sensor_scale_name.c_str());
    // If scaling value is not found, treat it as 1.
    double sensor_scaling_value = 1;
    if (!scaling_value.empty())
      base::StringToDouble(scaling_value, &sensor_scaling_value);

    const std::string offset_vallue = GetUdevDeviceGetSysattrValue(
        dev.get(), data.sensor_offset_file_name.c_str());
    // If offset value is not found, treat it as 0.
    double sensor_offset_value = 0;
    if (!offset_vallue.empty())
      base::StringToDouble(offset_vallue, &sensor_offset_value);

    const std::string frequency_value = GetUdevDeviceGetSysattrValue(
        dev.get(), data.sensor_frequency_file_name.c_str());
    // If frequency is not found, use default one from SensorPathsLinux struct.
    double sensor_frequency_value = data.default_configuration.frequency();
    // By default, |reporting_mode| is ON_CHANGE. But if platform provides
    // sampling frequency, the reporting mode is CONTINUOUS.
    mojom::ReportingMode reporting_mode = mojom::ReportingMode::ON_CHANGE;
    if (!frequency_value.empty()) {
      base::StringToDouble(frequency_value, &sensor_frequency_value);
      reporting_mode = mojom::ReportingMode::CONTINUOUS;
    }

    // Update own cache of known sensor devices.
    if (!base::Contains(sensors_by_node_, device_node))
      sensors_by_node_[device_node] = data.type;

    std::unique_ptr<SensorInfoLinux> device(new SensorInfoLinux(
        device_node, sensor_frequency_value, sensor_scaling_value,
        sensor_offset_value, reporting_mode, data.apply_scaling_func,
        std::move(sensor_file_names)));
    delegate_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SensorDeviceManager::Delegate::OnDeviceAdded,
                                  delegate_, data.type, std::move(device)));

    // One |dev| can represent more than one sensor.
    // For example, there is an accelerometer and gyroscope represented by one
    // |dev| in Chrome OS, kernel < 3.18. Thus, iterate through all possible
    // types of sensors.
  }
}

void SensorDeviceManager::OnDeviceRemoved(ScopedUdevDevicePtr dev) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string subsystem = GetUdevDeviceGetSubsystem(dev.get());
  if (subsystem.empty() || subsystem.compare("iio") != 0)
    return;

  const std::string device_node = GetUdevDeviceGetDevnode(dev.get());
  if (device_node.empty())
    return;

  auto sensor = sensors_by_node_.find(device_node);
  if (sensor == sensors_by_node_.end())
    return;
  mojom::SensorType type = sensor->second;
  sensors_by_node_.erase(sensor);

  delegate_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SensorDeviceManager::Delegate::OnDeviceRemoved,
                                delegate_, type, device_node));
}

void SensorDeviceManager::OnDeviceChanged(ScopedUdevDevicePtr) {}

}  // namespace device

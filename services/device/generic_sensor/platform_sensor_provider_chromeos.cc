// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_chromeos.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/components/sensors/sensor_util.h"
#include "services/device/generic_sensor/platform_sensor_chromeos.h"

namespace device {
namespace {

constexpr base::TimeDelta kReconnectDelay =
    base::TimeDelta::FromMilliseconds(1000);

base::Optional<mojom::SensorType> ConvertSensorType(
    chromeos::sensors::mojom::DeviceType device_type) {
  switch (device_type) {
    case chromeos::sensors::mojom::DeviceType::ACCEL:
      return mojom::SensorType::ACCELEROMETER;

    case chromeos::sensors::mojom::DeviceType::ANGLVEL:
      return mojom::SensorType::GYROSCOPE;

    case chromeos::sensors::mojom::DeviceType::LIGHT:
      return mojom::SensorType::AMBIENT_LIGHT;

    case chromeos::sensors::mojom::DeviceType::MAGN:
      return mojom::SensorType::MAGNETOMETER;

    default:
      return base::nullopt;
  }
}

bool DeviceNeedsLocationWithTypes(const std::vector<mojom::SensorType>& types) {
  for (auto type : types) {
    switch (type) {
      case mojom::SensorType::AMBIENT_LIGHT:
      case mojom::SensorType::ACCELEROMETER:
      case mojom::SensorType::GYROSCOPE:
      case mojom::SensorType::MAGNETOMETER:
        return true;
      default:
        break;
    }
  }

  return false;
}

}  // namespace

PlatformSensorProviderChromeOS::PlatformSensorProviderChromeOS() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RegisterSensorClient();
}

PlatformSensorProviderChromeOS::~PlatformSensorProviderChromeOS() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PlatformSensorProviderChromeOS::SetUpChannel(
    mojo::PendingRemote<chromeos::sensors::mojom::SensorService>
        pending_remote) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sensor_service_remote_.is_bound()) {
    LOG(ERROR) << "Ignoring the second Remote<SensorService>";
    return;
  }

  sensor_service_remote_.Bind(std::move(pending_remote));
  sensor_service_remote_.set_disconnect_handler(
      base::BindOnce(&PlatformSensorProviderChromeOS::OnSensorServiceDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));

  sensor_service_remote_->GetAllDeviceIds(
      base::BindOnce(&PlatformSensorProviderChromeOS::GetAllDeviceIdsCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PlatformSensorProviderChromeOS::CreateSensorInternal(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    CreateSensorCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!sensor_service_remote_.is_bound() || !AreAllSensorsReady()) {
    // Wait until |sensor_service_remote_| is connected and all sensors are
    // ready to proceed.
    // If |sensor_service_remote_| is disconnected, wait until it re-connects to
    // proceed, as it needs a valid SensorDevice channel.
    return;
  }

  // As mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES needs to know
  // whether mojom::SensorType::GYROSCOPE exists or not to determine the
  // algorithm, wait until all sensors ready before processing the fusion
  // sensors as well.
  if (IsFusionSensorType(type)) {
    CreateFusionSensor(type, reading_buffer, std::move(callback));
    return;
  }

  auto id_opt = GetDeviceId(type);
  if (!id_opt.has_value()) {
    std::move(callback).Run(nullptr);
    return;
  }
  int32_t id = id_opt.value();
  DCHECK(base::Contains(sensors_, id));

  auto& sensor = sensors_[id];
  DCHECK(sensor.scale.has_value());

  auto sensor_device_remote = GetSensorDeviceRemote(id);
  std::move(callback).Run(base::MakeRefCounted<PlatformSensorChromeOS>(
      id, type, reading_buffer, this, sensor.scale.value(),
      std::move(sensor_device_remote)));
}

void PlatformSensorProviderChromeOS::FreeResources() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool PlatformSensorProviderChromeOS::IsSensorTypeAvailable(
    mojom::SensorType type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GetDeviceId(type).has_value();
}

PlatformSensorProviderChromeOS::SensorData::SensorData() = default;
PlatformSensorProviderChromeOS::SensorData::~SensorData() = default;

base::Optional<PlatformSensorProviderChromeOS::SensorLocation>
PlatformSensorProviderChromeOS::ParseLocation(
    const base::Optional<std::string>& raw_location) {
  if (!raw_location.has_value()) {
    LOG(ERROR) << "No location attribute";
    return base::nullopt;
  }

  // These locations must be listed in the same order as the SensorLocation
  // enum.
  const std::vector<std::string> location_strings = {
      chromeos::sensors::mojom::kLocationBase,
      chromeos::sensors::mojom::kLocationLid,
      chromeos::sensors::mojom::kLocationCamera};
  const auto it = base::ranges::find(location_strings, raw_location.value());
  if (it == std::end(location_strings))
    return base::nullopt;

  return static_cast<SensorLocation>(
      std::distance(std::begin(location_strings), it));
}

base::Optional<int32_t> PlatformSensorProviderChromeOS::GetDeviceId(
    mojom::SensorType type) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const auto type_id = sensor_id_by_type_.find(type);
  if (type_id == sensor_id_by_type_.end())
    return base::nullopt;
  return type_id->second;
}

void PlatformSensorProviderChromeOS::RegisterSensorClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!sensor_hal_client_.is_bound());

  if (!chromeos::sensors::BindSensorHalClient(
          sensor_hal_client_.BindNewPipeAndPassRemote())) {
    LOG(ERROR) << "Failed to bind SensorHalClient via Crosapi";
    return;
  }

  sensor_hal_client_.set_disconnect_handler(
      base::BindOnce(&PlatformSensorProviderChromeOS::OnSensorHalClientFailure,
                     weak_ptr_factory_.GetWeakPtr(), kReconnectDelay));
}

void PlatformSensorProviderChromeOS::OnSensorHalClientFailure(
    base::TimeDelta reconnection_delay) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(ERROR) << "OnSensorHalClientFailure";

  ResetSensorService();
  sensor_hal_client_.reset();

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PlatformSensorProviderChromeOS::RegisterSensorClient,
                     weak_ptr_factory_.GetWeakPtr()),
      reconnection_delay);
}

void PlatformSensorProviderChromeOS::OnSensorServiceDisconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(ERROR) << "OnSensorServiceDisconnect";

  ResetSensorService();
}

void PlatformSensorProviderChromeOS::ResetSensorService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto& sensor : sensors_) {
    // Reset only the remote while keeping the attributes information to speed
    // up recovery, as the attributes won't need to be queried again.
    sensor.second.remote.reset();
  }

  sensor_service_remote_.reset();
}

void PlatformSensorProviderChromeOS::GetAllDeviceIdsCallback(
    const SensorIdTypesMap& ids_types) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sensor_service_remote_.is_bound());

  sensor_ids_received_ = true;

  for (const auto& id_types : ids_types) {
    int32_t id = id_types.first;
    SensorData& sensor = sensors_[id];

    if (sensor.ignored)
      continue;

    for (const auto& device_type : id_types.second) {
      auto type_opt = ConvertSensorType(device_type);
      if (!type_opt.has_value())
        continue;

      sensor.types.push_back(type_opt.value());
    }

    if (sensor.types.empty()) {
      sensor.ignored = true;
      continue;
    }

    sensor.remote.reset();

    std::vector<std::string> attr_names;
    if (!sensor.scale.has_value())
      attr_names.push_back(chromeos::sensors::mojom::kScale);
    if (DeviceNeedsLocationWithTypes(sensor.types) &&
        !sensor.location.has_value()) {
      attr_names.push_back(chromeos::sensors::mojom::kLocation);
    }

    if (attr_names.empty())
      continue;

    sensor.remote = GetSensorDeviceRemote(id);

    // Add a temporary disconnect handler to catch failures during sensor
    // enumeration. PlatformSensorChromeOS will handle disconnection during
    // normal operation.
    sensor.remote.set_disconnect_handler(base::BindOnce(
        &PlatformSensorProviderChromeOS::OnSensorDeviceDisconnect,
        weak_ptr_factory_.GetWeakPtr(), id));

    sensor.remote->GetAttributes(
        std::move(attr_names),
        base::BindOnce(&PlatformSensorProviderChromeOS::GetAttributesCallback,
                       weak_ptr_factory_.GetWeakPtr(), id));
  }

  ProcessSensorsIfPossible();
}

void PlatformSensorProviderChromeOS::GetAttributesCallback(
    int32_t id,
    const std::vector<base::Optional<std::string>>& values) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = sensors_.find(id);
  DCHECK(it != sensors_.end());
  auto& sensor = it->second;
  DCHECK(sensor.remote.is_bound());

  size_t index = 0;

  if (!sensor.scale.has_value()) {
    if (index >= values.size()) {
      LOG(ERROR) << "values doesn't contain scale attribute.";
      IgnoreSensor(sensor);
      return;
    }

    double scale = 0.0;
    if (!values[index].has_value() ||
        !base::StringToDouble(values[index].value(), &scale)) {
      LOG(ERROR) << "Invalid scale: " << values[index].value_or("")
                 << ", for accel with id: " << id;
      IgnoreSensor(sensor);
      return;
    }

    sensor.scale = scale;

    ++index;
  }

  if (DeviceNeedsLocationWithTypes(sensor.types) &&
      !sensor.location.has_value()) {
    if (index >= values.size()) {
      LOG(ERROR) << "values doesn't contain location attribute.";
      IgnoreSensor(sensor);
      return;
    }

    sensor.location = ParseLocation(values[index]);
    if (!sensor.location.has_value()) {
      LOG(ERROR) << "Failed to parse location: " << values[index].value_or("")
                 << ", with sensor id: " << id;
      IgnoreSensor(sensor);
      return;
    }

    ++index;
  }

  ProcessSensorsIfPossible();
}

void PlatformSensorProviderChromeOS::IgnoreSensor(SensorData& sensor) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  sensor.ignored = true;
  sensor.remote.reset();

  ProcessSensorsIfPossible();
}

bool PlatformSensorProviderChromeOS::AreAllSensorsReady() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!sensor_ids_received_)
    return false;

  return base::ranges::all_of(sensors_, [](const auto& sensor) {
    return sensor.second.ignored ||
           (sensor.second.scale.has_value() &&
            (!DeviceNeedsLocationWithTypes(sensor.second.types) ||
             sensor.second.location.has_value()));
  });
}

void PlatformSensorProviderChromeOS::OnSensorDeviceDisconnect(int32_t id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  LOG(ERROR) << "OnSensorDeviceDisconnect: " << id;

  // Assumes IIO Service has crashed and waits for its relaunch.
  ResetSensorService();
}

void PlatformSensorProviderChromeOS::ProcessSensorsIfPossible() {
  if (!AreAllSensorsReady())
    return;

  DetermineMotionSensors();
  DetermineLightSensor();

  RemoveUnusedSensorDeviceRemotes();
  ProcessStoredRequests();
}

// Follow Android's and W3C's requirements of motion sensors:
// Android: https://source.android.com/devices/sensors/sensor-types
// W3C: https://w3c.github.io/sensors/#local-coordinate-system
// To implement fusion/composite sensors, accelerometer, gyroscope (and
// magnetometer) must be in the same plane, so when it comes to choosing an
// accelerometer in a convertible device, we choose the one which is in the
// same place as the gyroscope. If we still have a choice, we use the one in
// the lid.
void PlatformSensorProviderChromeOS::DetermineMotionSensors() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  struct MotionSensorInfo {
    size_t count;
    std::vector<std::pair<int32_t, mojom::SensorType>> sensor_info_pairs;
  };
  std::map<SensorLocation, MotionSensorInfo> motion_sensor_location_info;

  for (const auto& sensor : sensors_) {
    if (sensor.second.ignored || !sensor.second.location)
      continue;

    SensorLocation location = sensor.second.location.value();
    DCHECK_LT(location, SensorLocation::kMax);

    for (auto type : sensor.second.types) {
      switch (type) {
        case mojom::SensorType::ACCELEROMETER:
        case mojom::SensorType::GYROSCOPE:
        case mojom::SensorType::MAGNETOMETER: {
          auto& motion_sensor_info = motion_sensor_location_info[location];
          motion_sensor_info.count++;
          motion_sensor_info.sensor_info_pairs.push_back(
              std::make_pair(sensor.first, type));
          break;
        }

        default:
          break;
      }
    }
  }

  const auto preferred_location =
      motion_sensor_location_info[SensorLocation::kLid].count >=
              motion_sensor_location_info[SensorLocation::kBase].count
          ? SensorLocation::kLid
          : SensorLocation::kBase;
  const auto& sensor_info_pairs =
      motion_sensor_location_info[preferred_location].sensor_info_pairs;
  for (const auto& pair : sensor_info_pairs)
    sensor_id_by_type_[pair.second] = pair.first;
}

// Prefer the light sensor on the lid, as it's more meaningful to web API users.
void PlatformSensorProviderChromeOS::DetermineLightSensor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::Optional<int32_t> id = base::nullopt;

  for (const auto& sensor : sensors_) {
    if (sensor.second.ignored ||
        !base::Contains(sensor.second.types, mojom::SensorType::AMBIENT_LIGHT))
      continue;

    if (!id.has_value() || sensor.second.location == SensorLocation::kLid)
      id = sensor.first;
  }

  if (id.has_value())
    sensor_id_by_type_[mojom::SensorType::AMBIENT_LIGHT] = id.value();
}

void PlatformSensorProviderChromeOS::RemoveUnusedSensorDeviceRemotes() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::set<int32_t> used_ids;
  for (const auto& type_id : sensor_id_by_type_)
    used_ids.emplace(type_id.second);

  for (auto& sensor : sensors_) {
    if (!base::Contains(used_ids, sensor.first))
      sensor.second.remote.reset();
  }
}

void PlatformSensorProviderChromeOS::ProcessStoredRequests() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::vector<mojom::SensorType> request_types = GetPendingRequestTypes();
  for (const auto& type : request_types) {
    SensorReadingSharedBuffer* reading_buffer =
        GetSensorReadingSharedBufferForType(type);

    if (!reading_buffer)
      continue;

    if (IsFusionSensorType(type)) {
      CreateFusionSensor(
          type, reading_buffer,
          base::BindOnce(&PlatformSensorProviderChromeOS::NotifySensorCreated,
                         weak_ptr_factory_.GetWeakPtr(), type));
      continue;
    }

    auto id_opt = GetDeviceId(type);
    if (!id_opt.has_value()) {
      NotifySensorCreated(type, nullptr);
      continue;
    }

    int32_t id = id_opt.value();
    DCHECK(base::Contains(sensors_, id));

    auto& sensor = sensors_[id];
    DCHECK(sensor.scale.has_value());

    auto sensor_device_remote = GetSensorDeviceRemote(id);
    NotifySensorCreated(
        type, base::MakeRefCounted<PlatformSensorChromeOS>(
                  id, type, reading_buffer, this, sensor.scale.value(),
                  std::move(sensor_device_remote)));
  }
}

mojo::Remote<chromeos::sensors::mojom::SensorDevice>
PlatformSensorProviderChromeOS::GetSensorDeviceRemote(int32_t id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sensor_service_remote_.is_bound());

  mojo::Remote<chromeos::sensors::mojom::SensorDevice> sensor_device_remote;
  auto& sensor = sensors_[id];
  if (sensor.remote.is_bound()) {
    // Reuse the previous remote.
    sensor_device_remote = std::move(sensor.remote);
  } else {
    sensor_service_remote_->GetDevice(
        id, sensor_device_remote.BindNewPipeAndPassReceiver());
  }

  return sensor_device_remote;
}

}  // namespace device

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor.h"

#include <utility>

#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/platform_sensor_util.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"

namespace device {

PlatformSensor::PlatformSensor(mojom::SensorType type,
                               SensorReadingSharedBuffer* reading_buffer,
                               PlatformSensorProvider* provider)
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      reading_buffer_(reading_buffer),
      type_(type),
      provider_(provider) {
  VLOG(1) << "Platform sensor created. Type " << type_ << ".";
}

PlatformSensor::~PlatformSensor() {
  if (provider_)
    provider_->RemoveSensor(GetType(), this);
  VLOG(1) << "Platform sensor released. Type " << type_ << ".";
}

mojom::SensorType PlatformSensor::GetType() const {
  return type_;
}

double PlatformSensor::GetMaximumSupportedFrequency() {
  return GetDefaultConfiguration().frequency();
}

double PlatformSensor::GetMinimumSupportedFrequency() {
  return 1.0 / (60 * 60);
}

void PlatformSensor::SensorReplaced() {
  ResetReadingBuffer();
}

bool PlatformSensor::StartListening(Client* client,
                                    const PlatformSensorConfiguration& config) {
  DCHECK(clients_.HasObserver(client));
  if (!CheckSensorConfiguration(config))
    return false;

  auto& config_list = config_map_[client];
  config_list.push_back(config);

  if (!UpdateSensorInternal(config_map_)) {
    config_list.pop_back();
    return false;
  }

  return true;
}

bool PlatformSensor::StopListening(Client* client,
                                   const PlatformSensorConfiguration& config) {
  DCHECK(clients_.HasObserver(client));
  auto client_entry = config_map_.find(client);
  if (client_entry == config_map_.end())
    return false;

  auto& config_list = client_entry->second;
  if (base::Erase(config_list, config) == 0)
    return false;

  return UpdateSensorInternal(config_map_);
}

bool PlatformSensor::StopListening(Client* client) {
  DCHECK(client);
  if (config_map_.erase(client) == 0)
    return false;
  return UpdateSensorInternal(config_map_);
}

void PlatformSensor::UpdateSensor() {
  UpdateSensorInternal(config_map_);
}

void PlatformSensor::AddClient(Client* client) {
  DCHECK(client);
  clients_.AddObserver(client);
}

void PlatformSensor::RemoveClient(Client* client) {
  DCHECK(client);
  clients_.RemoveObserver(client);
  StopListening(client);
}

bool PlatformSensor::GetLatestReading(SensorReading* result) {
  if (!reading_buffer_)
    return false;

  return SensorReadingSharedBufferReader::GetReading(reading_buffer_, result);
}

bool PlatformSensor::GetLatestRawReading(SensorReading* result) const {
  base::AutoLock auto_lock(lock_);
  if (!last_raw_reading_.has_value())
    return false;
  *result = last_raw_reading_.value();
  return true;
}

void PlatformSensor::UpdateSharedBufferAndNotifyClients(
    const SensorReading& reading) {
  if (UpdateSharedBuffer(reading, /*do_significance_check=*/true)) {
    main_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&PlatformSensor::NotifySensorReadingChanged,
                                  weak_factory_.GetWeakPtr()));
  }
}

bool PlatformSensor::UpdateSharedBuffer(const SensorReading& reading,
                                        bool do_significance_check) {
  if (!reading_buffer_)
    return false;

  {
    base::AutoLock auto_lock(lock_);
    // Bail out early if the new reading does not differ significantly from
    // our current one, when the sensor is not reporting data continuously.
    // Empty readings (i.e. with a zero timestamp) are always processed.
    if (GetReportingMode() == mojom::ReportingMode::ON_CHANGE &&
        do_significance_check && last_raw_reading_.has_value() &&
        !IsSignificantlyDifferent(*last_raw_reading_, reading, type_)) {
      return false;
    }
    // Save the raw (non-rounded) reading for fusion sensors.
    last_raw_reading_ = reading;
  }

  // Round the reading to guard user privacy. See https://crbug.com/1018180.
  SensorReading rounded_reading = reading;
  RoundSensorReading(&rounded_reading, type_);

  {
    base::AutoLock auto_lock(lock_);
    // Report new values only if rounded value is different compared to
    // previous value.
    if (GetReportingMode() == mojom::ReportingMode::ON_CHANGE &&
        do_significance_check && last_rounded_reading_.has_value() &&
        base::ranges::equal(rounded_reading.raw.values,
                            last_rounded_reading_->raw.values)) {
      return false;
    }
    // Save rounded value for next comparison.
    last_rounded_reading_ = rounded_reading;
  }
  reading_buffer_->seqlock.value().WriteBegin();
  device::OneWriterSeqLock::AtomicWriterMemcpy(
      &reading_buffer_->reading, &rounded_reading, sizeof(SensorReading));
  reading_buffer_->seqlock.value().WriteEnd();
  return true;
}

void PlatformSensor::NotifySensorReadingChanged() {
  for (auto& client : clients_) {
    if (!client.IsSuspended())
      client.OnSensorReadingChanged(type_);
  }
}

void PlatformSensor::NotifySensorError() {
  for (auto& client : clients_)
    client.OnSensorError();
}

void PlatformSensor::ResetReadingBuffer() {
  reading_buffer_ = nullptr;
}

bool PlatformSensor::UpdateSensorInternal(const ConfigMap& configurations) {
  const PlatformSensorConfiguration* optimal_configuration = nullptr;
  for (const auto& pair : configurations) {
    if (pair.first->IsSuspended())
      continue;

    const auto& conf_list = pair.second;
    for (const auto& configuration : conf_list) {
      if (!optimal_configuration || configuration > *optimal_configuration)
        optimal_configuration = &configuration;
    }
  }

  if (!optimal_configuration) {
    is_active_ = false;
    StopSensor();
    // If we reached this condition, we want to set the current reading to zero
    // regardless of the previous reading's value per
    // https://w3c.github.io/sensors/#set-sensor-settings. That is the reason
    // to skip significance check.
    UpdateSharedBuffer(SensorReading(), /*do_significance_check=*/false);
    return true;
  }

  is_active_ = StartSensor(*optimal_configuration);
  return is_active_;
}

bool PlatformSensor::IsActiveForTesting() const {
  return is_active_;
}

auto PlatformSensor::GetConfigMapForTesting() const -> const ConfigMap& {
  return config_map_;
}

void PlatformSensor::PostTaskToMainSequence(const base::Location& location,
                                            base::OnceClosure task) {
  main_task_runner()->PostTask(location, std::move(task));
}

bool PlatformSensor::IsSignificantlyDifferent(const SensorReading& lhs,
                                              const SensorReading& rhs,
                                              mojom::SensorType sensor_type) {
  switch (sensor_type) {
    case mojom::SensorType::AMBIENT_LIGHT:
      return std::fabs(lhs.als.value - rhs.als.value) >=
             kAlsSignificanceThreshold;

    case mojom::SensorType::ACCELEROMETER:
    case mojom::SensorType::GRAVITY:
    case mojom::SensorType::LINEAR_ACCELERATION:
    case mojom::SensorType::GYROSCOPE:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
    case mojom::SensorType::MAGNETOMETER:
    case mojom::SensorType::PRESSURE:
    case mojom::SensorType::PROXIMITY:
      return !base::ranges::equal(lhs.raw.values, rhs.raw.values);
  }
  NOTREACHED();
  return false;
}

}  // namespace device

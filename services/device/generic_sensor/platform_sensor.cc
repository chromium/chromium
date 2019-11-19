// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"

namespace device {

PlatformSensor::PlatformSensor(mojom::SensorType type,
                               SensorReadingSharedBuffer* reading_buffer,
                               PlatformSensorProvider* provider)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      reading_buffer_(reading_buffer),
      type_(type),
      provider_(provider) {}

PlatformSensor::~PlatformSensor() {
  if (provider_)
    provider_->RemoveSensor(GetType(), this);
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
  auto config_entry = std::find(config_list.begin(), config_list.end(), config);
  if (config_entry == config_list.end())
    return false;

  config_list.erase(config_entry);

  return UpdateSensorInternal(config_map_);
}

bool PlatformSensor::StopListening(Client* client) {
  DCHECK(client);
  auto client_entry = config_map_.find(client);
  if (client_entry == config_map_.end())
    return false;

  config_map_.erase(client_entry);

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
  return SensorReadingSharedBufferReader::GetReading(reading_buffer_, result);
}

void PlatformSensor::UpdateSharedBufferAndNotifyClients(
    const SensorReading& reading) {
  UpdateSharedBuffer(reading);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlatformSensor::NotifySensorReadingChanged,
                                weak_factory_.GetWeakPtr()));
}

void PlatformSensor::UpdateSharedBuffer(const SensorReading& reading) {
  ReadingBuffer* buffer = reading_buffer_;
  auto& seqlock = buffer->seqlock.value();
  seqlock.WriteBegin();
  buffer->reading = reading;
  seqlock.WriteEnd();
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
    UpdateSharedBuffer(SensorReading());
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

}  // namespace device

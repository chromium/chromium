// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_linux.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/platform_sensor_reader_linux.h"

namespace device {

PlatformSensorLinux::PlatformSensorLinux(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    base::WeakPtr<PlatformSensorProvider> provider,
    const SensorInfoLinux* sensor_device)
    : PlatformSensor(type, reading_buffer, std::move(provider)),
      default_configuration_(
          PlatformSensorConfiguration(sensor_device->device_frequency)),
      reporting_mode_(sensor_device->reporting_mode) {
  sensor_reader_ =
      SensorReader::Create(*sensor_device, weak_factory_.GetWeakPtr());
}

PlatformSensorLinux::~PlatformSensorLinux() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
}

mojom::ReportingMode PlatformSensorLinux::GetReportingMode() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  return reporting_mode_;
}

void PlatformSensorLinux::UpdatePlatformSensorReading(SensorReading reading) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  UpdateSharedBufferAndNotifyClients(reading);
}

void PlatformSensorLinux::NotifyPlatformSensorError() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  NotifySensorError();
}

bool PlatformSensorLinux::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  sensor_reader_->StartFetchingData(configuration);
  return true;
}

void PlatformSensorLinux::StopSensor() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  sensor_reader_->StopFetchingData();
}

bool PlatformSensorLinux::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  return configuration.frequency() > 0 &&
         configuration.frequency() <= default_configuration_.frequency();
}

PlatformSensorConfiguration PlatformSensorLinux::GetDefaultConfiguration() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  return default_configuration_;
}

}  // namespace device

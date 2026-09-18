// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_win.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

namespace device {

PlatformSensorWin::PlatformSensorWin(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    base::WeakPtr<PlatformSensorProvider> provider,
    ScopedPlatformSensorReaderWinBase sensor_reader)
    : PlatformSensor(type, reading_buffer, std::move(provider)),
      sensor_reader_(std::move(sensor_reader)) {
  DCHECK(sensor_reader_);
  sensor_reader_->SetClient(this);
}

PlatformSensorConfiguration PlatformSensorWin::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(GetSensorDefaultFrequency(GetType()));
}

mojom::ReportingMode PlatformSensorWin::GetReportingMode() {
  // All Windows sensors, even with high accuracy / sensitivity will not report
  // reading updates continuously. Therefore, return ON_CHANGE by default.
  return mojom::ReportingMode::ON_CHANGE;
}

double PlatformSensorWin::GetMaximumSupportedFrequency() {
  base::TimeDelta minimal_reporting_interval =
      sensor_reader_->GetMinimalReportingInterval();
  if (minimal_reporting_interval.is_zero()) {
    return GetSensorDefaultFrequency(GetType());
  }
  return minimal_reporting_interval.ToHz();
}

void PlatformSensorWin::OnReadingUpdated(const SensorReading& reading) {
  // This function is called from the reader's sequence, which for
  // PlatformSensorReaderWin32 is its COM STA sequence.
  UpdateSharedBufferAndNotifyClients(reading);
}

void PlatformSensorWin::OnSensorError() {
  PostTaskToMainSequence(FROM_HERE,
                         base::BindOnce(&PlatformSensorWin::NotifySensorError,
                                        weak_factory_.GetWeakPtr()));
}

bool PlatformSensorWin::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  return sensor_reader_->StartSensor(configuration);
}

void PlatformSensorWin::StopSensor() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  sensor_reader_->StopSensor();
}

bool PlatformSensorWin::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  const double frequency = configuration.frequency();
  if (!IsValidSensorFrequency(frequency)) {
    return false;
  }
  base::TimeDelta minimal_reporting_interval =
      sensor_reader_->GetMinimalReportingInterval();
  if (minimal_reporting_interval.is_zero()) {
    return true;
  }
  return frequency <= minimal_reporting_interval.ToHz();
}

PlatformSensorWin::~PlatformSensorWin() {
  DCHECK(main_task_runner()->RunsTasksInCurrentSequence());
  sensor_reader_->SetClient(nullptr);
}

}  // namespace device

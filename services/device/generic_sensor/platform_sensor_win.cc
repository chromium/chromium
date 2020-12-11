// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_win.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"

namespace device {

namespace {
constexpr double kDefaultSensorReportingFrequency = 5.0;
}  // namespace

PlatformSensorWin::PlatformSensorWin(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider,
    scoped_refptr<base::SingleThreadTaskRunner> sensor_thread_runner,
    std::unique_ptr<PlatformSensorReaderWinBase> sensor_reader)
    : PlatformSensor(type, reading_buffer, provider),
      sensor_thread_runner_(sensor_thread_runner),
      sensor_reader_(sensor_reader.release()) {
  DCHECK(sensor_reader_);
  sensor_reader_->SetClient(this);
}

PlatformSensorConfiguration PlatformSensorWin::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(kDefaultSensorReportingFrequency);
}

mojom::ReportingMode PlatformSensorWin::GetReportingMode() {
  // All Windows sensors, even with high accuracy / sensitivity will not report
  // reading updates continuously. Therefore, return ON_CHANGE by default.
  return mojom::ReportingMode::ON_CHANGE;
}

double PlatformSensorWin::GetMaximumSupportedFrequency() {
  base::TimeDelta minimal_reporting_interval_ms =
      sensor_reader_->GetMinimalReportingInterval();
  if (minimal_reporting_interval_ms.is_zero())
    return kDefaultSensorReportingFrequency;
  return 1.0 / minimal_reporting_interval_ms.InSecondsF();
}

void PlatformSensorWin::OnReadingUpdated(const SensorReading& reading) {
  UpdateSharedBufferAndNotifyClients(reading);
}

void PlatformSensorWin::OnSensorError() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&PlatformSensorWin::NotifySensorError,
                                        weak_factory_.GetWeakPtr()));
}

bool PlatformSensorWin::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return sensor_reader_->StartSensor(configuration);
}

void PlatformSensorWin::StopSensor() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  sensor_reader_->StopSensor();
}

bool PlatformSensorWin::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::TimeDelta minimal_reporting_interval_ms =
      sensor_reader_->GetMinimalReportingInterval();
  if (minimal_reporting_interval_ms.is_zero())
    return true;
  double max_frequency = 1.0 / minimal_reporting_interval_ms.InSecondsF();
  return configuration.frequency() <= max_frequency;
}

PlatformSensorWin::~PlatformSensorWin() {
  sensor_reader_->SetClient(nullptr);
  sensor_thread_runner_->DeleteSoon(FROM_HERE, sensor_reader_);
}

}  // namespace device

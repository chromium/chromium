// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_linux.h"

#include "base/files/file_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/platform_sensor_linux.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

class PollingSensorReader : public SensorReader {
 public:
  PollingSensorReader(const SensorInfoLinux* sensor_device,
                      base::WeakPtr<PlatformSensorLinux> sensor,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~PollingSensorReader() override;

  // SensorReader implements:
  void StartFetchingData(
      const PlatformSensorConfiguration& configuration) override;
  void StopFetchingData() override;

 private:
  // Initializes a read timer.
  void InitializeTimer(const PlatformSensorConfiguration& configuration);

  // Polls data and sends it to a |sensor_|.
  void PollForData();

  // Paths to sensor read files.
  const std::vector<base::FilePath> sensor_file_paths_;

  // Scaling value that are applied to raw data from sensors.
  const double scaling_value_;

  // Offset value.
  const double offset_value_;

  // Used to apply scalings and invert signs if needed.
  const SensorPathsLinux::ReaderFunctor apply_scaling_func_;

  // Repeating timer for data polling.
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(PollingSensorReader);
};

PollingSensorReader::PollingSensorReader(
    const SensorInfoLinux* sensor_device,
    base::WeakPtr<PlatformSensorLinux> sensor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : SensorReader(sensor, std::move(task_runner)),
      sensor_file_paths_(sensor_device->device_reading_files),
      scaling_value_(sensor_device->device_scaling_value),
      offset_value_(sensor_device->device_offset_value),
      apply_scaling_func_(sensor_device->apply_scaling_func) {}

PollingSensorReader::~PollingSensorReader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void PollingSensorReader::StartFetchingData(
    const PlatformSensorConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_reading_active_)
    StopFetchingData();
  InitializeTimer(configuration);
}

void PollingSensorReader::StopFetchingData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_reading_active_ = false;
  timer_.Stop();
}

void PollingSensorReader::InitializeTimer(
    const PlatformSensorConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_reading_active_);
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                        configuration.frequency()),
      this, &PollingSensorReader::PollForData);
  is_reading_active_ = true;
}

void PollingSensorReader::PollForData() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SensorReading readings;
  DCHECK_LE(sensor_file_paths_.size(), arraysize(readings.raw.values));
  int i = 0;
  for (const auto& path : sensor_file_paths_) {
    std::string new_read_value;
    if (!base::ReadFileToString(path, &new_read_value)) {
      NotifyReadError();
      StopFetchingData();
      return;
    }

    double new_value = 0;
    base::TrimWhitespaceASCII(new_read_value, base::TRIM_ALL, &new_read_value);
    if (!base::StringToDouble(new_read_value, &new_value)) {
      NotifyReadError();
      StopFetchingData();
      return;
    }
    readings.raw.values[i++] = new_value;
  }
  if (!apply_scaling_func_.is_null())
    apply_scaling_func_.Run(scaling_value_, offset_value_, readings);

  if (is_reading_active_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&PlatformSensorLinux::UpdatePlatformSensorReading,
                              sensor_, readings));
  }
}

// static
std::unique_ptr<SensorReader> SensorReader::Create(
    const SensorInfoLinux* sensor_device,
    base::WeakPtr<PlatformSensorLinux> sensor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  base::AssertBlockingAllowedDeprecated();
  // TODO(maksims): implement triggered reading. At the moment,
  // only polling read is supported.
  return std::make_unique<PollingSensorReader>(sensor_device, sensor,
                                               std::move(task_runner));
}

SensorReader::SensorReader(
    base::WeakPtr<PlatformSensorLinux> sensor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : sensor_(sensor),
      task_runner_(std::move(task_runner)),
      is_reading_active_(false) {
  DETACH_FROM_THREAD(thread_checker_);
}

SensorReader::~SensorReader() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SensorReader::NotifyReadError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_reading_active_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&PlatformSensorLinux::NotifyPlatformSensorError, sensor_));
  }
}

}  // namespace device

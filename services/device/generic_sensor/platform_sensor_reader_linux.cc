// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_linux.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/timer/timer.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/platform_sensor_linux.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

class PollingSensorReader : public SensorReader {
 public:
  PollingSensorReader(const SensorInfoLinux& sensor_info,
                      base::WeakPtr<PlatformSensorLinux> sensor,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~PollingSensorReader() override;

  // SensorReader overrides
  void StartFetchingData(
      const PlatformSensorConfiguration& configuration) override;
  void StopFetchingData() override;

 private:
  // Helper class that performs the actual I/O. It must run on a
  // SequencedTaskRunner that is properly configured for blocking I/O
  // operations.
  class BlockingTaskRunnerHelper final {
   public:
    BlockingTaskRunnerHelper(
        base::WeakPtr<PollingSensorReader> polling_sensor_reader,
        const SensorInfoLinux& sensor_info);
    ~BlockingTaskRunnerHelper();

    // Starts polling for data at a given |frequency|, and stops if there is an
    // error while reading or parsing the data.
    void StartPolling(double frequency);

    // Stops polling for data and notifying PollingSensorReader.
    void StopPolling();

   private:
    void StopWithError();

    void PollForData();

    // Repeating timer for data polling.
    base::RepeatingTimer timer_;

    // |polling_sensor_reader_| can only be checked for validity on
    // |owner_task_runner_|'s sequence.
    base::WeakPtr<PollingSensorReader> polling_sensor_reader_;

    // Task runner belonging to PollingSensorReader. Calls to
    // PollingSensorReader will be done via this task runner.
    scoped_refptr<base::SequencedTaskRunner> owner_task_runner_;

    // Sensor information we need in order to know where and how to read sensor
    // data.
    const SensorInfoLinux sensor_info_;

    SEQUENCE_CHECKER(sequence_checker_);

    DISALLOW_COPY_AND_ASSIGN(BlockingTaskRunnerHelper);
  };

  // Receives a reading from the platform sensor and forwards it to |sensor_|.
  void OnReadingAvailable(SensorReading reading);

  // Signals that an error occurred while trying to read from a platform sensor.
  void OnReadingError();

  // Initializes a read timer.
  void InitializeTimer(const PlatformSensorConfiguration& configuration);

  // In builds with DCHECK enabled, checks that methods of this class are
  // called on the right thread.
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_ =
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  std::unique_ptr<BlockingTaskRunnerHelper, base::OnTaskRunnerDeleter>
      blocking_task_helper_;

  base::WeakPtrFactory<PollingSensorReader> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PollingSensorReader);
};

PollingSensorReader::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::WeakPtr<PollingSensorReader> polling_sensor_reader,
    const SensorInfoLinux& sensor_info)
    : polling_sensor_reader_(polling_sensor_reader),
      owner_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      sensor_info_(sensor_info) {
  // Detaches from the sequence on which this object was created. It will be
  // bound to its owning sequence when StartPolling() is called.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PollingSensorReader::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PollingSensorReader::BlockingTaskRunnerHelper::StartPolling(
    double frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!timer_.IsRunning());
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMicroseconds(
                   base::Time::kMicrosecondsPerSecond / frequency),
               this,
               &PollingSensorReader::BlockingTaskRunnerHelper::PollForData);
}

void PollingSensorReader::BlockingTaskRunnerHelper::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
}

void PollingSensorReader::BlockingTaskRunnerHelper::StopWithError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopPolling();
  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PollingSensorReader::OnReadingError,
                                polling_sensor_reader_));
}

void PollingSensorReader::BlockingTaskRunnerHelper::PollForData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SensorReading readings;
  DCHECK_LE(sensor_info_.device_reading_files.size(),
            base::size(readings.raw.values));
  int i = 0;
  for (const auto& path : sensor_info_.device_reading_files) {
    std::string new_read_value;
    if (!base::ReadFileToString(path, &new_read_value)) {
      StopWithError();
      return;
    }

    double new_value = 0;
    base::TrimWhitespaceASCII(new_read_value, base::TRIM_ALL, &new_read_value);
    if (!base::StringToDouble(new_read_value, &new_value)) {
      StopWithError();
      return;
    }
    readings.raw.values[i++] = new_value;
  }

  const auto& scaling_function = sensor_info_.apply_scaling_func;
  if (!scaling_function.is_null()) {
    scaling_function.Run(sensor_info_.device_scaling_value,
                         sensor_info_.device_offset_value, readings);
  }

  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PollingSensorReader::OnReadingAvailable,
                                polling_sensor_reader_, readings));
}

PollingSensorReader::PollingSensorReader(
    const SensorInfoLinux& sensor_info,
    base::WeakPtr<PlatformSensorLinux> sensor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : SensorReader(sensor, std::move(task_runner)),
      blocking_task_helper_(nullptr,
                            base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  // We need to properly initialize |blocking_task_helper_| here because we need
  // |weak_factory_| to be created first.
  blocking_task_helper_.reset(
      new BlockingTaskRunnerHelper(weak_factory_.GetWeakPtr(), sensor_info));
}

PollingSensorReader::~PollingSensorReader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PollingSensorReader::StartFetchingData(
    const PlatformSensorConfiguration& configuration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_reading_active_)
    StopFetchingData();
  is_reading_active_ = true;
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::StartPolling,
                                base::Unretained(blocking_task_helper_.get()),
                                configuration.frequency()));
}

void PollingSensorReader::StopFetchingData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_reading_active_ = false;
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::StopPolling,
                                base::Unretained(blocking_task_helper_.get())));
}

void PollingSensorReader::OnReadingAvailable(SensorReading reading) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_reading_active_)
    return;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformSensorLinux::UpdatePlatformSensorReading, sensor_,
                     reading));
}

void PollingSensorReader::OnReadingError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyReadError();
}

// static
std::unique_ptr<SensorReader> SensorReader::Create(
    const SensorInfoLinux& sensor_info,
    base::WeakPtr<PlatformSensorLinux> sensor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // TODO(maksims): implement triggered reading. At the moment,
  // only polling read is supported.
  return std::make_unique<PollingSensorReader>(sensor_info, sensor,
                                               std::move(task_runner));
}

SensorReader::SensorReader(
    base::WeakPtr<PlatformSensorLinux> sensor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : sensor_(sensor),
      task_runner_(std::move(task_runner)),
      is_reading_active_(false) {
}

SensorReader::~SensorReader() = default;

void SensorReader::NotifyReadError() {
  if (is_reading_active_) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PlatformSensorLinux::NotifyPlatformSensorError,
                       sensor_));
  }
}

}  // namespace device

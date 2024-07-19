// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/generic_sensor/platform_sensor_reader_linux.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "services/device/generic_sensor/linux/sensor_data_linux.h"
#include "services/device/generic_sensor/platform_sensor_linux.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

class PollingSensorReader : public SensorReader {
 public:
  PollingSensorReader(const SensorInfoLinux& sensor_info,
                      base::WeakPtr<PlatformSensorLinux> sensor);

  PollingSensorReader(const PollingSensorReader&) = delete;
  PollingSensorReader& operator=(const PollingSensorReader&) = delete;

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
        scoped_refptr<base::SequencedTaskRunner> task_runner,
        const SensorInfoLinux& sensor_info);

    BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
    BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) =
        delete;

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
  };

  // Receives a reading from the platform sensor and forwards it to |sensor_|.
  void OnReadingAvailable(SensorReading reading);

  // Signals that an error occurred while trying to read from a platform sensor.
  void OnReadingError();

  // In builds with DCHECK enabled, checks that methods of this class are
  // called on the right thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<BlockingTaskRunnerHelper> blocking_task_helper_;

  base::WeakPtrFactory<PollingSensorReader> weak_factory_{this};
};

PollingSensorReader::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::WeakPtr<PollingSensorReader> polling_sensor_reader,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const SensorInfoLinux& sensor_info)
    : polling_sensor_reader_(polling_sensor_reader),
      owner_task_runner_(std::move(task_runner)),
      sensor_info_(sensor_info) {}

PollingSensorReader::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PollingSensorReader::BlockingTaskRunnerHelper::StartPolling(
    double frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!timer_.IsRunning());
  timer_.Start(
      FROM_HERE,
      base::Microseconds(base::Time::kMicrosecondsPerSecond / frequency), this,
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
            std::size(readings.raw.values));
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
    base::WeakPtr<PlatformSensorLinux> sensor)
    : SensorReader(sensor) {
  blocking_task_helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault(), sensor_info);
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
  blocking_task_helper_.AsyncCall(&BlockingTaskRunnerHelper::StartPolling)
      .WithArgs(configuration.frequency());
}

void PollingSensorReader::StopFetchingData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_reading_active_ = false;
  blocking_task_helper_.AsyncCall(&BlockingTaskRunnerHelper::StopPolling);
}

void PollingSensorReader::OnReadingAvailable(SensorReading reading) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_reading_active_)
    return;
  if (sensor_) {
    sensor_->PostTaskToMainSequence(
        FROM_HERE,
        base::BindOnce(&PlatformSensorLinux::UpdatePlatformSensorReading,
                       sensor_, reading));
  }
}

void PollingSensorReader::OnReadingError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyReadError();
}

// static
std::unique_ptr<SensorReader> SensorReader::Create(
    const SensorInfoLinux& sensor_info,
    base::WeakPtr<PlatformSensorLinux> sensor) {
  // TODO(maksims): implement triggered reading. At the moment,
  // only polling read is supported.
  return std::make_unique<PollingSensorReader>(sensor_info, sensor);
}

SensorReader::SensorReader(base::WeakPtr<PlatformSensorLinux> sensor)
    : sensor_(sensor), is_reading_active_(false) {}

SensorReader::~SensorReader() = default;

void SensorReader::NotifyReadError() {
  if (is_reading_active_ && sensor_) {
    sensor_->PostTaskToMainSequence(
        FROM_HERE,
        base::BindOnce(&PlatformSensorLinux::NotifyPlatformSensorError,
                       sensor_));
  }
}

}  // namespace device

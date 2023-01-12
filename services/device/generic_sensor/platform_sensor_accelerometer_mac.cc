// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_accelerometer_mac.h"

#include <stdint.h>

#include <cmath>

#include "base/functional/bind.h"
#include "base/numerics/math_constants.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "services/device/generic_sensor/platform_sensor_provider_mac.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"
#include "third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h"

namespace {

constexpr double kGravityThreshold = base::kMeanGravityDouble * 0.01;

bool IsSignificantlyDifferentMac(const device::SensorReading& reading1,
                                 const device::SensorReading& reading2) {
  return (std::fabs(reading1.accel.x - reading2.accel.x) >=
          kGravityThreshold) ||
         (std::fabs(reading1.accel.y - reading2.accel.y) >=
          kGravityThreshold) ||
         (std::fabs(reading1.accel.z - reading2.accel.z) >= kGravityThreshold);
}

}  // namespace

namespace device {

using mojom::SensorType;

// Helper class that performs the actual I/O. It must run on a
// SequencedTaskRunner that is properly configured for blocking I/O
// operations.
class PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper final {
 public:
  explicit BlockingTaskRunnerHelper(
      base::WeakPtr<PlatformSensorAccelerometerMac> platform_sensor);
  ~BlockingTaskRunnerHelper();

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  void Init();
  void StartPolling(double frequency);
  void StopPolling();

 private:
  void PollForData();

  // Repeating timer for data polling.
  base::RepeatingTimer timer_;

  // |platform_sensor_| can only be checked for validity on
  // |owner_task_runner_|'s sequence.
  base::WeakPtr<PlatformSensorAccelerometerMac> platform_sensor_;

  // Task runner belonging to PlatformSensorAccelerometerMac. Calls to it
  // will be done via this task runner.
  scoped_refptr<base::SequencedTaskRunner> owner_task_runner_;

  // SuddenMotionSensor instance that performs blocking calls.
  std::unique_ptr<SuddenMotionSensor> sudden_motion_sensor_;

  // In builds with DCHECK enabled, checks that methods of this class are
  // called on the right thread.
  SEQUENCE_CHECKER(sequence_checker_);
};

PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::
    BlockingTaskRunnerHelper(
        base::WeakPtr<PlatformSensorAccelerometerMac> platform_sensor)
    : platform_sensor_(platform_sensor),
      owner_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // Detaches from the sequence on which this object was created. It will be
  // bound to another sequence when Init() is called.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::
    ~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sudden_motion_sensor_)
    return;

  sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
}

void PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::StartPolling(
    double frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!timer_.IsRunning());

  if (!sudden_motion_sensor_)
    return;

  timer_.Start(
      FROM_HERE,
      base::Microseconds(base::Time::kMicrosecondsPerSecond / frequency), this,
      &PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::PollForData);
}

void PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::StopPolling() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Stop();
}

void PlatformSensorAccelerometerMac::BlockingTaskRunnerHelper::PollForData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sudden_motion_sensor_);

  // Retrieve per-axis calibrated values.
  float axis_value[3];
  if (!sudden_motion_sensor_->ReadSensorValues(axis_value))
    return;

  SensorReading reading;
  reading.accel.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
  reading.accel.x = axis_value[0] * base::kMeanGravityDouble;
  reading.accel.y = axis_value[1] * base::kMeanGravityDouble;
  reading.accel.z = axis_value[2] * base::kMeanGravityDouble;

  owner_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformSensorAccelerometerMac::OnReadingAvailable,
                     platform_sensor_, reading));
}

PlatformSensorAccelerometerMac::PlatformSensorAccelerometerMac(
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider)
    : PlatformSensor(SensorType::ACCELEROMETER, reading_buffer, provider),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      blocking_task_helper_(nullptr,
                            base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  // We need to properly initialize |blocking_task_helper_| here because we need
  // |weak_factory_| to be created first.
  blocking_task_helper_.reset(
      new BlockingTaskRunnerHelper(weak_factory_.GetWeakPtr()));
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::Init,
                                base::Unretained(blocking_task_helper_.get())));
}

PlatformSensorAccelerometerMac::~PlatformSensorAccelerometerMac() = default;

mojom::ReportingMode PlatformSensorAccelerometerMac::GetReportingMode() {
  return mojom::ReportingMode::ON_CHANGE;
}

bool PlatformSensorAccelerometerMac::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  return configuration.frequency() > 0 &&
         configuration.frequency() <=
             SensorTraits<SensorType::ACCELEROMETER>::kMaxAllowedFrequency;
}

PlatformSensorConfiguration
PlatformSensorAccelerometerMac::GetDefaultConfiguration() {
  return PlatformSensorConfiguration(
      SensorTraits<SensorType::ACCELEROMETER>::kDefaultFrequency);
}

bool PlatformSensorAccelerometerMac::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  if (is_reading_active_)
    StopSensor();
  is_reading_active_ = true;
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::StartPolling,
                                base::Unretained(blocking_task_helper_.get()),
                                configuration.frequency()));
  return true;
}

void PlatformSensorAccelerometerMac::StopSensor() {
  is_reading_active_ = false;
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&BlockingTaskRunnerHelper::StopPolling,
                                base::Unretained(blocking_task_helper_.get())));
}

void PlatformSensorAccelerometerMac::OnReadingAvailable(SensorReading reading) {
  if (is_reading_active_ && IsSignificantlyDifferentMac(reading_, reading)) {
    reading_ = reading;
    UpdateSharedBufferAndNotifyClients(reading);
  }
}

}  // namespace device

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_win.h"

#include <comdef.h>
#include <objbase.h>

#include <iomanip>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/platform_sensor_win.h"

namespace device {

PlatformSensorProviderWin::PlatformSensorProviderWin()
    : com_sta_task_runner_(base::CreateCOMSTATaskRunner(
          {base::ThreadPool(), base::TaskPriority::USER_VISIBLE})) {}

PlatformSensorProviderWin::~PlatformSensorProviderWin() = default;

void PlatformSensorProviderWin::SetSensorManagerForTesting(
    Microsoft::WRL::ComPtr<ISensorManager> sensor_manager) {
  sensor_manager_ = sensor_manager;
}

scoped_refptr<base::SingleThreadTaskRunner>
PlatformSensorProviderWin::GetComStaTaskRunnerForTesting() {
  return com_sta_task_runner_;
}

void PlatformSensorProviderWin::CreateSensorInternal(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const CreateSensorCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (sensor_manager_) {
    OnInitSensorManager(type, reading_buffer, callback);
  } else {
    com_sta_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&PlatformSensorProviderWin::InitSensorManager,
                   base::Unretained(this)),
        base::Bind(&PlatformSensorProviderWin::OnInitSensorManager,
                   base::Unretained(this), type, reading_buffer, callback));
  }
}

void PlatformSensorProviderWin::InitSensorManager() {
  DCHECK(com_sta_task_runner_->RunsTasksInCurrentSequence());

  HRESULT hr = ::CoCreateInstance(CLSID_SensorManager, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&sensor_manager_));
  if (FAILED(hr)) {
    // Only log this error the first time.
    static bool logged_failure = false;
    if (!logged_failure) {
      LOG(ERROR) << "Unable to create instance of SensorManager: "
                 << _com_error(hr).ErrorMessage() << " (0x" << std::hex
                 << std::uppercase << std::setfill('0') << std::setw(8) << hr
                 << ")";
      logged_failure = true;
    }
  }
}

void PlatformSensorProviderWin::OnInitSensorManager(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const CreateSensorCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!sensor_manager_) {
    callback.Run(nullptr);
    return;
  }

  switch (type) {
    // Fusion sensor.
    case mojom::SensorType::LINEAR_ACCELERATION: {
      auto linear_acceleration_fusion_algorithm = std::make_unique<
          LinearAccelerationFusionAlgorithmUsingAccelerometer>();
      // If this PlatformSensorFusion object is successfully initialized,
      // |callback| will be run with a reference to this object.
      PlatformSensorFusion::Create(
          reading_buffer, this, std::move(linear_acceleration_fusion_algorithm),
          callback);
      break;
    }

    // Try to create low-level sensors by default.
    default: {
      base::PostTaskAndReplyWithResult(
          com_sta_task_runner_.get(), FROM_HERE,
          base::Bind(&PlatformSensorProviderWin::CreateSensorReader,
                     base::Unretained(this), type),
          base::Bind(&PlatformSensorProviderWin::SensorReaderCreated,
                     base::Unretained(this), type, reading_buffer, callback));
      break;
    }
  }
}

void PlatformSensorProviderWin::SensorReaderCreated(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    const CreateSensorCallback& callback,
    std::unique_ptr<PlatformSensorReaderWinBase> sensor_reader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!sensor_reader) {
    // Fallback options for sensors that can be implemented using sensor
    // fusion. Note that it is important not to generate a cycle by adding a
    // fallback here that depends on one of the other fallbacks provided.
    switch (type) {
      case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES: {
        auto algorithm = std::make_unique<
            OrientationEulerAnglesFusionAlgorithmUsingQuaternion>(
            true /* absolute */);
        PlatformSensorFusion::Create(reading_buffer, this, std::move(algorithm),
                                     std::move(callback));
        return;
      }
      default:
        callback.Run(nullptr);
        return;
    }
  }

  scoped_refptr<PlatformSensor> sensor =
      new PlatformSensorWin(type, reading_buffer, this, com_sta_task_runner_,
                            std::move(sensor_reader));
  callback.Run(sensor);
}

std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorProviderWin::CreateSensorReader(mojom::SensorType type) {
  DCHECK(com_sta_task_runner_->RunsTasksInCurrentSequence());
  if (!sensor_manager_)
    return nullptr;
  return PlatformSensorReaderWin32::Create(type, sensor_manager_);
}

}  // namespace device

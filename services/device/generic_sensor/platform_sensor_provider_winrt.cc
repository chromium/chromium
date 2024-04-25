// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_provider_winrt.h"

#include <comdef.h>

#include "base/task/thread_pool.h"
#include "services/device/generic_sensor/gravity_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/linear_acceleration_fusion_algorithm_using_accelerometer.h"
#include "services/device/generic_sensor/orientation_euler_angles_fusion_algorithm_using_quaternion.h"
#include "services/device/generic_sensor/platform_sensor_fusion.h"
#include "services/device/generic_sensor/platform_sensor_reader_winrt.h"
#include "services/device/generic_sensor/platform_sensor_win.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"

namespace device {

std::unique_ptr<PlatformSensorReaderWinBase>
SensorReaderFactory::CreateSensorReader(mojom::SensorType type) {
  return PlatformSensorReaderWinrtFactory::Create(type);
}

PlatformSensorProviderWinrt::PlatformSensorProviderWinrt()
    : com_sta_task_runner_(base::ThreadPool::CreateCOMSTATaskRunner(
          {base::TaskPriority::USER_VISIBLE})),
      sensor_reader_factory_(std::make_unique<SensorReaderFactory>()) {}

PlatformSensorProviderWinrt::~PlatformSensorProviderWinrt() = default;

base::WeakPtr<PlatformSensorProvider> PlatformSensorProviderWinrt::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PlatformSensorProviderWinrt::SetSensorReaderFactoryForTesting(
    std::unique_ptr<SensorReaderFactory> sensor_reader_factory) {
  sensor_reader_factory_ = std::move(sensor_reader_factory);
}

void PlatformSensorProviderWinrt::CreateSensorInternal(
    mojom::SensorType type,
    CreateSensorCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (type) {
    // Fusion sensor.
    case mojom::SensorType::LINEAR_ACCELERATION: {
      auto linear_acceleration_fusion_algorithm = std::make_unique<
          LinearAccelerationFusionAlgorithmUsingAccelerometer>();
      // If this PlatformSensorFusion object is successfully initialized,
      // |callback| will be run with a reference to this object.
      PlatformSensorFusion::Create(
          AsWeakPtr(), std::move(linear_acceleration_fusion_algorithm),
          std::move(callback));
      break;
    }
    case mojom::SensorType::GRAVITY: {
      auto gravity_fusion_algorithm =
          std::make_unique<GravityFusionAlgorithmUsingAccelerometer>();
      // If this PlatformSensorFusion object is successfully initialized,
      // |callback| will be run with a reference to this object.
      PlatformSensorFusion::Create(AsWeakPtr(),
                                   std::move(gravity_fusion_algorithm),
                                   std::move(callback));
      break;
    }

    // Try to create low-level sensors by default.
    default: {
      com_sta_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&PlatformSensorProviderWinrt::CreateSensorReader,
                         base::Unretained(this), type),
          base::BindOnce(&PlatformSensorProviderWinrt::SensorReaderCreated,
                         base::Unretained(this), type, std::move(callback)));
      break;
    }
  }
}

void PlatformSensorProviderWinrt::SensorReaderCreated(
    mojom::SensorType type,
    CreateSensorCallback callback,
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
            /*absolute=*/true);
        PlatformSensorFusion::Create(AsWeakPtr(), std::move(algorithm),
                                     std::move(callback));
        return;
      }
      default:
        std::move(callback).Run(nullptr);
        return;
    }
  }
  scoped_refptr<PlatformSensor> sensor =
      base::MakeRefCounted<PlatformSensorWin>(
          type, GetSensorReadingSharedBufferForType(type), AsWeakPtr(),
          com_sta_task_runner_, std::move(sensor_reader));
  std::move(callback).Run(std::move(sensor));
}

std::unique_ptr<PlatformSensorReaderWinBase>
PlatformSensorProviderWinrt::CreateSensorReader(mojom::SensorType type) {
  DCHECK(com_sta_task_runner_->RunsTasksInCurrentSequence());
  return sensor_reader_factory_->CreateSensorReader(type);
}

}  // namespace device

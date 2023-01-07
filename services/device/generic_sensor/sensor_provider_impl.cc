// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/sensor_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/sensor_impl.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/sensor_traits.h"

namespace device {

namespace {

bool IsExtraSensorClass(mojom::SensorType type) {
  switch (type) {
    case mojom::SensorType::ACCELEROMETER:
    case mojom::SensorType::LINEAR_ACCELERATION:
    case mojom::SensorType::GRAVITY:
    case mojom::SensorType::GYROSCOPE:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
    case mojom::SensorType::RELATIVE_ORIENTATION_EULER_ANGLES:
    case mojom::SensorType::RELATIVE_ORIENTATION_QUATERNION:
      return false;
    default:
      return true;
  }
}

}  // namespace

SensorProviderImpl::SensorProviderImpl(
    std::unique_ptr<PlatformSensorProvider> provider)
    : provider_(std::move(provider)) {
  DCHECK(provider_);
}

SensorProviderImpl::~SensorProviderImpl() {}

void SensorProviderImpl::Bind(
    mojo::PendingReceiver<mojom::SensorProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SensorProviderImpl::GetSensor(mojom::SensorType type,
                                   GetSensorCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses) &&
      IsExtraSensorClass(type)) {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
    return;
  }
  auto cloned_region = provider_->CloneSharedMemoryRegion();
  if (!cloned_region.IsValid()) {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
    return;
  }

  scoped_refptr<PlatformSensor> sensor = provider_->GetSensor(type);
  if (!sensor) {
    provider_->CreateSensor(
        type, base::BindOnce(&SensorProviderImpl::SensorCreated,
                             weak_ptr_factory_.GetWeakPtr(), type,
                             std::move(cloned_region), std::move(callback)));
    return;
  }

  SensorCreated(type, std::move(cloned_region), std::move(callback),
                std::move(sensor));
}

void SensorProviderImpl::SensorCreated(
    mojom::SensorType type,
    base::ReadOnlySharedMemoryRegion cloned_region,
    GetSensorCallback callback,
    scoped_refptr<PlatformSensor> sensor) {
  if (!sensor) {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
    return;
  }

  auto init_params = mojom::SensorInitParams::New();

  auto sensor_impl = std::make_unique<SensorImpl>(sensor);
  init_params->client_receiver = sensor_impl->GetClient();

  mojo::PendingRemote<mojom::Sensor> pending_sensor;
  sensor_receivers_.Add(std::move(sensor_impl),
                        pending_sensor.InitWithNewPipeAndPassReceiver());
  init_params->sensor = std::move(pending_sensor);

  init_params->memory = std::move(cloned_region);
  init_params->buffer_offset = SensorReadingSharedBuffer::GetOffset(type);
  init_params->mode = sensor->GetReportingMode();

  double maximum_frequency = sensor->GetMaximumSupportedFrequency();
  DCHECK_GT(maximum_frequency, 0.0);

  double minimum_frequency = sensor->GetMinimumSupportedFrequency();
  DCHECK_GT(minimum_frequency, 0.0);

  const double maximum_allowed_frequency = GetSensorMaxAllowedFrequency(type);
  if (maximum_frequency > maximum_allowed_frequency)
    maximum_frequency = maximum_allowed_frequency;
  // These checks are to make sure the following assertion is still true:
  // 'minimum_frequency <= default_frequency <= maximum_frequency'
  // after we capped the maximium frequency to the value from traits
  // (and also in case platform gave us some wacky values).
  if (minimum_frequency > maximum_frequency)
    minimum_frequency = maximum_frequency;

  auto default_configuration = sensor->GetDefaultConfiguration();
  if (default_configuration.frequency() > maximum_frequency)
    default_configuration.set_frequency(maximum_frequency);
  if (default_configuration.frequency() < minimum_frequency)
    default_configuration.set_frequency(minimum_frequency);

  init_params->default_configuration = default_configuration;
  init_params->maximum_frequency = maximum_frequency;
  init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();
  DCHECK_GT(init_params->minimum_frequency, 0.0);
  DCHECK_GE(init_params->maximum_frequency, init_params->minimum_frequency);

  std::move(callback).Run(mojom::SensorCreationResult::SUCCESS,
                          std::move(init_params));
}

}  // namespace device

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/sensor_provider_impl.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/sensor_impl.h"
#include "services/device/generic_sensor/virtual_platform_sensor.h"
#include "services/device/generic_sensor/virtual_platform_sensor_provider.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer.h"
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

  // This is used to clean up VirtualSensorProvider instances if they do not
  // have any pending requests or connected sensors, as this class has
  // DeviceService's lifetime but VirtualSensorProviders are per
  // content::WebContents.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &SensorProviderImpl::OnReceiverDisconnected, base::Unretained(this)));
}

SensorProviderImpl::~SensorProviderImpl() = default;

void SensorProviderImpl::Bind(
    mojo::PendingReceiver<mojom::SensorProvider> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SensorProviderImpl::OnReceiverDisconnected() {
  // If the other side of the pipe has been disconnected, the corresponding
  // VirtualPlatformSensorProvider can be destroyed if it has no connected
  // sensors or pending requests, as it is never going to be referenced again.
  // This is the case in most workflows, as the Blink connections to SensorImpl
  // are shut down before WebContentsSensorProviderProxy in content terminates
  // its Mojo connection, but if a VirtualPlatformSensorProvider cannot be
  // removed it is not a big problem, as the data structure is not big.
  const mojo::ReceiverId receiver_id = receivers_.current_receiver();
  auto it = virtual_providers_.find(receiver_id);
  if (it != virtual_providers_.end()) {
    const auto& provider = it->second;
    if (!provider->has_pending_requests() && !provider->has_sensors()) {
      virtual_providers_.erase(it);
    }
  }
}

void SensorProviderImpl::GetSensor(mojom::SensorType type,
                                   GetSensorCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGenericSensorExtraClasses) &&
      IsExtraSensorClass(type)) {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
    return;
  }

  PlatformSensorProvider* provider = provider_.get();
  auto it_virtual_provider =
      virtual_providers_.find(receivers_.current_receiver());
  if (it_virtual_provider != virtual_providers_.end() &&
      it_virtual_provider->second->IsOverridingSensor(type)) {
    provider = it_virtual_provider->second.get();
  }

  auto cloned_region = provider->CloneSharedMemoryRegion();
  if (!cloned_region.IsValid()) {
    std::move(callback).Run(mojom::SensorCreationResult::ERROR_NOT_AVAILABLE,
                            nullptr);
    return;
  }

  scoped_refptr<PlatformSensor> sensor = provider->GetSensor(type);
  if (!sensor) {
    // If we are here, it means there is no virtual sensor of this type,
    // otherwise the GetSensor() call above would have returned it.
    provider->CreateSensor(
        type, base::BindOnce(&SensorProviderImpl::SensorCreated,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(cloned_region), std::move(callback)));
    return;
  }

  SensorCreated(std::move(cloned_region), std::move(callback),
                std::move(sensor));
}

void SensorProviderImpl::SensorCreated(
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
  init_params->buffer_offset =
      GetSensorReadingSharedBufferOffset(sensor->GetType());
  init_params->mode = sensor->GetReportingMode();

  double maximum_frequency = sensor->GetMaximumSupportedFrequency();
  DCHECK_GT(maximum_frequency, 0.0);

  double minimum_frequency = sensor->GetMinimumSupportedFrequency();
  DCHECK_GT(minimum_frequency, 0.0);

  const double maximum_allowed_frequency =
      GetSensorMaxAllowedFrequency(sensor->GetType());
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

void SensorProviderImpl::CreateVirtualSensor(
    mojom::SensorType type,
    mojom::VirtualSensorMetadataPtr metadata,
    CreateVirtualSensorCallback callback) {
  const mojo::ReceiverId receiver_id = receivers_.current_receiver();
  auto& virtual_provider = virtual_providers_[receiver_id];

  if (!virtual_provider) {
    virtual_provider = std::make_unique<VirtualPlatformSensorProvider>();
  }

  mojom::CreateVirtualSensorResult result =
      virtual_provider->AddSensorOverride(type, std::move(metadata))
          ? mojom::CreateVirtualSensorResult::kSuccess
          : mojom::CreateVirtualSensorResult::kSensorTypeAlreadyOverridden;
  std::move(callback).Run(result);
}

void SensorProviderImpl::UpdateVirtualSensor(
    mojom::SensorType type,
    const SensorReading& reading,
    UpdateVirtualSensorCallback callback) {
  auto virtual_provider_it =
      virtual_providers_.find(receivers_.current_receiver());

  if (virtual_provider_it == virtual_providers_.end()) {
    std::move(callback).Run(
        mojom::UpdateVirtualSensorResult::kSensorTypeNotOverridden);
    return;
  }

  auto* virtual_provider = virtual_provider_it->second.get();

  if (!virtual_provider->IsOverridingSensor(type)) {
    std::move(callback).Run(
        mojom::UpdateVirtualSensorResult::kSensorTypeNotOverridden);
    return;
  }

  virtual_provider->AddReading(type, reading);
  std::move(callback).Run(mojom::UpdateVirtualSensorResult::kSuccess);
}

void SensorProviderImpl::RemoveVirtualSensor(
    mojom::SensorType type,
    RemoveVirtualSensorCallback callback) {
  auto virtual_provider_it =
      virtual_providers_.find(receivers_.current_receiver());

  if (virtual_provider_it == virtual_providers_.end()) {
    std::move(callback).Run();
    return;
  }

  virtual_provider_it->second->RemoveSensorOverride(type);
  std::move(callback).Run();
}

void SensorProviderImpl::GetVirtualSensorInformation(
    mojom::SensorType type,
    GetVirtualSensorInformationCallback callback) {
  auto virtual_provider_it =
      virtual_providers_.find(receivers_.current_receiver());

  if (virtual_provider_it == virtual_providers_.end()) {
    std::move(callback).Run(mojom::GetVirtualSensorInformationResult::NewError(
        mojom::GetVirtualSensorInformationError::kSensorTypeNotOverridden));
    return;
  }

  auto* virtual_provider = virtual_provider_it->second.get();

  if (!virtual_provider->IsOverridingSensor(type)) {
    std::move(callback).Run(mojom::GetVirtualSensorInformationResult::NewError(
        mojom::GetVirtualSensorInformationError::kSensorTypeNotOverridden));
    return;
  }

  auto platform_sensor = virtual_provider->GetSensor(type);
  if (!platform_sensor) {
    // The sensor has not been created yet.
    std::move(callback).Run(mojom::GetVirtualSensorInformationResult::NewInfo(
        mojom::VirtualSensorInformation::New(/*sampling_frequency=*/0.0)));
    return;
  }

  auto* virtual_sensor =
      static_cast<VirtualPlatformSensor*>(platform_sensor.get());
  auto sensor_information = mojom::VirtualSensorInformation::New();
  sensor_information->sampling_frequency =
      virtual_sensor->optimal_configuration().has_value()
          ? virtual_sensor->optimal_configuration().value().frequency()
          : 0.0;

  std::move(callback).Run(mojom::GetVirtualSensorInformationResult::NewInfo(
      std::move(sensor_information)));
}

size_t SensorProviderImpl::GetVirtualProviderCountForTesting() const {
  return virtual_providers_.size();
}

const VirtualPlatformSensorProvider*
SensorProviderImpl::GetLastVirtualSensorProviderForTesting() const {
  CHECK_EQ(virtual_providers_.size(), 1U);
  return virtual_providers_.begin()->second.get();
}

}  // namespace device

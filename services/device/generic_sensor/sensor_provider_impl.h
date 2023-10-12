// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

class PlatformSensorProvider;
class PlatformSensor;
class VirtualPlatformSensorProvider;

// Implementation of SensorProvider mojo interface. Owns an instance of
// PlatformSensorProvider to create platform specific instances of
// PlatformSensor which are used by SensorImpl. A single instance of this class
// is owned by DeviceService.
class SensorProviderImpl final : public mojom::SensorProvider {
 public:
  explicit SensorProviderImpl(std::unique_ptr<PlatformSensorProvider> provider);

  SensorProviderImpl(const SensorProviderImpl&) = delete;
  SensorProviderImpl& operator=(const SensorProviderImpl&) = delete;

  ~SensorProviderImpl() override;

  void Bind(mojo::PendingReceiver<mojom::SensorProvider> receiver);

  size_t GetVirtualProviderCountForTesting() const;
  const VirtualPlatformSensorProvider* GetLastVirtualSensorProviderForTesting()
      const;

 private:
  // mojom::SensorProvider implementation.
  void GetSensor(mojom::SensorType type, GetSensorCallback callback) override;
  void CreateVirtualSensor(mojom::SensorType type,
                           mojom::VirtualSensorMetadataPtr metadata,
                           CreateVirtualSensorCallback callback) override;
  void UpdateVirtualSensor(mojom::SensorType type,
                           const SensorReading& reading,
                           UpdateVirtualSensorCallback callback) override;
  void RemoveVirtualSensor(mojom::SensorType type,
                           RemoveVirtualSensorCallback callback) override;
  void GetVirtualSensorInformation(
      mojom::SensorType type,
      GetVirtualSensorInformationCallback callback) override;

  // Helper callback method to return created sensors.
  void SensorCreated(base::ReadOnlySharedMemoryRegion cloned_region,
                     GetSensorCallback callback,
                     scoped_refptr<PlatformSensor> sensor);

  void OnReceiverDisconnected();

  std::unique_ptr<PlatformSensorProvider> provider_;
  std::map<mojo::ReceiverId, std::unique_ptr<VirtualPlatformSensorProvider>>
      virtual_providers_;
  mojo::ReceiverSet<mojom::SensorProvider> receivers_;
  mojo::UniqueReceiverSet<mojom::Sensor> sensor_receivers_;
  base::WeakPtrFactory<SensorProviderImpl> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_

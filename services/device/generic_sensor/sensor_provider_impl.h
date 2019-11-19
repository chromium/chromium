// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/device/public/mojom/sensor_provider.mojom.h"

namespace device {

class PlatformSensorProvider;
class PlatformSensor;

// Implementation of SensorProvider mojo interface. Owns an instance of
// PlatformSensorProvider to create platform specific instances of
// PlatformSensor which are used by SensorImpl. A single instance of this class
// is owned by DeviceService.
class SensorProviderImpl final : public mojom::SensorProvider {
 public:
  explicit SensorProviderImpl(std::unique_ptr<PlatformSensorProvider> provider);
  ~SensorProviderImpl() override;

  void Bind(mojo::PendingReceiver<mojom::SensorProvider> receiver);

 private:
  // SensorProvider implementation.
  void GetSensor(mojom::SensorType type,
                 GetSensorCallback callback) override;

  // Helper callback method to return created sensors.
  void SensorCreated(mojom::SensorType type,
                     mojo::ScopedSharedBufferHandle cloned_handle,
                     GetSensorCallback callback,
                     scoped_refptr<PlatformSensor> sensor);

  std::unique_ptr<PlatformSensorProvider> provider_;
  mojo::ReceiverSet<mojom::SensorProvider> receivers_;
  mojo::UniqueReceiverSet<mojom::Sensor> sensor_receivers_;
  base::WeakPtrFactory<SensorProviderImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SensorProviderImpl);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_SENSOR_PROVIDER_IMPL_H_

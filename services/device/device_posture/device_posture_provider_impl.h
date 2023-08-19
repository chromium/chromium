// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PROVIDER_IMPL_H_
#define SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PROVIDER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/device_posture_provider.mojom.h"

namespace device {

class DevicePosturePlatformProvider;

class DevicePostureProviderImpl : public mojom::DevicePostureProvider {
 public:
  explicit DevicePostureProviderImpl(
      std::unique_ptr<DevicePosturePlatformProvider> posture_provider);

  ~DevicePostureProviderImpl() override;
  DevicePostureProviderImpl(const DevicePostureProviderImpl&) = delete;
  DevicePostureProviderImpl& operator=(const DevicePostureProviderImpl&) =
      delete;

  // Adds this receiver to |receiverss_|.
  void Bind(mojo::PendingReceiver<mojom::DevicePostureProvider> receiver);
  void OnDevicePostureChanged(const mojom::DevicePostureType& posture);
  void OnViewportSegmentsChanged(const std::vector<gfx::Rect>& segments);

 private:
  // DevicePostureProvider implementation.
  void AddListenerAndGetCurrentPosture(
      mojo::PendingRemote<mojom::DevicePostureClient> client,
      AddListenerAndGetCurrentPostureCallback callback) override;
  void AddListenerAndGetCurrentViewportSegments(
      mojo::PendingRemote<mojom::DeviceViewportSegmentsClient> client,
      AddListenerAndGetCurrentViewportSegmentsCallback callback) override;
  void OnReceiverConnectionError();

  std::unique_ptr<DevicePosturePlatformProvider> platform_provider_;
  mojo::ReceiverSet<mojom::DevicePostureProvider> receivers_;
  mojo::RemoteSet<mojom::DevicePostureClient> posture_clients_;
  mojo::RemoteSet<mojom::DeviceViewportSegmentsClient>
      viewport_segments_clients_;
  base::WeakPtrFactory<DevicePostureProviderImpl> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_DEVICE_POSTURE_DEVICE_POSTURE_PROVIDER_IMPL_H_

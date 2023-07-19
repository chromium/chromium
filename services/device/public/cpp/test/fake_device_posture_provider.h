// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_DEVICE_POSTURE_PROVIDER_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_DEVICE_POSTURE_PROVIDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/mojom/device_posture_provider.mojom.h"

namespace device {

class FakeDevicePostureProvider : public mojom::DevicePostureProvider {
 public:
  FakeDevicePostureProvider();
  ~FakeDevicePostureProvider() override;

  FakeDevicePostureProvider(const FakeDevicePostureProvider&) = delete;
  FakeDevicePostureProvider& operator=(const FakeDevicePostureProvider&) =
      delete;

  // mojom::DevicePostureProvider:
  void AddListenerAndGetCurrentPosture(
      mojo::PendingRemote<mojom::DevicePostureClient> client,
      AddListenerAndGetCurrentPostureCallback callback) override;
  void AddListenerAndGetCurrentViewportSegments(
      mojo::PendingRemote<mojom::DeviceViewportSegmentsClient> client,
      AddListenerAndGetCurrentViewportSegmentsCallback callback) override;

  void SetCurrentPostureForTesting(device::mojom::DevicePostureType posture);
  void Bind(mojo::PendingReceiver<mojom::DevicePostureProvider> receiver);

 private:
  void DispatchPostureChanges();
  mojo::ReceiverSet<mojom::DevicePostureProvider> receivers_;
  mojo::RemoteSet<mojom::DevicePostureClient> posture_clients_;
  mojo::RemoteSet<mojom::DeviceViewportSegmentsClient>
      viewport_segment_clients_;
  mojom::DevicePostureType current_posture_ =
      mojom::DevicePostureType::kContinuous;
  std::vector<gfx::Rect> current_viewport_segments_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_DEVICE_POSTURE_PROVIDER_H_

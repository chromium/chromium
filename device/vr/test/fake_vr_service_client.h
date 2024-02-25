// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_TEST_FAKE_VR_SERVICE_CLIENT_H_
#define DEVICE_VR_TEST_FAKE_VR_SERVICE_CLIENT_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_device.mojom.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace device {
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT FakeVRServiceClient : public mojom::VRServiceClient {
 public:
  explicit FakeVRServiceClient(
      mojo::PendingReceiver<mojom::VRServiceClient> receiver);

  FakeVRServiceClient(const FakeVRServiceClient&) = delete;
  FakeVRServiceClient& operator=(const FakeVRServiceClient&) = delete;

  ~FakeVRServiceClient() override;

  void OnDeviceChanged() override {}
  void SetLastDeviceId(mojom::XRDeviceId id);
  bool CheckDeviceId(mojom::XRDeviceId id);

 private:
  mojom::XRDeviceId last_device_id_ = static_cast<mojom::XRDeviceId>(0);
  mojo::Receiver<mojom::VRServiceClient> receiver_;
};

}  // namespace device

#endif  // DEVICE_VR_TEST_FAKE_VR_SERVICE_CLIENT_H_

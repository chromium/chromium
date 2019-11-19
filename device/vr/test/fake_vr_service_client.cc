// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/test/fake_vr_service_client.h"

namespace device {

FakeVRServiceClient::FakeVRServiceClient(
    mojo::PendingReceiver<mojom::VRServiceClient> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeVRServiceClient::~FakeVRServiceClient() {}

void FakeVRServiceClient::SetLastDeviceId(mojom::XRDeviceId id) {
  last_device_id_ = id;
}

bool FakeVRServiceClient::CheckDeviceId(mojom::XRDeviceId id) {
  return id == last_device_id_;
}

}  // namespace device

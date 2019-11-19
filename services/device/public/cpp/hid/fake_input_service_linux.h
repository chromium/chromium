// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_FAKE_INPUT_SERVICE_LINUX_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_FAKE_INPUT_SERVICE_LINUX_H_

#include <map>
#include <string>

#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace device {

class FakeInputServiceLinux : public mojom::InputDeviceManager {
 public:
  using DeviceMap = std::map<std::string, mojom::InputDeviceInfoPtr>;

  FakeInputServiceLinux();
  ~FakeInputServiceLinux() override;

  // mojom::InputDeviceManager implementation:
  void GetDevicesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::InputDeviceManagerClient> client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;

  void Bind(mojo::PendingReceiver<mojom::InputDeviceManager> receiver);
  void AddDevice(mojom::InputDeviceInfoPtr info);
  void RemoveDevice(const std::string& id);

  DeviceMap devices_;

 private:
  mojo::ReceiverSet<mojom::InputDeviceManager> receivers_;
  mojo::AssociatedRemoteSet<mojom::InputDeviceManagerClient> clients_;

  DISALLOW_COPY_AND_ASSIGN(FakeInputServiceLinux);
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_FAKE_INPUT_SERVICE_LINUX_H_

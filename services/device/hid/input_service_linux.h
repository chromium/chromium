// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_INPUT_SERVICE_LINUX_H_
#define SERVICES_DEVICE_HID_INPUT_SERVICE_LINUX_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace device {

// This class provides information and notifications about
// connected/disconnected input/HID devices.
class InputServiceLinux : public mojom::InputDeviceManager {
 public:
  using DeviceMap = std::map<std::string, mojom::InputDeviceInfoPtr>;

  InputServiceLinux();

  InputServiceLinux(const InputServiceLinux&) = delete;
  InputServiceLinux& operator=(const InputServiceLinux&) = delete;

  ~InputServiceLinux() override;

  // Binds the |receiver| to an InputServiceLinux instance.
  static void BindReceiver(
      mojo::PendingReceiver<mojom::InputDeviceManager> receiver);

  // Returns the InputServiceLinux instance for the current process. Creates one
  // if none has been set.
  static InputServiceLinux* GetInstance();

  // Returns true if an InputServiceLinux instance has been set for the current
  // process. An instance is set on the first call to GetInstance() or
  // SetForTesting().
  static bool HasInstance();

  // Sets the InputServiceLinux instance for the current process. Cannot be
  // called if GetInstance() or SetForTesting() has already been called in the
  // current process. |service| will never be deleted.
  static void SetForTesting(std::unique_ptr<InputServiceLinux> service);

  void AddReceiver(mojo::PendingReceiver<mojom::InputDeviceManager> receiver);

  // mojom::InputDeviceManager implementation:
  void GetDevicesAndSetClient(
      mojo::PendingAssociatedRemote<mojom::InputDeviceManagerClient> client,
      GetDevicesCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void AddDevice(mojom::InputDeviceInfoPtr info);
  void RemoveDevice(const std::string& id);

 protected:
  DeviceMap devices_;

 private:
  THREAD_CHECKER(thread_checker_);
  mojo::ReceiverSet<mojom::InputDeviceManager> receivers_;
  mojo::AssociatedRemoteSet<mojom::InputDeviceManagerClient> clients_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_INPUT_SERVICE_LINUX_H_

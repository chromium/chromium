// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_LINUX_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_LINUX_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/unguessable_token.h"
#include "device/udev_linux/udev_watcher.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorLinux : public SerialDeviceEnumerator,
                                    public UdevWatcher::Observer {
 public:
  SerialDeviceEnumeratorLinux();
  ~SerialDeviceEnumeratorLinux() override;

  // SerialDeviceEnumerator
  std::vector<mojom::SerialPortInfoPtr> GetDevices() override;
  base::Optional<base::FilePath> GetPathFromToken(
      const base::UnguessableToken& token) override;

  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;

 private:
  void CreatePort(ScopedUdevDevicePtr device, const std::string& syspath);

  std::unique_ptr<UdevWatcher> watcher_;
  std::map<base::UnguessableToken, mojom::SerialPortInfoPtr> ports_;
  std::map<std::string, base::UnguessableToken> paths_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SerialDeviceEnumeratorLinux);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_LINUX_H_

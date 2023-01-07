// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_LINUX_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_LINUX_H_

#include <map>
#include <memory>
#include <string>

#include "device/udev_linux/udev_watcher.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumeratorLinux : public SerialDeviceEnumerator,
                                    public UdevWatcher::Observer {
 public:
  static std::unique_ptr<SerialDeviceEnumeratorLinux> Create();

  explicit SerialDeviceEnumeratorLinux(
      const base::FilePath& tty_driver_info_path);

  SerialDeviceEnumeratorLinux(const SerialDeviceEnumeratorLinux&) = delete;
  SerialDeviceEnumeratorLinux& operator=(const SerialDeviceEnumeratorLinux&) =
      delete;

  ~SerialDeviceEnumeratorLinux() override;

  // UdevWatcher::Observer
  void OnDeviceAdded(ScopedUdevDevicePtr device) override;
  void OnDeviceChanged(ScopedUdevDevicePtr device) override;
  void OnDeviceRemoved(ScopedUdevDevicePtr device) override;

 private:
  void CreatePort(ScopedUdevDevicePtr device, const std::string& syspath);

  std::unique_ptr<UdevWatcher> watcher_;
  const base::FilePath tty_driver_info_path_;
  std::map<std::string, base::UnguessableToken> paths_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_LINUX_H_

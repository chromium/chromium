// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_FAKE_SERIAL_DEVICE_ENUMERATOR_H_
#define SERVICES_DEVICE_SERIAL_FAKE_SERIAL_DEVICE_ENUMERATOR_H_

#include <map>

#include "base/files/file_path.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

class FakeSerialEnumerator : public SerialDeviceEnumerator {
 public:
  FakeSerialEnumerator();

  FakeSerialEnumerator(const FakeSerialEnumerator&) = delete;
  FakeSerialEnumerator& operator=(const FakeSerialEnumerator&) = delete;

  ~FakeSerialEnumerator() override;

  void AddDevicePath(const base::FilePath& path);
  void RemoveDevicePath(const base::FilePath& path);

 private:
  std::map<base::FilePath, base::UnguessableToken> paths_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_FAKE_SERIAL_DEVICE_ENUMERATOR_H_

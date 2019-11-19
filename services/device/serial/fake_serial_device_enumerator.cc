// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/fake_serial_device_enumerator.h"

#include <utility>

#include "base/stl_util.h"

namespace device {

FakeSerialEnumerator::FakeSerialEnumerator() = default;

FakeSerialEnumerator::~FakeSerialEnumerator() = default;

bool FakeSerialEnumerator::AddDevicePath(const base::FilePath& path) {
  if (base::Contains(device_paths_, path))
    return false;

  device_paths_.push_back(path);
  return true;
}

std::vector<mojom::SerialPortInfoPtr> FakeSerialEnumerator::GetDevices() {
  std::vector<device::mojom::SerialPortInfoPtr> devices;
  for (const auto& path : device_paths_) {
    auto device = device::mojom::SerialPortInfo::New();
    device->token = GetTokenFromPath(path);
    device->path = path;
    devices.push_back(std::move(device));
  }
  return devices;
}

}  // namespace device

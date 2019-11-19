// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/memory/ref_counted_memory.h"
#include "services/device/hid/mock_hid_connection.h"
#include "services/device/hid/mock_hid_service.h"

namespace device {

MockHidService::MockHidService() {}

MockHidService::~MockHidService() = default;

base::WeakPtr<HidService> MockHidService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MockHidService::AddDevice(scoped_refptr<HidDeviceInfo> info) {
  HidService::AddDevice(info);
}

void MockHidService::RemoveDevice(
    const HidPlatformDeviceId& platform_device_id) {
  HidService::RemoveDevice(platform_device_id);
}

void MockHidService::FirstEnumerationComplete() {
  HidService::FirstEnumerationComplete();
}

void MockHidService::Connect(const std::string& device_id,
                             const ConnectCallback& callback) {
  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    callback.Run(nullptr);
    return;
  }

  auto connection = base::MakeRefCounted<MockHidConnection>(map_entry->second);

  // Set up a single input report that is ready to be read from the device.
  // The first byte is the report id.
  const uint8_t data[] = "\1TestRead";
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(data, sizeof(data) - 1);
  connection->MockInputReport(std::move(buffer));

  callback.Run(connection);
}

const std::map<std::string, scoped_refptr<HidDeviceInfo>>&
MockHidService::devices() const {
  return HidService::devices();
}

}  // namespace device

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/mock_hid_service.h"

#include <map>

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "services/device/public/cpp/test/mock_hid_connection.h"

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
                             bool allow_protected_reports,
                             bool allow_fido_reports,
                             ConnectCallback callback) {
  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto connection = base::MakeRefCounted<MockHidConnection>(map_entry->second);

  // Set up a single input report that is ready to be read from the device.
  // The first byte is the report id.
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      base::byte_span_from_cstring("\1TestRead"));
  connection->MockInputReport(std::move(buffer));

  std::move(callback).Run(connection);
}

const std::map<std::string, scoped_refptr<HidDeviceInfo>>&
MockHidService::devices() const {
  return HidService::devices();
}

}  // namespace device

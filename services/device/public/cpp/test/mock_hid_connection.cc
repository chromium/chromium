// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/mock_hid_connection.h"

#include "base/containers/span.h"
#include "base/memory/ref_counted_memory.h"
#include "services/device/hid/hid_device_info.h"

namespace device {

MockHidConnection::MockHidConnection(scoped_refptr<HidDeviceInfo> device)
    : HidConnection(device,
                    /*allow_protected_reports=*/false,
                    /*allow_fido_reports=*/false) {}

MockHidConnection::~MockHidConnection() {}

void MockHidConnection::PlatformClose() {}

void MockHidConnection::PlatformWrite(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  std::move(callback).Run(true);
}

void MockHidConnection::PlatformGetFeatureReport(uint8_t report_id,
                                                 ReadCallback callback) {
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(
      base::byte_span_from_cstring("TestGetFeatureReport"));
  std::move(callback).Run(true, buffer, buffer->size());
}

void MockHidConnection::PlatformSendFeatureReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    WriteCallback callback) {
  std::move(callback).Run(true);
}

void MockHidConnection::MockInputReport(
    scoped_refptr<base::RefCountedBytes> buffer) {
  ProcessInputReport(buffer, buffer->size());
}

}  // namespace device

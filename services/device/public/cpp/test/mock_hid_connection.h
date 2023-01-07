// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_HID_CONNECTION_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_HID_CONNECTION_H_

#include "services/device/hid/hid_connection.h"

namespace device {

class MockHidConnection : public HidConnection {
 public:
  friend class base::RefCountedThreadSafe<MockHidConnection>;

  explicit MockHidConnection(scoped_refptr<HidDeviceInfo> device);
  MockHidConnection(MockHidConnection&) = delete;
  MockHidConnection& operator=(MockHidConnection&) = delete;

  // HidConnection implementation.
  void PlatformClose() override;
  void PlatformWrite(scoped_refptr<base::RefCountedBytes> buffer,
                     WriteCallback callback) override;
  void PlatformGetFeatureReport(uint8_t report_id,
                                ReadCallback callback) override;
  void PlatformSendFeatureReport(scoped_refptr<base::RefCountedBytes> buffer,
                                 WriteCallback callback) override;

  void MockInputReport(scoped_refptr<base::RefCountedBytes> buffer);

 private:
  ~MockHidConnection() override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_HID_CONNECTION_H_

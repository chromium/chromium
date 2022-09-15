// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_HID_SERVICE_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_HID_SERVICE_H_

#include "services/device/hid/hid_service.h"

namespace device {

class MockHidService : public HidService {
 public:
  MockHidService();
  MockHidService(MockHidService&) = delete;
  MockHidService& operator=(MockHidService&) = delete;
  ~MockHidService() override;

  // Public wrappers around protected functions needed for tests.
  void AddDevice(scoped_refptr<HidDeviceInfo> info);
  void RemoveDevice(const HidPlatformDeviceId& platform_device_id);
  void FirstEnumerationComplete();
  const std::map<std::string, scoped_refptr<HidDeviceInfo>>& devices() const;

  void Connect(const std::string& device_id,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;

 private:
  base::WeakPtr<HidService> GetWeakPtr() override;

  base::WeakPtrFactory<MockHidService> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_MOCK_HID_SERVICE_H_

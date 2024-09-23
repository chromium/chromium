// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_FUCHSIA_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_FUCHSIA_H_

#include "services/device/hid/hid_service.h"

namespace device {

// TODO(crbug.com/42050450): Implement this.
class HidServiceFuchsia : public HidService {
 public:
  HidServiceFuchsia();
  ~HidServiceFuchsia() override;

  HidServiceFuchsia(const HidServiceFuchsia&) = delete;
  HidServiceFuchsia& operator=(const HidServiceFuchsia&) = delete;

 private:
  // HidService implementation.
  void Connect(const std::string& device_id,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

  base::WeakPtrFactory<HidServiceFuchsia> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_FUCHSIA_H_

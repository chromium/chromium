// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_PREPARSED_DATA_H_
#define SERVICES_DEVICE_HID_HID_PREPARSED_DATA_H_

#include <windows.h>

// NOTE: <hidsdi.h> must be included before <hidpi.h>. clang-format will want to
// reorder them.
// clang-format off
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
// clang-format on

#include <memory>
#include <vector>

#include "services/device/hid/hid_service_win.h"

namespace device {

class HidPreparsedData : public HidServiceWin::PreparsedData {
 public:
  // Return a HidPreparsedData constructed from an open |device_handle|, or
  // nullptr if the handle is invalid or the device data could not be read.
  static std::unique_ptr<HidPreparsedData> Create(HANDLE device_handle);

  HidPreparsedData(const HidPreparsedData&) = delete;
  HidPreparsedData& operator=(const HidPreparsedData&) = delete;
  ~HidPreparsedData() override;

  // HidServiceWin::PreparsedData implementation.
  const HIDP_CAPS& GetCaps() const override;
  std::vector<ReportItem> GetReportItems(
      HIDP_REPORT_TYPE report_type) const override;

 private:
  HidPreparsedData(PHIDP_PREPARSED_DATA preparsed_data, HIDP_CAPS capabilities);

  const PHIDP_PREPARSED_DATA preparsed_data_;
  const HIDP_CAPS capabilities_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_PREPARSED_DATA_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_UTILS_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_UTILS_H_

#include "services/device/public/cpp/hid/hid_report_type.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// Indicates whether reports of type `report_type` that appear in a top-level
// collection with usage `hid_usage_and_page` should be blocked.
bool IsAlwaysProtected(const mojom::HidUsageAndPage& hid_usage_and_page,
                       HidReportType report_type);

// Returns true if `collection` contains any unprotected reports, or false if
// all its reports are always protected.
bool CollectionHasUnprotectedReports(
    const mojom::HidCollectionInfo& collection);

// Returns the collection in `device` containing a report with ID `report_id`
// and type `report_type`, or nullptr if there is no matching report.
const mojom::HidCollectionInfo* FindCollectionWithReport(
    const mojom::HidDeviceInfo& device,
    uint8_t report_id,
    HidReportType report_type);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_UTILS_H_

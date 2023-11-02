// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_HID_TEST_UTIL_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_HID_TEST_UTIL_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "services/device/public/mojom/hid.mojom-forward.h"

namespace device {

// Returns a mojom::HidDeviceInfoPtr with the specified |vendor_id| and
// |product_id|. The |collections| member is populated by the information in
// |report_descriptor_data|.
mojom::HidDeviceInfoPtr CreateDeviceFromReportDescriptor(
    uint16_t vendor_id,
    uint16_t product_id,
    base::span<const uint8_t> report_descriptor_data);

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_HID_TEST_UTIL_H_

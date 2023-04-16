// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/hid_test_util.h"

#include "base/uuid.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"
#include "services/device/public/cpp/hid/hid_report_descriptor.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

mojom::HidDeviceInfoPtr CreateDeviceFromReportDescriptor(
    uint16_t vendor_id,
    uint16_t product_id,
    base::span<const uint8_t> report_descriptor_data) {
  auto device = mojom::HidDeviceInfo::New();
  device->guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  device->vendor_id = vendor_id;
  device->product_id = product_id;
  device->product_name = "Test Device";
  device->bus_type = mojom::HidBusType::kHIDBusTypeUSB;
  device->report_descriptor.insert(device->report_descriptor.begin(),
                                   report_descriptor_data.begin(),
                                   report_descriptor_data.end());
  size_t max_input_report_size;
  size_t max_output_report_size;
  size_t max_feature_report_size;
  HidReportDescriptor descriptor_parser(report_descriptor_data);
  descriptor_parser.GetDetails(&device->collections, &device->has_report_id,
                               &max_input_report_size, &max_output_report_size,
                               &max_feature_report_size);
  device->max_input_report_size = max_input_report_size;
  device->max_output_report_size = max_output_report_size;
  device->max_feature_report_size = max_feature_report_size;
  auto& blocklist = HidBlocklist::Get();
  device->protected_input_report_ids =
      blocklist.GetProtectedReportIds(HidBlocklist::kReportTypeInput, vendor_id,
                                      product_id, device->collections);
  device->protected_output_report_ids = blocklist.GetProtectedReportIds(
      HidBlocklist::kReportTypeOutput, vendor_id, product_id,
      device->collections);
  device->protected_feature_report_ids = blocklist.GetProtectedReportIds(
      HidBlocklist::kReportTypeFeature, vendor_id, product_id,
      device->collections);
  device->is_excluded_by_blocklist =
      blocklist.IsVendorProductBlocked(vendor_id, product_id);
  return device;
}

}  // namespace device

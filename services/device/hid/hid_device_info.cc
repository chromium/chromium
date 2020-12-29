// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_device_info.h"

#include "base/guid.h"
#include "build/build_config.h"
#include "services/device/public/cpp/hid/hid_blocklist.h"
#include "services/device/public/cpp/hid/hid_report_descriptor.h"

namespace device {

HidDeviceInfo::PlatformDeviceIdEntry::PlatformDeviceIdEntry(
    base::flat_set<uint8_t> report_ids,
    HidPlatformDeviceId platform_device_id)
    : report_ids(std::move(report_ids)),
      platform_device_id(platform_device_id) {}

HidDeviceInfo::PlatformDeviceIdEntry::PlatformDeviceIdEntry(
    const PlatformDeviceIdEntry& entry) = default;

HidDeviceInfo::PlatformDeviceIdEntry&
HidDeviceInfo::PlatformDeviceIdEntry::operator=(
    const PlatformDeviceIdEntry& entry) = default;

HidDeviceInfo::PlatformDeviceIdEntry::~PlatformDeviceIdEntry() = default;

HidDeviceInfo::HidDeviceInfo(HidPlatformDeviceId platform_device_id,
                             const std::string& physical_device_id,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& product_name,
                             const std::string& serial_number,
                             mojom::HidBusType bus_type,
                             const std::vector<uint8_t> report_descriptor,
                             std::string device_node) {
  std::vector<mojom::HidCollectionInfoPtr> collections;
  bool has_report_id;
  size_t max_input_report_size;
  size_t max_output_report_size;
  size_t max_feature_report_size;

  HidReportDescriptor descriptor_parser(report_descriptor);
  descriptor_parser.GetDetails(&collections, &has_report_id,
                               &max_input_report_size, &max_output_report_size,
                               &max_feature_report_size);

  auto protected_input_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeInput, vendor_id, product_id, collections);
  auto protected_output_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeOutput, vendor_id, product_id, collections);
  auto protected_feature_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeFeature, vendor_id, product_id, collections);

  std::vector<uint8_t> report_ids;
  if (has_report_id) {
    for (const auto& collection : collections) {
      report_ids.insert(report_ids.end(), collection->report_ids.begin(),
                        collection->report_ids.end());
    }
  } else {
    report_ids.push_back(0);
  }
  platform_device_id_map_.emplace_back(report_ids, platform_device_id);

  device_ = mojom::HidDeviceInfo::New(
      base::GenerateGUID(), physical_device_id, vendor_id, product_id,
      product_name, serial_number, bus_type, report_descriptor,
      std::move(collections), has_report_id, max_input_report_size,
      max_output_report_size, max_feature_report_size, device_node,
      protected_input_report_ids, protected_output_report_ids,
      protected_feature_report_ids);
}

HidDeviceInfo::HidDeviceInfo(
    PlatformDeviceIdMap platform_device_id_map,
    const std::string& physical_device_id,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::string& product_name,
    const std::string& serial_number,
    mojom::HidBusType bus_type,
    std::vector<mojom::HidCollectionInfoPtr> collections,
    size_t max_input_report_size,
    size_t max_output_report_size,
    size_t max_feature_report_size)
    : platform_device_id_map_(std::move(platform_device_id_map)) {
  bool has_report_id = false;
  for (const auto& collection : collections) {
    if (!collection->report_ids.empty()) {
      has_report_id = true;
      break;
    }
  }

  auto protected_input_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeInput, vendor_id, product_id, collections);
  auto protected_output_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeOutput, vendor_id, product_id, collections);
  auto protected_feature_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeFeature, vendor_id, product_id, collections);

  std::vector<uint8_t> report_descriptor;
  device_ = mojom::HidDeviceInfo::New(
      base::GenerateGUID(), physical_device_id, vendor_id, product_id,
      product_name, serial_number, bus_type, report_descriptor,
      std::move(collections), has_report_id, max_input_report_size,
      max_output_report_size, max_feature_report_size, "",
      protected_input_report_ids, protected_output_report_ids,
      protected_feature_report_ids);
}

HidDeviceInfo::~HidDeviceInfo() = default;

}  // namespace device

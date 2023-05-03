// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_device_info.h"

#include "base/containers/contains.h"
#include "base/uuid.h"
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
                             base::span<const uint8_t> report_descriptor,
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
  auto is_excluded_by_blocklist =
      HidBlocklist::Get().IsVendorProductBlocked(vendor_id, product_id);

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
      base::Uuid::GenerateRandomV4().AsLowercaseString(), physical_device_id,
      vendor_id, product_id, product_name, serial_number, bus_type,
      std::vector<uint8_t>(report_descriptor.begin(), report_descriptor.end()),
      std::move(collections), has_report_id, max_input_report_size,
      max_output_report_size, max_feature_report_size, device_node,
      protected_input_report_ids, protected_output_report_ids,
      protected_feature_report_ids, is_excluded_by_blocklist);
}

HidDeviceInfo::HidDeviceInfo(HidPlatformDeviceId platform_device_id,
                             const std::string& physical_device_id,
                             const std::string& interface_id,
                             uint16_t vendor_id,
                             uint16_t product_id,
                             const std::string& product_name,
                             const std::string& serial_number,
                             mojom::HidBusType bus_type,
                             mojom::HidCollectionInfoPtr collection,
                             size_t max_input_report_size,
                             size_t max_output_report_size,
                             size_t max_feature_report_size)
    : interface_id_(interface_id) {
  bool has_report_id = !collection->report_ids.empty();
  if (has_report_id) {
    platform_device_id_map_.emplace_back(collection->report_ids,
                                         platform_device_id);
  } else {
    platform_device_id_map_.emplace_back(std::vector<uint8_t>{0},
                                         platform_device_id);
  }
  std::vector<mojom::HidCollectionInfoPtr> collections;
  collections.push_back(std::move(collection));
  auto protected_input_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeInput, vendor_id, product_id, collections);
  auto protected_output_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeOutput, vendor_id, product_id, collections);
  auto protected_feature_report_ids = HidBlocklist::Get().GetProtectedReportIds(
      HidBlocklist::kReportTypeFeature, vendor_id, product_id, collections);
  auto is_excluded_by_blocklist =
      HidBlocklist::Get().IsVendorProductBlocked(vendor_id, product_id);

  device_ = mojom::HidDeviceInfo::New(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), physical_device_id,
      vendor_id, product_id, product_name, serial_number, bus_type,
      /*report_descriptor=*/std::vector<uint8_t>{}, std::move(collections),
      has_report_id, max_input_report_size, max_output_report_size,
      max_feature_report_size, /*device_node=*/"", protected_input_report_ids,
      protected_output_report_ids, protected_feature_report_ids,
      is_excluded_by_blocklist);
}

HidDeviceInfo::~HidDeviceInfo() = default;

void HidDeviceInfo::AppendDeviceInfo(scoped_refptr<HidDeviceInfo> device_info) {
  // Check that |device_info| has an interface ID and it matches ours.
  DCHECK(interface_id_);
  DCHECK(device_info->interface_id());
  DCHECK_EQ(*interface_id_, *device_info->interface_id());

  // Check that the device-level properties are identical.
  DCHECK_EQ(device_->physical_device_id, device_info->physical_device_id());
  DCHECK_EQ(device_->vendor_id, device_info->vendor_id());
  DCHECK_EQ(device_->product_id, device_info->product_id());
  DCHECK_EQ(device_->product_name, device_info->product_name());
  DCHECK_EQ(device_->serial_number, device_info->serial_number());
  DCHECK_EQ(device_->bus_type, device_info->bus_type());

  // Append collections from |device_info|.
  for (const auto& collection : device_info->collections())
    device_->collections.push_back(collection->Clone());

  // Append platform device IDs from |device_info|.
  for (const auto& entry : device_info->platform_device_id_map()) {
    platform_device_id_map_.push_back(
        {entry.report_ids, entry.platform_device_id});
  }

  // Update the maximum report sizes.
  device_->max_input_report_size = std::max(
      device_->max_input_report_size, device_info->max_input_report_size());
  device_->max_output_report_size = std::max(
      device_->max_output_report_size, device_info->max_output_report_size());
  device_->max_feature_report_size = std::max(
      device_->max_feature_report_size, device_info->max_feature_report_size());

  // Update the protected report IDs.
  device_->protected_input_report_ids =
      HidBlocklist::Get().GetProtectedReportIds(
          HidBlocklist::kReportTypeInput, device_->vendor_id,
          device_->product_id, device_->collections);
  device_->protected_output_report_ids =
      HidBlocklist::Get().GetProtectedReportIds(
          HidBlocklist::kReportTypeOutput, device_->vendor_id,
          device_->product_id, device_->collections);
  device_->protected_feature_report_ids =
      HidBlocklist::Get().GetProtectedReportIds(
          HidBlocklist::kReportTypeFeature, device_->vendor_id,
          device_->product_id, device_->collections);

  device_->is_excluded_by_blocklist =
      HidBlocklist::Get().IsVendorProductBlocked(device_->vendor_id,
                                                 device_->product_id);
}

}  // namespace device

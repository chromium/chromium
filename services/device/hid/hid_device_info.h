// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_DEVICE_INFO_H_
#define SERVICES_DEVICE_HID_HID_DEVICE_INFO_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

#if BUILDFLAG(IS_MAC)
typedef uint64_t HidPlatformDeviceId;
#elif BUILDFLAG(IS_WIN)
typedef std::wstring HidPlatformDeviceId;
#else
typedef std::string HidPlatformDeviceId;
#endif

class HidDeviceInfo : public base::RefCountedThreadSafe<HidDeviceInfo> {
 public:
  // PlatformDeviceIdMap defines a mapping from report IDs to the
  // HidPlatformDeviceId responsible for handling those reports.
  struct PlatformDeviceIdEntry {
    PlatformDeviceIdEntry(base::flat_set<uint8_t> report_ids,
                          HidPlatformDeviceId platform_device_id);
    PlatformDeviceIdEntry(const PlatformDeviceIdEntry& entry);
    PlatformDeviceIdEntry& operator=(const PlatformDeviceIdEntry& entry);
    ~PlatformDeviceIdEntry();

    base::flat_set<uint8_t> report_ids;
    HidPlatformDeviceId platform_device_id;
  };
  using PlatformDeviceIdMap = std::vector<PlatformDeviceIdEntry>;

  HidDeviceInfo(HidPlatformDeviceId platform_device_id,
                const std::string& physical_device_id,
                uint16_t vendor_id,
                uint16_t product_id,
                const std::string& product_name,
                const std::string& serial_number,
                mojom::HidBusType bus_type,
                base::span<const uint8_t> report_descriptor,
                std::string device_node = "");
  HidDeviceInfo(HidPlatformDeviceId platform_device_id,
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
                size_t max_feature_report_size);
  HidDeviceInfo(const HidDeviceInfo& entry) = delete;
  HidDeviceInfo& operator=(const HidDeviceInfo& entry) = delete;

  const mojom::HidDeviceInfoPtr& device() { return device_; }

  // Device identification.
  const std::string& device_guid() const { return device_->guid; }
  const PlatformDeviceIdMap& platform_device_id_map() const {
    return platform_device_id_map_;
  }
  const std::optional<std::string>& interface_id() const {
    return interface_id_;
  }
  const std::string& physical_device_id() const {
    return device_->physical_device_id;
  }
  uint16_t vendor_id() const { return device_->vendor_id; }
  uint16_t product_id() const { return device_->product_id; }
  const std::string& product_name() const { return device_->product_name; }
  const std::string& serial_number() const { return device_->serial_number; }
  mojom::HidBusType bus_type() const { return device_->bus_type; }

  // Top-Level Collections information.
  const std::vector<mojom::HidCollectionInfoPtr>& collections() const {
    return device_->collections;
  }
  bool has_report_id() const { return device_->has_report_id; }
  uint64_t max_input_report_size() const {
    return device_->max_input_report_size;
  }
  uint64_t max_output_report_size() const {
    return device_->max_output_report_size;
  }
  uint64_t max_feature_report_size() const {
    return device_->max_feature_report_size;
  }

  // The raw HID report descriptor is not available on Windows.
  const std::vector<uint8_t>& report_descriptor() const {
    return device_->report_descriptor;
  }
  const std::string& device_node() const { return device_->device_node; }

  // Merge the device information in |device_info| into this object.
  // |device_info| must be part of the same HID interface.
  void AppendDeviceInfo(scoped_refptr<HidDeviceInfo> device_info);

 protected:
  virtual ~HidDeviceInfo();

 private:
  friend class base::RefCountedThreadSafe<HidDeviceInfo>;

  PlatformDeviceIdMap platform_device_id_map_;

  // On platforms where the system enumerates top-level HID collections as
  // separate logical devices, |interface_id_| is an identifier for the HID
  // interface and is used to associate HidDeviceInfo objects generated from
  // the same HID interface. May be std::nullopt if the system does not split
  // top-level collections during enumeration.
  std::optional<std::string> interface_id_;

  mojom::HidDeviceInfoPtr device_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_DEVICE_INFO_H_

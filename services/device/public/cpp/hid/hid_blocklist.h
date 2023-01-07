// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_BLOCKLIST_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_BLOCKLIST_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "services/device/public/mojom/hid.mojom-forward.h"

namespace device {

// Feature used to configure entries in the HID blocklist which can be deployed
// using a server configuration.
BASE_DECLARE_FEATURE(kWebHidBlocklist);

// Dynamic additions to the HID blocklist.
extern const base::FeatureParam<std::string> kWebHidBlocklistAdditions;

class HidBlocklist final {
 public:
  enum ReportType {
    kReportTypeAny = 0,
    kReportTypeInput,
    kReportTypeOutput,
    kReportTypeFeature,
  };

  struct Entry {
    bool has_vendor_id;
    uint16_t vendor_id;

    bool has_product_id;
    uint16_t product_id;

    bool has_usage_page;
    uint16_t usage_page;

    bool has_usage;
    uint16_t usage;

    bool has_report_id;
    uint8_t report_id;

    ReportType report_type;
  };

  HidBlocklist(const HidBlocklist&) = delete;
  HidBlocklist& operator=(const HidBlocklist&) = delete;
  ~HidBlocklist();

  // Returns a singleton instance of the blocklist.
  static HidBlocklist& Get();

  // Returns true if a device is blocked given the |vendor_id| and |product_id|.
  bool IsVendorProductBlocked(uint16_t vendor_id, uint16_t product_id);

  // Returns true if |vendor_id| and |product_id| are blocked by an |entry|.
  static bool IsVendorProductBlockedByEntry(const HidBlocklist::Entry& entry,
                                            uint16_t vendor_id,
                                            uint16_t product_id);

  // Given the |vendor_id|, |product_id|, and |collections| for a HID device,
  // returns a vector of protected report IDs for reports of type |report_type|.
  std::vector<uint8_t> GetProtectedReportIds(
      ReportType report_type,
      uint16_t vendor_id,
      uint16_t product_id,
      const std::vector<mojom::HidCollectionInfoPtr>& collections);

  // Returns the number of dynamic blocklist entries.
  size_t GetDynamicEntryCountForTest() const { return dynamic_entries_.size(); }

  // Reloads the blocklist for testing purposes.
  void ResetToDefaultValuesForTest();

 private:
  // Friend NoDestructor to permit access to the private constructor.
  friend class base::NoDestructor<HidBlocklist>;

  HidBlocklist();

  // Populates the blocklist with values set via a Finch experiment which allows
  // the set of blocked devices to be updated without shipping new executable
  // versions.
  //
  // The variation string must be a comma-separated list of blocklist rules,
  // where each rule is composed of six properties of the form:
  //
  //     vendor_id:product_id:usage_page:usage:report_id:report_type
  //
  // Each property may be empty, indicating that the rule should match any value
  // for that property. When vendor_id, product_id, usage_page, or usage are
  // specified, they must be a 16-bit integer written as exactly 4 hexadecimal
  // digits. When report_id is specified, it must be an 8-bit integer written as
  // exactly 2 hexadecimal digits. When report_type is specified, it must be a
  // single character I, O, or F.
  //
  // Invalid entries in the comma-separated list will be ignored.
  //
  // Example:
  //     "::f1d0:::, 1234:5678::::, abcd:0001:::01:I"
  void PopulateWithServerProvidedValues();

  std::vector<Entry> dynamic_entries_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_BLOCKLIST_H_

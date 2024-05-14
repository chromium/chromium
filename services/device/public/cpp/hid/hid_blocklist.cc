// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_blocklist.h"

#include <string_view>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "components/variations/variations_associated_data.h"
#include "services/device/public/cpp/hid/hid_switches.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

namespace {

#define VENDOR_PRODUCT_RULE(vid, pid)                       \
  {                                                         \
    true, (vid), true, (pid), false, 0, false, 0, false, 0, \
        HidBlocklist::ReportType::kReportTypeAny            \
  }

constexpr HidBlocklist::Entry kStaticEntries[] = {
    // Block all top-level collections with the FIDO usage page.
    {false, 0, false, 0, true, mojom::kPageFido, false, 0, false, 0,
     HidBlocklist::ReportType::kReportTypeAny},

    // KEY-ID
    VENDOR_PRODUCT_RULE(0x096e, 0x0850),
    // Feitian devices
    VENDOR_PRODUCT_RULE(0x096e, 0x0852),
    VENDOR_PRODUCT_RULE(0x096e, 0x0853),
    VENDOR_PRODUCT_RULE(0x096e, 0x0854),
    VENDOR_PRODUCT_RULE(0x096e, 0x0856),
    VENDOR_PRODUCT_RULE(0x096e, 0x0858),
    VENDOR_PRODUCT_RULE(0x096e, 0x085a),
    VENDOR_PRODUCT_RULE(0x096e, 0x085b),
    // HyperFIDO
    VENDOR_PRODUCT_RULE(0x096e, 0x0880),
    // HID Global BlueTrust Token
    VENDOR_PRODUCT_RULE(0x09c3, 0x0023),
    // Yubikey devices
    VENDOR_PRODUCT_RULE(0x1050, 0x0010),
    VENDOR_PRODUCT_RULE(0x1050, 0x0018),
    VENDOR_PRODUCT_RULE(0x1050, 0x0030),
    VENDOR_PRODUCT_RULE(0x1050, 0x0110),
    VENDOR_PRODUCT_RULE(0x1050, 0x0111),
    VENDOR_PRODUCT_RULE(0x1050, 0x0112),
    VENDOR_PRODUCT_RULE(0x1050, 0x0113),
    VENDOR_PRODUCT_RULE(0x1050, 0x0114),
    VENDOR_PRODUCT_RULE(0x1050, 0x0115),
    VENDOR_PRODUCT_RULE(0x1050, 0x0116),
    VENDOR_PRODUCT_RULE(0x1050, 0x0120),
    VENDOR_PRODUCT_RULE(0x1050, 0x0200),
    VENDOR_PRODUCT_RULE(0x1050, 0x0211),
    VENDOR_PRODUCT_RULE(0x1050, 0x0401),
    VENDOR_PRODUCT_RULE(0x1050, 0x0402),
    VENDOR_PRODUCT_RULE(0x1050, 0x0403),
    VENDOR_PRODUCT_RULE(0x1050, 0x0404),
    VENDOR_PRODUCT_RULE(0x1050, 0x0405),
    VENDOR_PRODUCT_RULE(0x1050, 0x0406),
    VENDOR_PRODUCT_RULE(0x1050, 0x0407),
    VENDOR_PRODUCT_RULE(0x1050, 0x0410),
    // U2F Zero
    VENDOR_PRODUCT_RULE(0x10c4, 0x8acf),
    // Mooltipass Mini-BLE
    VENDOR_PRODUCT_RULE(0x1209, 0x4321),
    // Mooltipass Arduino sketch
    VENDOR_PRODUCT_RULE(0x1209, 0x4322),
    // Titan
    VENDOR_PRODUCT_RULE(0x18d1, 0x5026),
    // VASCO
    VENDOR_PRODUCT_RULE(0x1a44, 0x00bb),
    // OnlyKey
    VENDOR_PRODUCT_RULE(0x1d50, 0x60fc),
    // Keydo AES
    VENDOR_PRODUCT_RULE(0x1e0d, 0xf1ae),
    // Neowave Keydo
    VENDOR_PRODUCT_RULE(0x1e0d, 0xf1d0),
    // Thetis
    VENDOR_PRODUCT_RULE(0x1ea8, 0xf025),
    // Nitrokey
    VENDOR_PRODUCT_RULE(0x20a0, 0x4287),
    // JaCarta
    VENDOR_PRODUCT_RULE(0x24dc, 0x0101),
    // Happlink
    VENDOR_PRODUCT_RULE(0x2581, 0xf1d0),
    // Bluink
    VENDOR_PRODUCT_RULE(0x2abe, 0x1002),
    // Feitian USB, HyperFIDO
    VENDOR_PRODUCT_RULE(0x2ccf, 0x0880),

    // Block Jabra access to certain proprietary functionality.
    {true, /*vendorId=*/0x0b0e, false, 0, true, /*usagePage=*/0xff00, false, 0,
     true, /*reportId=*/0x05, HidBlocklist::ReportType::kReportTypeOutput},
};

bool IsValidBlocklistEntry(const HidBlocklist::Entry& entry) {
  // An entry with a product ID parameter must also specify a vendor ID.
  if (!entry.has_vendor_id && entry.has_product_id)
    return false;

  // An entry with a usage ID parameter must also specify a usage page.
  if (!entry.has_usage_page && entry.has_usage)
    return false;

  return true;
}

const std::vector<mojom::HidReportDescriptionPtr>& GetReportsForType(
    HidBlocklist::ReportType report_type,
    const mojom::HidCollectionInfo& collection) {
  switch (report_type) {
    case HidBlocklist::kReportTypeInput:
      return collection.input_reports;
    case HidBlocklist::kReportTypeOutput:
      return collection.output_reports;
    case HidBlocklist::kReportTypeFeature:
      return collection.feature_reports;
    case HidBlocklist::kReportTypeAny:
      NOTREACHED_IN_MIGRATION();
      return collection.input_reports;
  }
}

// Iterates over |collections| to find reports of type |report_type| that should
// be protected according to the blocklist rule |entry|. |vendor_id| and
// |product_id| are the vendor and product IDs of the device with these reports.
// The report IDs of the protected reports are inserted into |protected_ids|.
void CheckBlocklistEntry(
    const HidBlocklist::Entry& entry,
    HidBlocklist::ReportType report_type,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::vector<mojom::HidCollectionInfoPtr>& collections,
    std::set<uint8_t>& protected_ids) {
  DCHECK_NE(report_type, HidBlocklist::kReportTypeAny);
  if (entry.report_type != HidBlocklist::kReportTypeAny &&
      entry.report_type != report_type) {
    return;
  }

  if (entry.has_vendor_id) {
    if (entry.vendor_id != vendor_id)
      return;

    if (entry.has_product_id && entry.product_id != product_id)
      return;
  }

  for (const auto& collection : collections) {
    if (entry.has_usage_page) {
      if (entry.usage_page != collection->usage->usage_page)
        continue;

      if (entry.has_usage && entry.usage != collection->usage->usage)
        continue;
    }

    const auto& reports = GetReportsForType(report_type, *collection);
    for (const auto& report : reports) {
      if (!entry.has_report_id || entry.report_id == report->report_id)
        protected_ids.insert(report->report_id);
    }
  }
}

// Returns true if the passed string is exactly |digits| digits long and only
// contains valid hexadecimal characters (no leading 0x).
bool IsHexComponent(std::string_view string, size_t digits) {
  if (string.length() != digits)
    return false;

  // This is necessary because base::HexStringToUInt allows whitespace and the
  // "0x" prefix in its input.
  for (char c : string) {
    if (c >= '0' && c <= '9')
      continue;
    if (c >= 'a' && c <= 'f')
      continue;
    if (c >= 'A' && c <= 'F')
      continue;
    return false;
  }
  return true;
}

// Returns true if the passed string is "I" (input report), "O" (output report),
// "F" (feature report), or "" (any report type).
bool IsReportTypeComponent(std::string_view string) {
  return string.empty() ||
         (string.length() == 1 &&
          (string[0] == 'I' || string[0] == 'O' || string[0] == 'F'));
}

}  // namespace

BASE_FEATURE(kWebHidBlocklist,
             "WebHIDBlocklist",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kWebHidBlocklistAdditions{
    &kWebHidBlocklist, "blocklist_additions", /*default_value=*/""};

// static
HidBlocklist& HidBlocklist::Get() {
  static base::NoDestructor<HidBlocklist> instance;
  return *instance;
}

bool HidBlocklist::IsVendorProductBlocked(uint16_t vendor_id,
                                          uint16_t product_id) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHidBlocklist)) {
    return false;
  }

  for (const auto& entry : kStaticEntries) {
    if (IsVendorProductBlockedByEntry(entry, vendor_id, product_id))
      return true;
  }
  for (const auto& entry : dynamic_entries_) {
    if (IsVendorProductBlockedByEntry(entry, vendor_id, product_id))
      return true;
  }
  return false;
}

// static
bool HidBlocklist::IsVendorProductBlockedByEntry(
    const HidBlocklist::Entry& entry,
    uint16_t vendor_id,
    uint16_t product_id) {
  // The blocklist `entry` must match on device IDs and nothing else.
  if (!entry.has_vendor_id || entry.has_usage_page || entry.has_report_id ||
      entry.report_type != kReportTypeAny) {
    return false;
  }
  // If `product_id` is specified, it must match.
  if (entry.has_product_id && entry.product_id != product_id)
    return false;
  // `vendor_id` must match.
  return entry.vendor_id == vendor_id;
}

std::vector<uint8_t> HidBlocklist::GetProtectedReportIds(
    HidBlocklist::ReportType report_type,
    uint16_t vendor_id,
    uint16_t product_id,
    const std::vector<mojom::HidCollectionInfoPtr>& collections) {
  DCHECK_NE(report_type, ReportType::kReportTypeAny);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHidBlocklist)) {
    return {};
  }

  std::set<uint8_t> protected_ids;
  for (const auto& entry : kStaticEntries) {
    CheckBlocklistEntry(entry, report_type, vendor_id, product_id, collections,
                        protected_ids);
  }
  for (const auto& entry : dynamic_entries_) {
    CheckBlocklistEntry(entry, report_type, vendor_id, product_id, collections,
                        protected_ids);
  }
  return std::vector<uint8_t>(protected_ids.begin(), protected_ids.end());
}

void HidBlocklist::PopulateWithServerProvidedValues() {
  std::string blocklist_string = kWebHidBlocklistAdditions.Get();
  DLOG(WARNING) << "HID blocklist additions: " << blocklist_string;
  for (const auto& blocklist_rule :
       base::SplitStringPiece(blocklist_string, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> components = base::SplitStringPiece(
        blocklist_rule, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (components.size() != 6) {
      DLOG(WARNING) << "Wrong number of components in HID blocklist rule: "
                    << blocklist_rule;
      continue;
    }

    // The vendor ID, product ID, usage page, and usage must be specified as
    // either an empty string or a 16-bit hexadecimal value.
    if ((!components[0].empty() && !IsHexComponent(components[0], 4)) ||
        (!components[1].empty() && !IsHexComponent(components[1], 4)) ||
        (!components[2].empty() && !IsHexComponent(components[2], 4)) ||
        (!components[3].empty() && !IsHexComponent(components[3], 4))) {
      DLOG(WARNING) << "Bad component format in HID blocklist rule: "
                    << blocklist_rule;
      continue;
    }

    // The report ID must be specified as either an empty string or an 8-bit
    // hexadecimal value.
    if (!components[4].empty() && !IsHexComponent(components[4], 2)) {
      DLOG(WARNING) << "Bad component format in HID blocklist rule: "
                    << blocklist_rule;
      continue;
    }

    // The report type must be specified as either an empty string or a single
    // character 'I', 'O', or 'F'.
    if (!components[5].empty() && !IsReportTypeComponent(components[5])) {
      DLOG(WARNING) << "Bad component format in HID blocklist rule: "
                    << blocklist_rule;
      continue;
    }

    Entry entry = {};
    uint32_t int_value;
    if (!components[0].empty()) {
      base::HexStringToUInt(components[0], &int_value);
      entry.has_vendor_id = true;
      entry.vendor_id = static_cast<uint16_t>(int_value);
    }
    if (!components[1].empty()) {
      base::HexStringToUInt(components[1], &int_value);
      entry.has_product_id = true;
      entry.product_id = static_cast<uint16_t>(int_value);
    }
    if (!components[2].empty()) {
      base::HexStringToUInt(components[2], &int_value);
      entry.has_usage_page = true;
      entry.usage_page = static_cast<uint16_t>(int_value);
    }
    if (!components[3].empty()) {
      base::HexStringToUInt(components[3], &int_value);
      entry.has_usage = true;
      entry.usage = static_cast<uint16_t>(int_value);
    }
    if (!components[4].empty()) {
      base::HexStringToUInt(components[4], &int_value);
      entry.has_report_id = true;
      entry.report_id = static_cast<uint16_t>(int_value);
    }
    if (components[5] == "I")
      entry.report_type = HidBlocklist::kReportTypeInput;
    else if (components[5] == "O")
      entry.report_type = HidBlocklist::kReportTypeOutput;
    else if (components[5] == "F")
      entry.report_type = HidBlocklist::kReportTypeFeature;

    if (!IsValidBlocklistEntry(entry)) {
      DLOG(WARNING) << "Ivalid HID blocklist rule: " << blocklist_rule;
      continue;
    }

    dynamic_entries_.push_back(entry);
  }
}

void HidBlocklist::ResetToDefaultValuesForTest() {
  dynamic_entries_.clear();
  PopulateWithServerProvidedValues();
}

HidBlocklist::HidBlocklist() {
#if DCHECK_IS_ON()
  for (const auto& entry : kStaticEntries)
    DCHECK(IsValidBlocklistEntry(entry));
#endif
}

}  // namespace device

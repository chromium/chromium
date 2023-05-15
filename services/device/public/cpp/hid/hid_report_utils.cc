// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_report_utils.h"

#include "base/containers/contains.h"

namespace device {

namespace {

const std::vector<mojom::HidReportDescriptionPtr>& ReportsForType(
    const mojom::HidCollectionInfo& collection,
    HidReportType report_type) {
  switch (report_type) {
    case HidReportType::kInput:
      return collection.input_reports;
    case HidReportType::kOutput:
      return collection.output_reports;
    case HidReportType::kFeature:
      return collection.feature_reports;
  }
}

}  // namespace

bool IsAlwaysProtected(const mojom::HidUsageAndPage& hid_usage_and_page,
                       HidReportType report_type) {
  const uint16_t usage = hid_usage_and_page.usage;
  const uint16_t usage_page = hid_usage_and_page.usage_page;

  if (usage_page == mojom::kPageKeyboard) {
    return true;
  }

  if (usage_page != mojom::kPageGenericDesktop) {
    return false;
  }

  if (usage == mojom::kGenericDesktopPointer ||
      usage == mojom::kGenericDesktopMouse ||
      usage == mojom::kGenericDesktopKeyboard ||
      usage == mojom::kGenericDesktopKeypad) {
    return report_type != HidReportType::kFeature;
  }

  if (usage >= mojom::kGenericDesktopSystemControl &&
      usage <= mojom::kGenericDesktopSystemWarmRestart) {
    return true;
  }

  if (usage >= mojom::kGenericDesktopSystemDock &&
      usage <= mojom::kGenericDesktopSystemDisplaySwap) {
    return true;
  }

  return false;
}

bool CollectionHasUnprotectedReports(
    const mojom::HidCollectionInfo& collection) {
  const HidReportType report_types[] = {
      HidReportType::kInput, HidReportType::kOutput, HidReportType::kFeature};
  return base::ranges::any_of(report_types, [&collection](auto report_type) {
    return !ReportsForType(collection, report_type).empty() &&
           !IsAlwaysProtected(*collection.usage, report_type);
  });
}

const mojom::HidCollectionInfo* FindCollectionWithReport(
    const mojom::HidDeviceInfo& device,
    uint8_t report_id,
    HidReportType report_type) {
  if (!device.has_report_id) {
    // `report_id` must be zero if the device does not use numbered reports.
    if (report_id != 0) {
      return nullptr;
    }

    // Return the first collection with a report of type `report_type`, or
    // nullptr if there is no report of that type.
    auto find_it = base::ranges::find_if(
        device.collections, [report_type](const auto& collection) {
          return !ReportsForType(*collection, report_type).empty();
        });
    if (find_it == device.collections.end()) {
      return nullptr;
    }

    CHECK(find_it->get());
    return find_it->get();
  }

  // `report_id` must be non-zero if the device uses numbered reports.
  if (report_id == 0) {
    return nullptr;
  }

  // Return the collection containing a report with `report_id` and type
  // `report_type`, or nullptr if it is not in any collection.
  auto find_it = base::ranges::find_if(
      device.collections, [report_id, report_type](const auto& collection) {
        return base::Contains(ReportsForType(*collection, report_type),
                              report_id,
                              &mojom::HidReportDescription::report_id);
      });
  if (find_it == device.collections.end()) {
    return nullptr;
  }

  CHECK(find_it->get());
  return find_it->get();
}

}  // namespace device

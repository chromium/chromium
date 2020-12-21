// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_win.h"

#define INITGUID

#include <dbt.h>
#include <devpkey.h>
#include <setupapi.h>
#include <stddef.h>
#include <wdmguid.h>
#include <winioctl.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/memory/free_deleter.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_devinfo.h"
#include "base/win/win_util.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/hid/hid_connection_win.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/hid/hid_preparsed_data.h"

namespace device {

namespace {

// Flags for the BitField member of HIDP_BUTTON_CAPS and HIDP_VALUE_CAPS. This
// bitfield is defined in the Device Class Definition for HID v1.11 section
// 6.2.2.5.
// https://www.usb.org/document-library/device-class-definition-hid-111
constexpr uint16_t kBitFieldFlagConstant = 1 << 0;
constexpr uint16_t kBitFieldFlagVariable = 1 << 1;
constexpr uint16_t kBitFieldFlagRelative = 1 << 2;
constexpr uint16_t kBitFieldFlagWrap = 1 << 3;
constexpr uint16_t kBitFieldFlagNonLinear = 1 << 4;
constexpr uint16_t kBitFieldFlagNoPreferredState = 1 << 5;
constexpr uint16_t kBitFieldFlagHasNullPosition = 1 << 6;
constexpr uint16_t kBitFieldFlagVolatile = 1 << 7;
constexpr uint16_t kBitFieldFlagBufferedBytes = 1 << 8;

// Unpacks |bit_field| into the corresponding members of |item|.
void UnpackBitField(uint16_t bit_field, mojom::HidReportItem* item) {
  item->is_constant = bit_field & kBitFieldFlagConstant;
  item->is_variable = bit_field & kBitFieldFlagVariable;
  item->is_relative = bit_field & kBitFieldFlagRelative;
  item->wrap = bit_field & kBitFieldFlagWrap;
  item->is_non_linear = bit_field & kBitFieldFlagNonLinear;
  item->no_preferred_state = bit_field & kBitFieldFlagNoPreferredState;
  item->has_null_position = bit_field & kBitFieldFlagHasNullPosition;
  item->is_volatile = bit_field & kBitFieldFlagVolatile;
  item->is_buffered_bytes = bit_field & kBitFieldFlagBufferedBytes;
}

// Looks up the value of a GUID-type device property specified by |property| for
// the device described by |device_info_data|. On success, returns true and sets
// |property_buffer| to the property value. Returns false if the property is not
// present or has a different type.
bool GetDeviceGuidProperty(HDEVINFO device_info_set,
                           SP_DEVINFO_DATA& device_info_data,
                           const DEVPROPKEY& property,
                           GUID* property_buffer) {
  DEVPROPTYPE property_type;
  if (!SetupDiGetDeviceProperty(
          device_info_set, &device_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(property_buffer), sizeof(*property_buffer),
          /*RequiredSize=*/nullptr, /*Flags=*/0) ||
      property_type != DEVPROP_TYPE_GUID) {
    return false;
  }
  return true;
}

// Looks up information about the device described by |device_interface_data|
// in |device_info_set|. On success, returns true and sets |device_info_data|
// and |device_path|. Returns false if an error occurred.
bool GetDeviceInfoAndPathFromInterface(
    HDEVINFO device_info_set,
    SP_DEVICE_INTERFACE_DATA& device_interface_data,
    SP_DEVINFO_DATA* device_info_data,
    std::wstring* device_path) {
  // Get the required buffer size. When called with
  // DeviceInterfaceDetailData == nullptr and DeviceInterfaceDetailSize == 0,
  // SetupDiGetDeviceInterfaceDetail returns the required buffer size at
  // RequiredSize and fails with GetLastError() == ERROR_INSUFFICIENT_BUFFER.
  DWORD required_size;
  if (SetupDiGetDeviceInterfaceDetail(device_info_set, &device_interface_data,
                                      /*DeviceInterfaceDetailData=*/nullptr,
                                      /*DeviceInterfaceDetailSize=*/0,
                                      &required_size,
                                      /*DeviceInfoData=*/nullptr) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return false;
  }

  std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
      device_interface_detail_data(
          static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(required_size)));
  device_interface_detail_data->cbSize = sizeof(*device_interface_detail_data);

  // Call the function again with the correct buffer size to get the detailed
  // data for this device.
  if (!SetupDiGetDeviceInterfaceDetail(device_info_set, &device_interface_data,
                                       device_interface_detail_data.get(),
                                       required_size, /*RequiredSize=*/nullptr,
                                       device_info_data)) {
    return false;
  }

  // Windows uses case-insensitive paths and may return paths that differ only
  // by case. Canonicalize the device path by converting to lowercase.
  std::wstring path = device_interface_detail_data->DevicePath;
  DCHECK(base::IsStringASCII(path));
  *device_path = base::ToLowerASCII(path);
  return true;
}

// Returns a device info set containing only the device described by
// |device_path|, or an invalid ScopedDevInfo if there was an error while
// creating the device set. The device info is returned in |device_info_data|.
base::win::ScopedDevInfo GetDeviceInfoFromPath(
    const std::wstring& device_path,
    SP_DEVINFO_DATA* device_info_data) {
  base::win::ScopedDevInfo device_info_set(SetupDiGetClassDevs(
      &GUID_DEVINTERFACE_HID, /*Enumerator=*/nullptr,
      /*hwndParent=*/0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
  if (!device_info_set.is_valid())
    return base::win::ScopedDevInfo();

  SP_DEVICE_INTERFACE_DATA device_interface_data;
  device_interface_data.cbSize = sizeof(device_interface_data);
  if (!SetupDiOpenDeviceInterface(device_info_set.get(), device_path.c_str(),
                                  /*OpenFlags=*/0, &device_interface_data)) {
    return base::win::ScopedDevInfo();
  }

  std::wstring intf_device_path;
  GetDeviceInfoAndPathFromInterface(device_info_set.get(),
                                    device_interface_data, device_info_data,
                                    &intf_device_path);
  DCHECK_EQ(intf_device_path, device_path);
  return device_info_set;
}

mojom::HidReportItemPtr CreateHidReportItem(
    const HidServiceWin::PreparsedData::ReportItem& item) {
  auto hid_report_item = mojom::HidReportItem::New();
  UnpackBitField(item.bit_field, hid_report_item.get());
  if (item.usage_minimum == item.usage_maximum) {
    hid_report_item->is_range = false;
    hid_report_item->usages.push_back(
        mojom::HidUsageAndPage::New(item.usage_minimum, item.usage_page));
    hid_report_item->usage_minimum = mojom::HidUsageAndPage::New(0, 0);
    hid_report_item->usage_maximum = mojom::HidUsageAndPage::New(0, 0);
  } else {
    hid_report_item->is_range = true;
    hid_report_item->usage_minimum =
        mojom::HidUsageAndPage::New(item.usage_minimum, item.usage_page);
    hid_report_item->usage_maximum =
        mojom::HidUsageAndPage::New(item.usage_maximum, item.usage_page);
  }
  hid_report_item->designator_minimum = item.designator_minimum;
  hid_report_item->designator_maximum = item.designator_maximum;
  hid_report_item->string_minimum = item.string_minimum;
  hid_report_item->string_maximum = item.string_maximum;
  hid_report_item->logical_minimum = item.logical_minimum;
  hid_report_item->logical_maximum = item.logical_maximum;
  hid_report_item->physical_minimum = item.physical_minimum;
  hid_report_item->physical_maximum = item.physical_maximum;
  hid_report_item->unit_exponent = item.unit_exponent;
  hid_report_item->unit = item.unit;
  hid_report_item->report_size = item.report_size;
  hid_report_item->report_count = item.report_count;
  return hid_report_item;
}

// Returns a mojom::HidReportItemPtr representing a constant (zero) field within
// a report. |bit_size| is the bit width of the constant field.
mojom::HidReportItemPtr CreateConstHidReportItem(uint16_t bit_size) {
  auto hid_report_item = mojom::HidReportItem::New();
  hid_report_item->is_constant = true;
  hid_report_item->report_count = 1;
  hid_report_item->report_size = bit_size;
  hid_report_item->usage_minimum = mojom::HidUsageAndPage::New(0, 0);
  hid_report_item->usage_maximum = mojom::HidUsageAndPage::New(0, 0);
  return hid_report_item;
}

// Returns a vector of mojom::HidReportDescriptionPtr constructed from the
// information about the top-level collection described by |preparsed_data|.
// The returned vector contains information about all reports of type
// |report_type|.
std::vector<mojom::HidReportDescriptionPtr> CreateReportDescriptions(
    const HidServiceWin::PreparsedData& preparsed_data,
    HIDP_REPORT_TYPE report_type) {
  auto report_items = preparsed_data.GetReportItems(report_type);

  // Sort items by |report_id| and |bit_index|.
  base::ranges::sort(report_items, [](const auto& a, const auto& b) {
    if (a.report_id < b.report_id)
      return true;
    if (a.report_id == b.report_id)
      return a.bit_index < b.bit_index;
    return false;
  });

  std::vector<mojom::HidReportDescriptionPtr> reports;
  mojom::HidReportDescription* current_report = nullptr;
  mojom::HidReportItem* current_item = nullptr;
  size_t current_bit_index = 0;
  size_t next_bit_index = 0;
  for (const auto& item : report_items) {
    if (!current_report || current_report->report_id != item.report_id) {
      reports.push_back(mojom::HidReportDescription::New());
      current_report = reports.back().get();
      current_report->report_id = item.report_id;
      current_item = nullptr;
      current_bit_index = 0;
      next_bit_index = 0;
    }
    // If |item| occupies the same bit index as |current_item| then they must be
    // merged into a single HidReportItem. This can occur when a report item is
    // defined with a list of usages instead of a usage range.
    if (current_item && current_bit_index == item.bit_index) {
      // Usage ranges cannot be merged into a single item. Ensure that both
      // |item| and |current_item| are single-usage items. If either has a usage
      // range, omit |item| from the report.
      if (!current_item->is_range && item.usage_minimum == item.usage_maximum) {
        current_item->usages.push_back(
            mojom::HidUsageAndPage::New(item.usage_minimum, item.usage_page));
      }
      continue;
    }
    // If there is a gap between the last bit of |current_item| and the first
    // bit of |item|, insert a constant item for padding.
    if (next_bit_index < item.bit_index) {
      size_t pad_bits = item.bit_index - next_bit_index;
      current_report->items.push_back(CreateConstHidReportItem(pad_bits));
    }
    current_report->items.push_back(CreateHidReportItem(item));
    current_item = current_report->items.back().get();
    current_bit_index = item.bit_index;
    next_bit_index = item.bit_index + item.report_size * item.report_count;
  }

  // Compute the size of each report and, if needed, add a final constant item
  // to pad the report to the expected report byte length.
  const size_t report_byte_length =
      preparsed_data.GetReportByteLength(report_type);
  for (auto& report : reports) {
    size_t bit_length = 0;
    for (auto& item : report->items)
      bit_length += item->report_size * item->report_count;
    DCHECK_LE(bit_length, report_byte_length * CHAR_BIT);
    size_t pad_bits = report_byte_length * CHAR_BIT - bit_length;
    if (pad_bits > 0)
      report->items.push_back(CreateConstHidReportItem(pad_bits));
  }

  return reports;
}

}  // namespace

mojom::HidCollectionInfoPtr
HidServiceWin::PreparsedData::CreateHidCollectionInfo() const {
  const HIDP_CAPS& caps = GetCaps();
  auto collection_info = mojom::HidCollectionInfo::New();
  collection_info->usage =
      mojom::HidUsageAndPage::New(caps.Usage, caps.UsagePage);
  collection_info->input_reports = CreateReportDescriptions(*this, HidP_Input);
  collection_info->output_reports =
      CreateReportDescriptions(*this, HidP_Output);
  collection_info->feature_reports =
      CreateReportDescriptions(*this, HidP_Feature);

  // Collect and de-duplicate report IDs.
  std::set<uint8_t> report_ids;
  for (const auto& report : collection_info->input_reports) {
    if (report->report_id)
      report_ids.insert(report->report_id);
  }
  for (const auto& report : collection_info->output_reports) {
    if (report->report_id)
      report_ids.insert(report->report_id);
  }
  for (const auto& report : collection_info->feature_reports) {
    if (report->report_id)
      report_ids.insert(report->report_id);
  }
  collection_info->report_ids.insert(collection_info->report_ids.end(),
                                     report_ids.begin(), report_ids.end());

  return collection_info;
}

uint16_t HidServiceWin::PreparsedData::GetReportByteLength(
    HIDP_REPORT_TYPE report_type) const {
  uint16_t report_length = 0;
  switch (report_type) {
    case HidP_Input:
      report_length = GetCaps().InputReportByteLength;
      break;
    case HidP_Output:
      report_length = GetCaps().OutputReportByteLength;
      break;
    case HidP_Feature:
      report_length = GetCaps().FeatureReportByteLength;
      break;
    default:
      NOTREACHED();
      break;
  }
  // Whether or not the device includes report IDs in its reports the size
  // of the report ID is included in the value provided by Windows. This
  // appears contrary to the MSDN documentation.
  if (report_length)
    return report_length - 1;

  return 0;
}

HidServiceWin::HidServiceWin()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      blocking_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kBlockingTaskTraits)),
      device_observer_(this) {
  DeviceMonitorWin* device_monitor =
      DeviceMonitorWin::GetForDeviceInterface(GUID_DEVINTERFACE_HID);
  if (device_monitor)
    device_observer_.Add(device_monitor);

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HidServiceWin::EnumerateBlocking,
                                weak_factory_.GetWeakPtr(), task_runner_));
}

HidServiceWin::~HidServiceWin() = default;

void HidServiceWin::Connect(const std::string& device_guid,
                            bool allow_protected_reports,
                            ConnectCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto& map_entry = devices().find(device_guid);
  if (map_entry == devices().end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfo> device_info = map_entry->second;

  base::win::ScopedHandle file(OpenDevice(device_info->platform_device_id()));
  if (!file.IsValid()) {
    HID_PLOG(EVENT) << "Failed to open device";
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), nullptr));
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     HidConnectionWin::Create(device_info, std::move(file),
                                              allow_protected_reports)));
}

base::WeakPtr<HidService> HidServiceWin::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
void HidServiceWin::EnumerateBlocking(
    base::WeakPtr<HidServiceWin> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::win::ScopedDevInfo dev_info(SetupDiGetClassDevs(
      &GUID_DEVINTERFACE_HID, /*Enumerator=*/nullptr,
      /*hwndParent=*/nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));

  if (dev_info.is_valid()) {
    SP_DEVICE_INTERFACE_DATA device_interface_data = {0};
    device_interface_data.cbSize = sizeof(device_interface_data);

    for (int device_index = 0; SetupDiEnumDeviceInterfaces(
             dev_info.get(), /*DeviceInfoData=*/nullptr, &GUID_DEVINTERFACE_HID,
             device_index, &device_interface_data);
         ++device_index) {
      SP_DEVINFO_DATA dev_info_data = {0};
      dev_info_data.cbSize = sizeof(dev_info_data);
      std::wstring device_path;
      if (!GetDeviceInfoAndPathFromInterface(dev_info.get(),
                                             device_interface_data,
                                             &dev_info_data, &device_path)) {
        continue;
      }

      // Get the container ID for the physical device.
      GUID container_id;
      if (!GetDeviceGuidProperty(dev_info.get(), dev_info_data,
                                 DEVPKEY_Device_ContainerId, &container_id)) {
        continue;
      }
      std::string physical_device_id =
          base::WideToUTF8(base::win::WStringFromGUID(container_id));

      AddDeviceBlocking(service, task_runner, device_path, physical_device_id);
    }
  }

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HidServiceWin::FirstEnumerationComplete, service));
}

// static
void HidServiceWin::AddDeviceBlocking(
    base::WeakPtr<HidServiceWin> service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::wstring& device_path,
    const std::string& physical_device_id) {
  base::win::ScopedHandle device_handle(OpenDevice(device_path));
  if (!device_handle.IsValid()) {
    return;
  }

  HIDD_ATTRIBUTES attrib = {0};
  attrib.Size = sizeof(attrib);
  if (!HidD_GetAttributes(device_handle.Get(), &attrib)) {
    HID_LOG(EVENT) << "Failed to get device attributes.";
    return;
  }

  auto preparsed_data = HidPreparsedData::Create(device_handle.Get());
  if (!preparsed_data)
    return;

  // 1023 characters plus NULL terminator is more than enough for a USB string
  // descriptor which is limited to 126 characters.
  base::char16 buffer[1024];
  std::string product_name;
  if (HidD_GetProductString(device_handle.Get(), &buffer[0], sizeof(buffer))) {
    // NULL termination guaranteed by the API.
    product_name = base::UTF16ToUTF8(buffer);
  }
  std::string serial_number;
  if (HidD_GetSerialNumberString(device_handle.Get(), &buffer[0],
                                 sizeof(buffer))) {
    // NULL termination guaranteed by the API.
    serial_number = base::UTF16ToUTF8(buffer);
  }

  // This populates the HidDeviceInfo instance without a raw report descriptor.
  // The descriptor is unavailable on Windows because HID devices are exposed to
  // user-space as individual top-level collections.
  scoped_refptr<HidDeviceInfo> device_info(
      new HidDeviceInfo(device_path, physical_device_id, attrib.VendorID,
                        attrib.ProductID, product_name, serial_number,
                        // TODO(crbug.com/443335): Detect Bluetooth.
                        mojom::HidBusType::kHIDBusTypeUSB,
                        preparsed_data->CreateHidCollectionInfo(),
                        preparsed_data->GetReportByteLength(HidP_Input),
                        preparsed_data->GetReportByteLength(HidP_Output),
                        preparsed_data->GetReportByteLength(HidP_Feature)));

  task_runner->PostTask(FROM_HERE, base::BindOnce(&HidServiceWin::AddDevice,
                                                  service, device_info));
}

void HidServiceWin::OnDeviceAdded(const GUID& class_guid,
                                  const std::wstring& device_path) {
  SP_DEVINFO_DATA device_info_data = {0};
  device_info_data.cbSize = sizeof(device_info_data);
  auto device_info_set = GetDeviceInfoFromPath(device_path, &device_info_data);
  if (!device_info_set.is_valid())
    return;

  GUID container_id;
  if (!GetDeviceGuidProperty(device_info_set.get(), device_info_data,
                             DEVPKEY_Device_ContainerId, &container_id)) {
    return;
  }
  std::string physical_device_id =
      base::WideToUTF8(base::win::WStringFromGUID(container_id));

  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&HidServiceWin::AddDeviceBlocking,
                                weak_factory_.GetWeakPtr(), task_runner_,
                                device_path, physical_device_id));
}

void HidServiceWin::OnDeviceRemoved(const GUID& class_guid,
                                    const std::wstring& device_path) {
  // Execute a no-op closure on the file task runner to synchronize with any
  // devices that are still being enumerated.
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&HidServiceWin::RemoveDevice, weak_factory_.GetWeakPtr(),
                     device_path));
}

// static
base::win::ScopedHandle HidServiceWin::OpenDevice(
    const std::wstring& device_path) {
  base::win::ScopedHandle file(
      CreateFile(device_path.c_str(), GENERIC_WRITE | GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                 FILE_FLAG_OVERLAPPED, nullptr));
  if (!file.IsValid() && GetLastError() == ERROR_ACCESS_DENIED) {
    file.Set(CreateFile(device_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                        nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
  }
  return file;
}

}  // namespace device

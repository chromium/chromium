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

#include <memory>
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

namespace device {

namespace {

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

}  // namespace

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

HidServiceWin::~HidServiceWin() {}

void HidServiceWin::Connect(const std::string& device_guid,
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
                     HidConnectionWin::Create(device_info, std::move(file))));
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
void HidServiceWin::CollectInfoFromButtonCaps(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_REPORT_TYPE report_type,
    USHORT button_caps_length,
    mojom::HidCollectionInfo* collection_info) {
  if (button_caps_length > 0) {
    std::unique_ptr<HIDP_BUTTON_CAPS[]> button_caps(
        new HIDP_BUTTON_CAPS[button_caps_length]);
    if (HidP_GetButtonCaps(report_type, &button_caps[0], &button_caps_length,
                           preparsed_data) == HIDP_STATUS_SUCCESS) {
      for (size_t i = 0; i < button_caps_length; i++) {
        int report_id = button_caps[i].ReportID;
        if (report_id != 0) {
          collection_info->report_ids.push_back(report_id);
        }
      }
    }
  }
}

// static
void HidServiceWin::CollectInfoFromValueCaps(
    PHIDP_PREPARSED_DATA preparsed_data,
    HIDP_REPORT_TYPE report_type,
    USHORT value_caps_length,
    mojom::HidCollectionInfo* collection_info) {
  if (value_caps_length > 0) {
    std::unique_ptr<HIDP_VALUE_CAPS[]> value_caps(
        new HIDP_VALUE_CAPS[value_caps_length]);
    if (HidP_GetValueCaps(report_type, &value_caps[0], &value_caps_length,
                          preparsed_data) == HIDP_STATUS_SUCCESS) {
      for (size_t i = 0; i < value_caps_length; i++) {
        int report_id = value_caps[i].ReportID;
        if (report_id != 0) {
          collection_info->report_ids.push_back(report_id);
        }
      }
    }
  }
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

  PHIDP_PREPARSED_DATA preparsed_data = nullptr;
  if (!HidD_GetPreparsedData(device_handle.Get(), &preparsed_data) ||
      !preparsed_data) {
    HID_LOG(EVENT) << "Failed to get device data.";
    return;
  }

  HIDP_CAPS capabilities = {0};
  if (HidP_GetCaps(preparsed_data, &capabilities) != HIDP_STATUS_SUCCESS) {
    HID_LOG(EVENT) << "Failed to get device capabilities.";
    HidD_FreePreparsedData(preparsed_data);
    return;
  }

  // Whether or not the device includes report IDs in its reports the size
  // of the report ID is included in the value provided by Windows. This
  // appears contrary to the MSDN documentation.
  size_t max_input_report_size = 0;
  size_t max_output_report_size = 0;
  size_t max_feature_report_size = 0;
  if (capabilities.InputReportByteLength > 0) {
    max_input_report_size = capabilities.InputReportByteLength - 1;
  }
  if (capabilities.OutputReportByteLength > 0) {
    max_output_report_size = capabilities.OutputReportByteLength - 1;
  }
  if (capabilities.FeatureReportByteLength > 0) {
    max_feature_report_size = capabilities.FeatureReportByteLength - 1;
  }

  auto collection_info = mojom::HidCollectionInfo::New();
  collection_info->usage =
      mojom::HidUsageAndPage::New(capabilities.Usage, capabilities.UsagePage);
  CollectInfoFromButtonCaps(preparsed_data, HidP_Input,
                            capabilities.NumberInputButtonCaps,
                            collection_info.get());
  CollectInfoFromButtonCaps(preparsed_data, HidP_Output,
                            capabilities.NumberOutputButtonCaps,
                            collection_info.get());
  CollectInfoFromButtonCaps(preparsed_data, HidP_Feature,
                            capabilities.NumberFeatureButtonCaps,
                            collection_info.get());
  CollectInfoFromValueCaps(preparsed_data, HidP_Input,
                           capabilities.NumberInputValueCaps,
                           collection_info.get());
  CollectInfoFromValueCaps(preparsed_data, HidP_Output,
                           capabilities.NumberOutputValueCaps,
                           collection_info.get());
  CollectInfoFromValueCaps(preparsed_data, HidP_Feature,
                           capabilities.NumberFeatureValueCaps,
                           collection_info.get());

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
  scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfo(
      device_path, physical_device_id, attrib.VendorID, attrib.ProductID,
      product_name, serial_number,
      // TODO(reillyg): Detect Bluetooth. crbug.com/443335
      mojom::HidBusType::kHIDBusTypeUSB, std::move(collection_info),
      max_input_report_size, max_output_report_size, max_feature_report_size));

  HidD_FreePreparsedData(preparsed_data);
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

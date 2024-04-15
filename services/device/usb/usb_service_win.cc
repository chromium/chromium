// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_service_win.h"

#include <windows.h>

#include <string_view>

#define INITGUID

#include <objbase.h>

#include <devpkey.h>
#include <setupapi.h>
#include <stdint.h>
#include <usbiodef.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_generic.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/registry.h"
#include "base/win/scoped_devinfo.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/webusb_descriptors.h"
#include "third_party/re2/src/re2/re2.h"

namespace device {

namespace {

bool IsCompositeDevice(const std::wstring& service_name) {
  // Windows built-in composite device driver
  return base::EqualsCaseInsensitiveASCII(service_name, L"usbccgp") ||
         // Samsung Mobile USB Composite device driver
         base::EqualsCaseInsensitiveASCII(service_name, L"dg_ssudbus");
}

std::ostream& operator<<(std::ostream& os, const DEVPROPKEY& value) {
  os << "{" << base::win::WStringFromGUID(value.fmtid) << ", " << value.pid
     << "}";
  return os;
}

std::optional<uint32_t> GetDeviceUint32Property(HDEVINFO dev_info,
                                                SP_DEVINFO_DATA* dev_info_data,
                                                const DEVPROPKEY& property) {
  // SetupDiGetDeviceProperty() makes an RPC which may block.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DEVPROPTYPE property_type;
  uint32_t buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(&buffer), sizeof(buffer), nullptr, 0)) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_UINT32) {
    USB_LOG(ERROR) << "SetupDiGetDeviceProperty(" << property
                   << ") returned unexpected type (" << property_type
                   << " != " << DEVPROP_TYPE_UINT32 << ")";
    return std::nullopt;
  }

  return buffer;
}

std::optional<std::wstring> GetDeviceStringProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property) {
  // SetupDiGetDeviceProperty() makes an RPC which may block.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, nullptr, 0, &required_size, 0)) {
    USB_LOG(ERROR) << "SetupDiGetDeviceProperty(" << property
                   << ") unexpectedly succeeded";
    return std::nullopt;
  }

  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_STRING) {
    USB_LOG(ERROR) << "SetupDiGetDeviceProperty(" << property
                   << ") returned unexpected type (" << property_type
                   << " != " << DEVPROP_TYPE_STRING << ")";
    return std::nullopt;
  }

  std::wstring buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, nullptr, 0)) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  return buffer;
}

std::optional<std::vector<std::wstring>> GetDeviceStringListProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property) {
  // SetupDiGetDeviceProperty() makes an RPC which may block.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, nullptr, 0, &required_size, 0)) {
    USB_LOG(ERROR) << "SetupDiGetDeviceProperty(" << property
                   << ") unexpectedly succeeded";
    return std::nullopt;
  }

  if (GetLastError() == ERROR_NOT_FOUND) {
    // Simplify callers by returning empty list when the property isn't found.
    return std::vector<std::wstring>();
  }

  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_STRING_LIST) {
    USB_LOG(ERROR) << "SetupDiGetDeviceProperty(" << property
                   << ") returned unexpected type (" << property_type
                   << " != " << DEVPROP_TYPE_STRING_LIST << ")";
    return std::nullopt;
  }

  std::wstring buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, nullptr, 0)) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  // Windows string list properties use a NUL character as the delimiter.
  return base::SplitString(buffer, std::wstring_view(L"\0", 1),
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

std::wstring GetServiceName(HDEVINFO dev_info, SP_DEVINFO_DATA* dev_info_data) {
  std::optional<std::wstring> property =
      GetDeviceStringProperty(dev_info, dev_info_data, DEVPKEY_Device_Service);
  if (!property.has_value())
    return std::wstring();

  // Windows pads this string with a variable number of NUL bytes for no
  // discernible reason.
  return std::wstring(base::TrimString(*property, std::wstring_view(L"\0", 1),
                                       base::TRIM_TRAILING));
}

bool GetDeviceInterfaceDetails(HDEVINFO dev_info,
                               SP_DEVICE_INTERFACE_DATA* device_interface_data,
                               std::wstring* device_path,
                               uint32_t* bus_number,
                               uint32_t* port_number,
                               std::wstring* instance_id,
                               std::wstring* parent_instance_id,
                               std::vector<std::wstring>* child_instance_ids,
                               std::vector<std::wstring>* hardware_ids,
                               std::wstring* service_name) {
  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);

  DWORD required_size = 0;
  std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA, base::FreeDeleter>
      device_interface_detail_data;

  // Probing for the required size of the SP_DEVICE_INTERFACE_DETAIL_DATA
  // struct is only required if we are looking for the device path.
  // Otherwise all the necessary data can be queried from the SP_DEVINFO_DATA.
  if (device_path) {
    if (!SetupDiGetDeviceInterfaceDetail(dev_info, device_interface_data,
                                         /*DeviceInterfaceDetailData=*/nullptr,
                                         /*DeviceInterfaceDetailDataSize=*/0,
                                         &required_size,
                                         /*DeviceInfoData=*/nullptr) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      return false;
    }

    device_interface_detail_data.reset(
        static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(malloc(required_size)));
    device_interface_detail_data->cbSize =
        sizeof(*device_interface_detail_data);
  }

  if (!SetupDiGetDeviceInterfaceDetail(
          dev_info, device_interface_data, device_interface_detail_data.get(),
          required_size, /*RequiredSize=*/nullptr, &dev_info_data) &&
      (device_path || GetLastError() != ERROR_INSUFFICIENT_BUFFER)) {
    USB_PLOG(ERROR) << "SetupDiGetDeviceInterfaceDetail";
    return false;
  }

  if (device_path)
    *device_path = std::wstring(device_interface_detail_data->DevicePath);

  if (bus_number) {
    auto result = GetDeviceUint32Property(dev_info, &dev_info_data,
                                          DEVPKEY_Device_BusNumber);
    if (!result.has_value())
      return false;
    *bus_number = result.value();
  }

  if (port_number) {
    auto result = GetDeviceUint32Property(dev_info, &dev_info_data,
                                          DEVPKEY_Device_Address);
    if (!result.has_value())
      return false;
    *port_number = result.value();
  }

  if (instance_id) {
    auto result = GetDeviceStringProperty(dev_info, &dev_info_data,
                                          DEVPKEY_Device_InstanceId);
    if (!result.has_value())
      return false;
    *instance_id = std::move(result.value());
  }

  if (parent_instance_id) {
    auto result = GetDeviceStringProperty(dev_info, &dev_info_data,
                                          DEVPKEY_Device_Parent);
    if (!result.has_value())
      return false;
    *parent_instance_id = std::move(result.value());
  }

  if (child_instance_ids) {
    auto result = GetDeviceStringListProperty(dev_info, &dev_info_data,
                                              DEVPKEY_Device_Children);
    if (!result.has_value())
      return false;
    *child_instance_ids = std::move(result.value());
  }

  if (hardware_ids) {
    auto result = GetDeviceStringListProperty(dev_info, &dev_info_data,
                                              DEVPKEY_Device_HardwareIds);
    if (!result.has_value())
      return false;
    *hardware_ids = std::move(result.value());
  }

  if (service_name) {
    *service_name = GetServiceName(dev_info, &dev_info_data);
    if (service_name->empty()) {
      return false;
    }
  }

  return true;
}

std::wstring GetDevicePath(const std::wstring& instance_id,
                           const GUID& device_interface_guid) {
  base::win::ScopedDevInfo dev_info(
      SetupDiGetClassDevs(&device_interface_guid, instance_id.c_str(), 0,
                          DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
  if (!dev_info.is_valid()) {
    USB_PLOG(ERROR) << "SetupDiGetClassDevs";
    return std::wstring();
  }

  SP_DEVICE_INTERFACE_DATA device_interface_data = {};
  device_interface_data.cbSize = sizeof(device_interface_data);
  if (!SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                   &device_interface_guid, 0,
                                   &device_interface_data)) {
    USB_PLOG(ERROR) << "SetupDiEnumDeviceInterfaces";
    return std::wstring();
  }

  std::wstring device_path;
  if (!GetDeviceInterfaceDetails(
          dev_info.get(), &device_interface_data, &device_path,
          /*bus_number=*/nullptr, /*port_number=*/nullptr,
          /*instance_id=*/nullptr, /*parent_instance_id=*/nullptr,
          /*child_instance_ids=*/nullptr, /*hardware_ids=*/nullptr,
          /*service_name=*/nullptr)) {
    return std::wstring();
  }

  return device_path;
}

int GetInterfaceNumber(const std::wstring& instance_id,
                       const std::vector<std::wstring>& hardware_ids) {
  // According to MSDN the instance IDs for the device nodes created by the
  // composite driver is in the form "USB\VID_vvvv&PID_dddd&MI_zz" where "zz"
  // is the interface number.
  //
  // https://docs.microsoft.com/en-us/windows-hardware/drivers/install/standard-usb-identifiers#multiple-interface-usb-devices
  RE2 pattern("MI_([0-9a-fA-F]{2})");

  std::string instance_id_ascii = base::WideToASCII(instance_id);
  std::string match;
  if (!RE2::PartialMatch(instance_id_ascii, pattern, &match)) {
    // Alternative composite drivers, such as the one used for Samsung devices,
    // don't use the standard format for the instance ID, but one of the
    // hardware IDs will still match the expected pattern.
    bool found = false;
    for (const std::wstring& hardware_id : hardware_ids) {
      std::string hardware_id_ascii = base::WideToASCII(hardware_id);
      if (RE2::PartialMatch(hardware_id_ascii, pattern, &match)) {
        found = true;
        break;
      }
    }
    if (!found)
      return -1;
  }

  int interface_number;
  if (!base::HexStringToInt(match, &interface_number))
    return -1;
  return interface_number;
}

UsbDeviceWin::FunctionInfo GetFunctionInfo(const std::wstring& instance_id) {
  UsbDeviceWin::FunctionInfo info;
  info.interface_number = -1;

  base::win::ScopedDevInfo dev_info(
      SetupDiCreateDeviceInfoList(nullptr, nullptr));
  if (!dev_info.is_valid()) {
    USB_PLOG(ERROR) << "SetupDiCreateDeviceInfoList";
    return info;
  }

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  if (!SetupDiOpenDeviceInfo(dev_info.get(), instance_id.c_str(), nullptr, 0,
                             &dev_info_data)) {
    USB_PLOG(ERROR) << "SetupDiOpenDeviceInfo";
    return info;
  }

  info.driver = GetServiceName(dev_info.get(), &dev_info_data);
  if (info.driver.empty()) {
    return info;
  }

  std::optional<std::vector<std::wstring>> hardware_ids =
      GetDeviceStringListProperty(dev_info.get(), &dev_info_data,
                                  DEVPKEY_Device_HardwareIds);
  if (!hardware_ids.has_value()) {
    return info;
  }

  info.interface_number = GetInterfaceNumber(instance_id, *hardware_ids);
  if (info.interface_number == -1)
    return info;

  if (!base::EqualsCaseInsensitiveASCII(info.driver, L"winusb"))
    return info;

  // Boost priority while potentially loading Advapi32.dll on a background
  // thread for the registry functions used below.
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  // There is no standard device interface GUID for USB functions and so we
  // must discover the set of GUIDs that have been set in the registry by
  // the INF file or Microsoft OS Compatibility descriptors before
  // SetupDiGetDeviceInterfaceDetail() can be used to get the device path.
  HKEY key = SetupDiOpenDevRegKey(dev_info.get(), &dev_info_data,
                                  DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
  if (key == INVALID_HANDLE_VALUE) {
    USB_PLOG(ERROR) << "Could not open device registry key";
    return info;
  }
  base::win::RegKey scoped_key(key);

  // Devices may either have DeviceInterfaceGUID or DeviceInterfaceGUIDs
  // registry keys. Read both and only consider it an error if there are no
  // useful results.
  std::vector<std::wstring> device_interface_guids;
  LONG guids_result =
      scoped_key.ReadValues(L"DeviceInterfaceGUIDs", &device_interface_guids);

  std::wstring device_interface_guid;
  LONG guid_result =
      scoped_key.ReadValue(L"DeviceInterfaceGUID", &device_interface_guid);
  if (SUCCEEDED(guid_result)) {
    device_interface_guids.push_back(std::move(device_interface_guid));
  }

  if (device_interface_guids.empty()) {
    if (FAILED(guids_result)) {
      USB_LOG(ERROR) << "Could not read DeviceInterfaceGUIDs: "
                     << logging::SystemErrorCodeToString(guids_result);
    }
    if (FAILED(guid_result)) {
      USB_LOG(ERROR) << "Could not read DeviceInterfaceGUID: "
                     << logging::SystemErrorCodeToString(guid_result);
    }
    return info;
  }

  for (const auto& guid_string : device_interface_guids) {
    // Boost priority while potentially loading Ole32.dll on a background
    // thread for CLSIDFromString().
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    GUID guid;
    if (FAILED(CLSIDFromString(guid_string.c_str(), &guid))) {
      USB_LOG(ERROR) << "Failed to parse device interface GUID: "
                     << guid_string;
      continue;
    }

    info.path = GetDevicePath(instance_id, guid);
    if (!info.path.empty())
      return info;
  }

  return info;
}

}  // namespace

class UsbServiceWin::BlockingTaskRunnerHelper {
 public:
  BlockingTaskRunnerHelper(
      base::WeakPtr<UsbServiceWin> service,
      scoped_refptr<base::SequencedTaskRunner> service_task_runner)
      : service_task_runner_(std::move(service_task_runner)),
        service_(std::move(service)) {
    // Boost priority while potentially loading SetupAPI.dll for the following
    // functions on a background thread.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    base::win::ScopedDevInfo dev_info(
        SetupDiGetClassDevs(&GUID_DEVINTERFACE_USB_DEVICE, nullptr, 0,
                            DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
    if (!dev_info.is_valid()) {
      USB_PLOG(ERROR) << "Failed to set up device enumeration";
      service_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&UsbServiceWin::HelperStarted, service_));
      return;
    }

    SP_DEVICE_INTERFACE_DATA device_interface_data = {};
    device_interface_data.cbSize = sizeof(device_interface_data);
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                                  &GUID_DEVINTERFACE_USB_DEVICE,
                                                  i, &device_interface_data);
         ++i) {
      EnumerateDevice(dev_info.get(), &device_interface_data, std::nullopt);
    }

    if (GetLastError() != ERROR_NO_MORE_ITEMS)
      USB_PLOG(ERROR) << "Failed to enumerate devices";

    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UsbServiceWin::HelperStarted, service_));
  }

  ~BlockingTaskRunnerHelper() = default;

  void OnDeviceAdded(const GUID& guid, const std::wstring& device_path) {
    // Boost priority while potentially loading SetupAPI.dll and Ole32.dll on a
    // background thread for the following functions.
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    base::win::ScopedDevInfo dev_info(SetupDiGetClassDevs(
        &guid, nullptr, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
    if (!dev_info.is_valid()) {
      USB_PLOG(ERROR) << "Failed to set up device enumeration";
      return;
    }

    SP_DEVICE_INTERFACE_DATA device_interface_data = {};
    device_interface_data.cbSize = sizeof(device_interface_data);
    if (!SetupDiOpenDeviceInterface(dev_info.get(), device_path.c_str(), 0,
                                    &device_interface_data)) {
      USB_PLOG(ERROR) << "Failed to add device interface: " << device_path;
      return;
    }

    if (IsEqualGUID(guid, GUID_DEVINTERFACE_USB_DEVICE)) {
      EnumerateDevice(dev_info.get(), &device_interface_data, device_path);
    } else {
      EnumeratePotentialFunction(dev_info.get(), &device_interface_data,
                                 device_path);
    }
  }

 private:
  void EnumerateDevice(HDEVINFO dev_info,
                       SP_DEVICE_INTERFACE_DATA* device_interface_data,
                       const std::optional<std::wstring>& opt_device_path) {
    std::wstring device_path;
    std::wstring* device_path_ptr = &device_path;
    if (opt_device_path) {
      device_path = *opt_device_path;
      device_path_ptr = nullptr;
    }

    uint32_t bus_number;
    uint32_t port_number;
    std::wstring parent_instance_id;
    std::vector<std::wstring> child_instance_ids;
    std::wstring service_name;
    if (!GetDeviceInterfaceDetails(dev_info, device_interface_data,
                                   device_path_ptr, &bus_number, &port_number,
                                   /*instance_id=*/nullptr, &parent_instance_id,
                                   &child_instance_ids,
                                   /*hardware_ids=*/nullptr, &service_name)) {
      return;
    }

    auto driver_type = UsbDeviceWin::DriverType::kUnsupported;
    std::vector<std::pair<int, UsbDeviceWin::FunctionInfo>> functions;
    if (IsCompositeDevice(service_name)) {
      driver_type = UsbDeviceWin::DriverType::kComposite;
      // For composite devices Windows a composite device driver (usually the
      // built-in usbccgp.sys) creates child device nodes for each device
      // function. The device paths for these children must be opened in order
      // to communicate with the WinUSB driver.
      for (const std::wstring& instance_id : child_instance_ids) {
        UsbDeviceWin::FunctionInfo info = GetFunctionInfo(instance_id);
        if (info.interface_number != -1) {
          functions.emplace_back(info.interface_number, info);
        }
      }
    } else if (base::EqualsCaseInsensitiveASCII(service_name, L"winusb")) {
      driver_type = UsbDeviceWin::DriverType::kWinUSB;
      // A non-composite device has a single device node for all interfaces. It
      // may still include multiple functions but they will be ignored.
      UsbDeviceWin::FunctionInfo info;
      info.driver = service_name;
      info.path = device_path;
      functions.emplace_back(/*interface_number=*/0, info);
    }

    std::wstring& hub_path = hub_paths_[parent_instance_id];
    if (hub_path.empty()) {
      hub_path = GetDevicePath(parent_instance_id, GUID_DEVINTERFACE_USB_HUB);
      if (hub_path.empty())
        return;
    }

    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&UsbServiceWin::CreateDeviceObject, service_,
                                  std::move(device_path), std::move(hub_path),
                                  std::move(functions), bus_number, port_number,
                                  driver_type, service_name));
  }

  void EnumeratePotentialFunction(
      HDEVINFO dev_info,
      SP_DEVICE_INTERFACE_DATA* device_interface_data,
      const std::wstring& device_path) {
    std::wstring instance_id;
    std::wstring parent_instance_id;
    std::vector<std::wstring> hardware_ids;
    std::wstring service_name;
    if (!GetDeviceInterfaceDetails(
            dev_info, device_interface_data,
            /*device_path=*/nullptr, /*bus_number=*/nullptr,
            /*port_number=*/nullptr, &instance_id, &parent_instance_id,
            /*child_instance_ids=*/nullptr, &hardware_ids, &service_name)) {
      return;
    }

    int interface_number = GetInterfaceNumber(instance_id, hardware_ids);
    if (interface_number == -1)
      return;

    std::wstring parent_path =
        GetDevicePath(parent_instance_id, GUID_DEVINTERFACE_USB_DEVICE);
    if (parent_path.empty())
      return;

    UsbDeviceWin::FunctionInfo info;
    info.driver = service_name;
    info.path = device_path;

    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&UsbServiceWin::UpdateFunction, service_,
                       std::move(parent_path), interface_number, info));
  }

  std::unordered_map<std::wstring, std::wstring> hub_paths_;

  // Calls back to |service_| must be posted to |service_task_runner_|, which
  // runs tasks on the thread where that object lives.
  scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  base::WeakPtr<UsbServiceWin> service_;
};

UsbServiceWin::UsbServiceWin()
    : blocking_task_runner_(CreateBlockingTaskRunner()) {
  DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
  if (device_monitor)
    device_observation_.Observe(device_monitor);

  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      blocking_task_runner_, weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

UsbServiceWin::~UsbServiceWin() {
  NotifyWillDestroyUsbService();
}

void UsbServiceWin::GetDevices(GetDevicesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enumeration_ready())
    UsbService::GetDevices(std::move(callback));
  else
    enumeration_callbacks_.push_back(std::move(callback));
}

void UsbServiceWin::OnDeviceAdded(const GUID& class_guid,
                                  const std::wstring& device_path) {
  helper_.AsyncCall(&BlockingTaskRunnerHelper::OnDeviceAdded)
      .WithArgs(class_guid, device_path);
}

void UsbServiceWin::OnDeviceRemoved(const GUID& class_guid,
                                    const std::wstring& device_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto by_path_it = devices_by_path_.find(device_path);
  if (by_path_it == devices_by_path_.end())
    return;

  scoped_refptr<UsbDeviceWin> device = by_path_it->second;
  devices_by_path_.erase(by_path_it);
  device->OnDisconnect();

  auto by_guid_it = devices().find(device->guid());
  if (by_guid_it != devices().end() && enumeration_ready()) {
    USB_LOG(USER) << "USB device removed: path=" << device->device_path()
                  << " guid=" << device->guid();

    devices().erase(by_guid_it);
    NotifyDeviceRemoved(device);
  }
}

void UsbServiceWin::HelperStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  helper_started_ = true;
  if (enumeration_ready()) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (auto& callback : enumeration_callbacks_)
      std::move(callback).Run(result);
    enumeration_callbacks_.clear();
  }
}

void UsbServiceWin::CreateDeviceObject(
    const std::wstring& device_path,
    const std::wstring& hub_path,
    const base::flat_map<int, UsbDeviceWin::FunctionInfo>& functions,
    uint32_t bus_number,
    uint32_t port_number,
    UsbDeviceWin::DriverType driver_type,
    const std::wstring& driver_name) {
  if (base::Contains(devices_by_path_, device_path)) {
    USB_LOG(ERROR) << "Got duplicate add event for path: " << device_path;
    return;
  }

  // Devices that appear during initial enumeration are gathered into the first
  // result returned by GetDevices() and prevent device add/remove notifications
  // from being sent.
  if (!enumeration_ready())
    ++first_enumeration_countdown_;

  auto device = base::MakeRefCounted<UsbDeviceWin>(
      device_path, hub_path, functions, bus_number, port_number, driver_type);
  devices_by_path_[device->device_path()] = device;
  device->ReadDescriptors(
      blocking_task_runner_,
      base::BindOnce(&UsbServiceWin::DeviceReady, weak_factory_.GetWeakPtr(),
                     device, driver_name));
}

void UsbServiceWin::UpdateFunction(
    const std::wstring& device_path,
    int interface_number,
    const UsbDeviceWin::FunctionInfo& function_info) {
  auto it = devices_by_path_.find(device_path);
  if (it == devices_by_path_.end())
    return;
  const scoped_refptr<UsbDeviceWin>& device = it->second;

  USB_LOG(EVENT) << "USB device function updated: guid=" << device->guid()
                 << ", interface_number=" << interface_number << ", path=\""
                 << function_info.path << "\", driver=\""
                 << function_info.driver << "\"";
  device->UpdateFunction(interface_number, function_info);
}

void UsbServiceWin::DeviceReady(scoped_refptr<UsbDeviceWin> device,
                                const std::wstring& driver_name,
                                bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool enumeration_became_ready = false;
  if (!enumeration_ready()) {
    DCHECK_GT(first_enumeration_countdown_, 0u);
    first_enumeration_countdown_--;
    if (enumeration_ready())
      enumeration_became_ready = true;
  }

  // If |device| was disconnected while descriptors were being read then it
  // will have been removed from |devices_by_path_|.
  auto it = devices_by_path_.find(device->device_path());
  if (it == devices_by_path_.end()) {
    success = false;
  } else if (success) {
    DCHECK(!base::Contains(devices(), device->guid()));
    devices()[device->guid()] = device;

    USB_LOG(USER) << "USB device added: path=" << device->device_path()
                  << " vendor=" << device->vendor_id() << " \""
                  << device->manufacturer_string()
                  << "\", product=" << device->product_id() << " \""
                  << device->product_string() << "\", serial=\""
                  << device->serial_number() << "\", driver=\"" << driver_name
                  << "\", guid=" << device->guid();
  } else {
    devices_by_path_.erase(it);
  }

  if (enumeration_became_ready) {
    std::vector<scoped_refptr<UsbDevice>> result;
    result.reserve(devices().size());
    for (const auto& map_entry : devices())
      result.push_back(map_entry.second);
    for (auto& callback : enumeration_callbacks_)
      std::move(callback).Run(result);
    enumeration_callbacks_.clear();
  } else if (success && enumeration_ready()) {
    NotifyDeviceAdded(device);
  }
}

}  // namespace device

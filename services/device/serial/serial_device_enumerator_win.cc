// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#define INITGUID
#include <devguid.h>
#include <devpkey.h>
#include <ntddser.h>
#include <setupapi.h>
#include <stdint.h>
#include <usbioctl.h>
#include <usbiodef.h>
#include <usbspec.h>
#include <winioctl.h>

// LogSeverity is both a macro in setupapi.h and an enum in absl, which is used
// indirectly via //base.
#undef LogSeverity

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/byte_conversions.h"
#include "base/scoped_generic.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/scoped_devinfo.h"
#include "base/win/scoped_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/device_features.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_service_win.h"
#include "services/device/utils/setupdi_utils_win.h"
#include "third_party/re2/src/re2/re2.h"

namespace device {

namespace {

// Returns the value of `property` for the device described by
// `dev_info_data` as a UTF-8 encoded string.
std::optional<std::string> GetProperty(HDEVINFO dev_info,
                                       SP_DEVINFO_DATA* dev_info_data,
                                       const DEVPROPKEY& property) {
  // SetupDiGetDeviceProperty() makes an RPC which may block.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::optional<std::wstring> property_value = GetDeviceStringProperty(
      dev_info, dev_info_data, property, device_event_log::LOG_TYPE_SERIAL);
  if (!property_value) {
    return std::nullopt;
  }
  return base::WideToUTF8(*property_value);
}

// Get the port name from the registry.
std::optional<std::string> GetPortName(HDEVINFO dev_info,
                                       SP_DEVINFO_DATA* dev_info_data) {
  HKEY key = SetupDiOpenDevRegKey(dev_info, dev_info_data, DICS_FLAG_GLOBAL, 0,
                                  DIREG_DEV, KEY_READ);
  if (key == INVALID_HANDLE_VALUE) {
    SERIAL_PLOG(ERROR) << "Could not open device registry key";
    return std::nullopt;
  }
  base::win::RegKey scoped_key(key);

  std::wstring port_name;
  LONG result = scoped_key.ReadValue(L"PortName", &port_name);
  if (result != ERROR_SUCCESS) {
    SERIAL_LOG(ERROR) << "Failed to read port name: "
                      << logging::SystemErrorCodeToString(result);
    return std::nullopt;
  }

  return base::SysWideToUTF8(port_name);
}

// Deduce the path for the device from the port name.
base::FilePath GetPath(std::string port_name) {
  // For COM numbers less than 9, CreateFile is called with a string such as
  // "COM1". For numbers greater than 9, a prefix of "\\.\" must be added.
  if (port_name.length() > std::string_view("COM9").length()) {
    return base::FilePath(LR"(\\.\)").AppendASCII(port_name);
  }

  return base::FilePath::FromUTF8Unsafe(port_name);
}

// Searches for the display name in the device's friendly name. Returns nullopt
// if the name does not match the expected pattern.
std::optional<std::string> GetDisplayName(const std::string& friendly_name) {
  std::string display_name;
  if (!RE2::PartialMatch(friendly_name, R"((.*) \(COM[0-9]+\))",
                         &display_name)) {
    return std::nullopt;
  }
  return display_name;
}

// Searches for the vendor ID in the device's instance ID. Returns nullopt if
// the instance ID does not match the expected pattern.
std::optional<uint32_t> GetVendorID(const std::string& instance_id) {
  std::string vendor_id_str;
  if (!RE2::PartialMatch(instance_id, "VID_([0-9a-fA-F]+)", &vendor_id_str)) {
    return std::nullopt;
  }

  uint32_t vendor_id;
  if (!base::HexStringToUInt(vendor_id_str, &vendor_id)) {
    return std::nullopt;
  }

  return vendor_id;
}

// Searches for the product ID in the device's instance ID. Returns nullopt if
// the instance ID does not match the expected pattern.
std::optional<uint32_t> GetProductID(const std::string& instance_id) {
  std::string product_id_str;
  if (!RE2::PartialMatch(instance_id, "PID_([0-9a-fA-F]+)", &product_id_str)) {
    return std::nullopt;
  }

  uint32_t product_id;
  if (!base::HexStringToUInt(product_id_str, &product_id)) {
    return std::nullopt;
  }

  return product_id;
}

std::wstring GetDevicePath(const std::wstring& instance_id,
                           const GUID& device_interface_guid) {
  base::win::ScopedDevInfo dev_info(
      SetupDiGetClassDevs(&device_interface_guid, instance_id.c_str(), 0,
                          DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));
  if (!dev_info.is_valid()) {
    return std::wstring();
  }

  SP_DEVICE_INTERFACE_DATA device_interface_data = {};
  device_interface_data.cbSize = sizeof(device_interface_data);
  if (!SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr,
                                   &device_interface_guid, 0,
                                   &device_interface_data)) {
    return std::wstring();
  }

  DWORD required_size = 0;
  if (!SetupDiGetDeviceInterfaceDetail(dev_info.get(), &device_interface_data,
                                       nullptr, 0, &required_size, nullptr) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return std::wstring();
  }

  std::vector<uint8_t> buffer(required_size);
  // SAFETY: `buffer` holds `required_size` bytes as reported by
  // SetupDiGetDeviceInterfaceDetail() above. Only the `cbSize` field and the
  // NUL-terminated `DevicePath` string written by the call below are accessed
  // through this pointer, and both lie within those `required_size` bytes.
  auto* device_interface_detail_data = UNSAFE_BUFFERS(
      reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(buffer.data()));
  device_interface_detail_data->cbSize = sizeof(*device_interface_detail_data);
  if (!SetupDiGetDeviceInterfaceDetail(dev_info.get(), &device_interface_data,
                                       device_interface_detail_data,
                                       required_size, nullptr, nullptr)) {
    return std::wstring();
  }

  return std::wstring(device_interface_detail_data->DevicePath);
}

struct UsbDeviceLocation {
  std::wstring hub_path;
  uint32_t port_number;
  int interface_number = -1;
};

std::optional<UsbDeviceLocation> FindUsbDeviceLocation(
    std::wstring instance_id) {
  // SetupDi functions make RPCs to the device manager service which may
  // block.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int interface_number = -1;
  for (int depth = 0; depth < 8 && !instance_id.empty(); ++depth) {
    base::win::ScopedDevInfo dev_info(
        SetupDiCreateDeviceInfoList(nullptr, nullptr));
    if (!dev_info.is_valid()) {
      return std::nullopt;
    }

    SP_DEVINFO_DATA dev_info_data = {};
    dev_info_data.cbSize = sizeof(dev_info_data);
    if (!SetupDiOpenDeviceInfo(dev_info.get(), instance_id.c_str(), nullptr, 0,
                               &dev_info_data)) {
      return std::nullopt;
    }

    if (interface_number == -1) {
      std::optional<std::vector<std::wstring>> hardware_ids =
          GetDeviceStringListProperty(dev_info.get(), &dev_info_data,
                                      DEVPKEY_Device_HardwareIds,
                                      device_event_log::LOG_TYPE_SERIAL);
      if (hardware_ids) {
        interface_number = GetInterfaceNumber(instance_id, *hardware_ids);
      }
    }

    std::optional<std::wstring> parent_instance_id = GetDeviceStringProperty(
        dev_info.get(), &dev_info_data, DEVPKEY_Device_Parent,
        device_event_log::LOG_TYPE_SERIAL);
    std::wstring device_path =
        GetDevicePath(instance_id, GUID_DEVINTERFACE_USB_DEVICE);
    if (!device_path.empty() && parent_instance_id) {
      std::optional<uint32_t> port_number = GetDeviceUint32Property(
          dev_info.get(), &dev_info_data, DEVPKEY_Device_Address,
          device_event_log::LOG_TYPE_SERIAL);
      std::wstring hub_path =
          GetDevicePath(*parent_instance_id, GUID_DEVINTERFACE_USB_HUB);
      if (port_number && !hub_path.empty()) {
        return UsbDeviceLocation{std::move(hub_path), *port_number,
                                 interface_number};
      }
    }

    if (!parent_instance_id) {
      return std::nullopt;
    }
    instance_id = *parent_instance_id;
  }

  return std::nullopt;
}

std::pair<DWORD, DWORD> DeviceIoControlBlocking(HANDLE handle,
                                                DWORD control_code,
                                                void* buffer,
                                                DWORD buffer_size) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DWORD bytes_transferred = 0;
  if (!DeviceIoControl(handle, control_code, buffer, buffer_size, buffer,
                       buffer_size, &bytes_transferred, nullptr)) {
    return {GetLastError(), bytes_transferred};
  }

  return {ERROR_SUCCESS, bytes_transferred};
}

std::optional<std::vector<uint8_t>> ReadUsbDescriptorFromHub(
    HANDLE hub_handle,
    uint32_t port_number,
    uint8_t descriptor_type,
    uint8_t descriptor_index,
    uint16_t language_id,
    size_t length) {
  std::vector<uint8_t> request_buffer(sizeof(USB_DESCRIPTOR_REQUEST) + length);
  // SAFETY: `request_buffer` was allocated with
  // sizeof(USB_DESCRIPTOR_REQUEST) + `length` bytes, so the
  // USB_DESCRIPTOR_REQUEST fields written through this pointer are within the
  // allocation.
  auto* descriptor_request = UNSAFE_BUFFERS(
      reinterpret_cast<USB_DESCRIPTOR_REQUEST*>(request_buffer.data()));
  descriptor_request->ConnectionIndex = port_number;
  descriptor_request->SetupPacket.bmRequest = BMREQUEST_DEVICE_TO_HOST;
  descriptor_request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
  descriptor_request->SetupPacket.wValue =
      descriptor_type << 8 | descriptor_index;
  descriptor_request->SetupPacket.wIndex = language_id;
  descriptor_request->SetupPacket.wLength = length;

  auto [result, bytes_transferred] = DeviceIoControlBlocking(
      hub_handle, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
      request_buffer.data(), request_buffer.size());
  if (result != ERROR_SUCCESS ||
      bytes_transferred <= sizeof(USB_DESCRIPTOR_REQUEST)) {
    return std::nullopt;
  }

  base::span<const uint8_t> result_span =
      base::span(request_buffer)
          .first(bytes_transferred)
          .subspan(sizeof(USB_DESCRIPTOR_REQUEST));
  return base::ToVector(result_span);
}

std::optional<std::string> ReadUsbStringDescriptor(HANDLE hub_handle,
                                                   uint32_t port_number,
                                                   uint8_t descriptor_index) {
  if (descriptor_index == 0) {
    return std::nullopt;
  }

  uint16_t language_id = 0x0409;
  std::optional<std::vector<uint8_t>> language_descriptor =
      ReadUsbDescriptorFromHub(hub_handle, port_number,
                               USB_STRING_DESCRIPTOR_TYPE, 0, 0, 255);
  if (language_descriptor && language_descriptor->size() >= 4) {
    language_id = base::U16FromLittleEndian(
        base::span(*language_descriptor).subspan<2, 2>());
  }

  std::optional<std::vector<uint8_t>> string_descriptor =
      ReadUsbDescriptorFromHub(hub_handle, port_number,
                               USB_STRING_DESCRIPTOR_TYPE, descriptor_index,
                               language_id, 255);
  std::u16string utf16;
  if (!string_descriptor ||
      !ParseUsbStringDescriptor(*string_descriptor, &utf16)) {
    return std::nullopt;
  }

  return base::UTF16ToUTF8(utf16);
}

}  // namespace

namespace serial_win_internal {

std::optional<uint8_t> FindInterfaceStringDescriptorIndex(
    base::span<const uint8_t> configuration_descriptor,
    int interface_number) {
  auto it = configuration_descriptor.begin();
  while (it != configuration_descriptor.end()) {
    if (std::distance(it, configuration_descriptor.end()) < 2) {
      return std::nullopt;
    }

    uint8_t length = it[0];
    if (length < 2 ||
        length > std::distance(it, configuration_descriptor.end())) {
      return std::nullopt;
    }

    if (it[1] == USB_INTERFACE_DESCRIPTOR_TYPE && length >= 9 &&
        it[2] == interface_number && it[3] == 0) {
      return it[8];
    }

    std::advance(it, length);
  }

  return std::nullopt;
}

std::optional<std::string> BuildUsbDisplayName(
    const std::optional<std::string>& product_name,
    const std::optional<std::string>& interface_name) {
  if (product_name && interface_name && *product_name != *interface_name) {
    return base::StringPrintf("%s - %s", product_name->c_str(),
                              interface_name->c_str());
  }
  if (product_name) {
    return product_name;
  }
  return interface_name;
}

}  // namespace serial_win_internal

namespace {

std::optional<std::string> ReadUsbDisplayName(
    const UsbDeviceLocation& location) {
  base::win::ScopedHandle hub_handle(
      CreateFile(location.hub_path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE,
                 nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
  if (!hub_handle.is_valid()) {
    return std::nullopt;
  }

  USB_NODE_CONNECTION_INFORMATION_EX node_connection_info = {};
  node_connection_info.ConnectionIndex = location.port_number;
  auto [result, bytes_transferred] = DeviceIoControlBlocking(
      hub_handle.Get(), IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
      &node_connection_info, sizeof(node_connection_info));
  if (result != ERROR_SUCCESS ||
      bytes_transferred != sizeof(node_connection_info)) {
    return std::nullopt;
  }

  std::optional<std::string> product_name =
      ReadUsbStringDescriptor(hub_handle.Get(), location.port_number,
                              node_connection_info.DeviceDescriptor.iProduct);
  std::optional<std::string> interface_name;
  if (location.interface_number != -1) {
    for (uint8_t i = 0;
         i < node_connection_info.DeviceDescriptor.bNumConfigurations; ++i) {
      std::optional<std::vector<uint8_t>> config_header =
          ReadUsbDescriptorFromHub(hub_handle.Get(), location.port_number,
                                   USB_CONFIGURATION_DESCRIPTOR_TYPE, i, 0, 9);
      if (!config_header || config_header->size() < 9) {
        continue;
      }

      uint16_t total_length =
          base::U16FromLittleEndian(base::span(*config_header).subspan<2, 2>());
      std::optional<std::vector<uint8_t>> config_descriptor =
          ReadUsbDescriptorFromHub(hub_handle.Get(), location.port_number,
                                   USB_CONFIGURATION_DESCRIPTOR_TYPE, i, 0,
                                   total_length);
      if (!config_descriptor) {
        continue;
      }

      std::optional<uint8_t> interface_string_index =
          serial_win_internal::FindInterfaceStringDescriptorIndex(
              *config_descriptor, location.interface_number);
      if (interface_string_index && *interface_string_index != 0) {
        interface_name = ReadUsbStringDescriptor(
            hub_handle.Get(), location.port_number, *interface_string_index);
        break;
      }
    }
  }

  return serial_win_internal::BuildUsbDisplayName(product_name, interface_name);
}

}  // namespace

class SerialDeviceEnumeratorWin::UiThreadHelper
    : public DeviceMonitorWin::Observer {
 public:
  UiThreadHelper(base::WeakPtr<SerialDeviceEnumeratorWin> enumerator,
                 scoped_refptr<base::SequencedTaskRunner> task_runner)
      : enumerator_(std::move(enumerator)),
        task_runner_(std::move(task_runner)) {
    // Note that this uses GUID_DEVINTERFACE_COMPORT even though we use
    // GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR for enumeration because it
    // doesn't seem to make a difference and ports which aren't enumerable by
    // device interface don't generate WM_DEVICECHANGE events.
    device_observation_.Observe(
        DeviceMonitorWin::GetForDeviceInterface(GUID_DEVINTERFACE_COMPORT));
  }

  // Disallow copy and assignment.
  UiThreadHelper(UiThreadHelper&) = delete;
  UiThreadHelper& operator=(UiThreadHelper&) = delete;

  virtual ~UiThreadHelper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void OnDeviceAdded(const GUID& class_guid,
                     const std::wstring& device_path) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialDeviceEnumeratorWin::OnPathAdded,
                                  enumerator_, device_path));
  }

  void OnDeviceRemoved(const GUID& class_guid,
                       const std::wstring& device_path) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialDeviceEnumeratorWin::OnPathRemoved,
                                  enumerator_, device_path));
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak reference to the SerialDeviceEnumeratorWin that owns this object.
  // Calls on |enumerator_| must be posted to |task_runner_|.
  base::WeakPtr<SerialDeviceEnumeratorWin> enumerator_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::ScopedObservation<DeviceMonitorWin, DeviceMonitorWin::Observer>
      device_observation_{this};
};

SerialDeviceEnumeratorWin::SerialDeviceEnumeratorWin(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  helper_ = base::SequenceBound<UiThreadHelper>(
      std::move(ui_task_runner), weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());

  DoInitialEnumeration();
}

SerialDeviceEnumeratorWin::~SerialDeviceEnumeratorWin() = default;

void SerialDeviceEnumeratorWin::OnPathAdded(const std::wstring& device_path) {
  base::win::ScopedDevInfo dev_info(
      SetupDiCreateDeviceInfoList(nullptr, nullptr));
  if (!dev_info.is_valid())
    return;

  if (!SetupDiOpenDeviceInterface(dev_info.get(), device_path.c_str(), 0,
                                  nullptr)) {
    return;
  }

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  if (!SetupDiEnumDeviceInfo(dev_info.get(), 0, &dev_info_data))
    return;

  EnumeratePort(dev_info.get(), &dev_info_data, /*check_port_name=*/false);
}

void SerialDeviceEnumeratorWin::OnPathRemoved(const std::wstring& device_path) {
  base::win::ScopedDevInfo dev_info(
      SetupDiCreateDeviceInfoList(nullptr, nullptr));
  if (!dev_info.is_valid())
    return;

  if (!SetupDiOpenDeviceInterface(dev_info.get(), device_path.c_str(), 0,
                                  nullptr)) {
    return;
  }

  SP_DEVINFO_DATA dev_info_data = {};
  dev_info_data.cbSize = sizeof(dev_info_data);
  if (!SetupDiEnumDeviceInfo(dev_info.get(), 0, &dev_info_data))
    return;

  std::optional<std::string> port_name =
      GetPortName(dev_info.get(), &dev_info_data);
  if (!port_name)
    return;

  auto it = paths_.find(GetPath(*port_name));
  if (it == paths_.end())
    return;

  base::UnguessableToken token = it->second;

  paths_.erase(it);
  RemovePort(token);
}

void SerialDeviceEnumeratorWin::DoInitialEnumeration() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // On Windows 10 and above most COM port drivers register using the COMPORT
  // device interface class. Try to enumerate these first.
  {
    base::win::ScopedDevInfo dev_info;
    dev_info.reset(SetupDiGetClassDevs(&GUID_DEVINTERFACE_COMPORT, nullptr, 0,
                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    if (!dev_info.is_valid())
      return;

    SP_DEVINFO_DATA dev_info_data = {.cbSize = sizeof(dev_info_data)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info.get(), i, &dev_info_data);
         i++) {
      EnumeratePort(dev_info.get(), &dev_info_data, /*check_port_name=*/false);
    }
  }

  // To detect devices which don't register with GUID_DEVINTERFACE_COMPORT also
  // enuerate all devices in the "Ports" and "Modems" device classes. These must
  // be checked to see if the port name starts with "COM" because it also
  // includes LPT ports.
  constexpr const GUID* kDeviceClasses[] = {&GUID_DEVCLASS_MODEM,
                                            &GUID_DEVCLASS_PORTS};
  for (const GUID* guid : kDeviceClasses) {
    base::win::ScopedDevInfo dev_info;
    dev_info.reset(SetupDiGetClassDevs(guid, nullptr, 0, DIGCF_PRESENT));
    if (!dev_info.is_valid())
      return;

    SP_DEVINFO_DATA dev_info_data = {.cbSize = sizeof(dev_info_data)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info.get(), i, &dev_info_data);
         i++) {
      EnumeratePort(dev_info.get(), &dev_info_data, /*check_port_name=*/true);
    }
  }
}

void SerialDeviceEnumeratorWin::EnumeratePort(HDEVINFO dev_info,
                                              SP_DEVINFO_DATA* dev_info_data,
                                              bool check_port_name) {
  std::optional<std::string> port_name = GetPortName(dev_info, dev_info_data);
  if (!port_name)
    return;

  if (check_port_name && !base::StartsWith(*port_name, "COM"))
    return;

  // Check whether the currently enumerating port has been seen before since
  // the method above will generate duplicate enumerations for some ports.
  base::FilePath path = GetPath(*port_name);
  if (paths_.contains(path))
    return;

  std::optional<std::string> instance_id =
      GetProperty(dev_info, dev_info_data, DEVPKEY_Device_InstanceId);
  if (!instance_id)
    return;

  // Some versions of Windows pad this string with a variable number of NUL
  // bytes for no discernible reason.
  instance_id = std::string(base::TrimString(
      *instance_id, std::string_view("\0", 1), base::TRIM_TRAILING));

  base::UnguessableToken token = base::UnguessableToken::Create();
  auto info = mojom::SerialPortInfo::New();
  info->token = token;
  info->path = path;
  info->device_instance_id = *instance_id;

  if (base::FeatureList::IsEnabled(features::kSerialUsbDisplayNameWin)) {
    std::optional<UsbDeviceLocation> usb_device_location =
        FindUsbDeviceLocation(base::UTF8ToWide(*instance_id));
    if (usb_device_location) {
      info->display_name = ReadUsbDisplayName(*usb_device_location);
    }
  }

  if (!info->display_name) {
    info->display_name = GetProperty(dev_info, dev_info_data,
                                     DEVPKEY_Device_BusReportedDeviceDesc);
  }

  if (info->display_name) {
    // This string is also sometimes padded with a variable number of NUL bytes
    // for no discernible reason.
    info->display_name = std::string(base::TrimString(
        *info->display_name, std::string_view("\0", 1), base::TRIM_TRAILING));
  } else {
    // Fall back to the "friendly name" if no "bus reported device description"
    // is available. This name will likely be the same for all devices using the
    // same driver.
    std::optional<std::string> friendly_name =
        GetProperty(dev_info, dev_info_data, DEVPKEY_Device_FriendlyName);
    if (!friendly_name)
      return;

    info->display_name = GetDisplayName(*friendly_name);
  }

  // The instance ID looks like "FTDIBUS\VID_0403+PID_6001+A703X87GA\0000".
  std::optional<uint32_t> vendor_id = GetVendorID(*instance_id);
  std::optional<uint32_t> product_id = GetProductID(*instance_id);
  std::optional<std::string> vendor_id_str, product_id_str;
  if (vendor_id) {
    info->has_vendor_id = true;
    info->vendor_id = *vendor_id;
    vendor_id_str = base::StringPrintf("%04X", *vendor_id);
  }
  if (product_id) {
    info->has_product_id = true;
    info->product_id = *product_id;
    product_id_str = base::StringPrintf("%04X", *product_id);
  }

  SERIAL_LOG(EVENT) << "Serial device added: path=" << info->path
                    << " instance_id=" << info->device_instance_id
                    << " vid=" << vendor_id_str.value_or("(none)")
                    << " pid=" << product_id_str.value_or("(none)");

  paths_.insert(std::make_pair(path, token));
  AddPort(std::move(info));
}

}  // namespace device

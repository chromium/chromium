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

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
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
#include "components/device_event_log/device_event_log.h"
#include "third_party/re2/src/re2/re2.h"

namespace device {

namespace {

std::optional<std::string> GetProperty(HDEVINFO dev_info,
                                       SP_DEVINFO_DATA* dev_info_data,
                                       const DEVPROPKEY& property) {
  // SetupDiGetDeviceProperty() makes an RPC which may block.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, /*PropertyBuffer=*/nullptr,
                               /*PropertyBufferSize=*/0, &required_size,
                               /*Flags=*/0) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
      property_type != DEVPROP_TYPE_STRING) {
    return std::nullopt;
  }

  std::wstring buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, /*RequiredSize=*/nullptr, /*Flags=*/0)) {
    return std::nullopt;
  }

  return base::WideToUTF8(buffer);
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
  if (base::Contains(paths_, path))
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

  // TODO(crbug.com/40653536): While the "bus reported device
  // description" is usually the USB product string this is still up to the
  // individual serial driver and could be equal to the "friendly name". It
  // would be more reliable to read the real USB strings here.
  info->display_name = GetProperty(dev_info, dev_info_data,
                                   DEVPKEY_Device_BusReportedDeviceDesc);
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

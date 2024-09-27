// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_api_utils.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/common/api/bluetooth.h"

namespace bluetooth = extensions::api::bluetooth;

using bluetooth::VendorIdSource;
using device::BluetoothDevice;
using device::BluetoothDeviceType;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
using device::BluetoothTransport;
#endif

namespace {

bool ConvertVendorIDSourceToApi(const BluetoothDevice::VendorIDSource& input,
                                bluetooth::VendorIdSource* output) {
  switch (input) {
    case BluetoothDevice::VENDOR_ID_UNKNOWN:
      *output = bluetooth::VendorIdSource::kNone;
      return true;
    case BluetoothDevice::VENDOR_ID_BLUETOOTH:
      *output = bluetooth::VendorIdSource::kBluetooth;
      return true;
    case BluetoothDevice::VENDOR_ID_USB:
      *output = bluetooth::VendorIdSource::kUsb;
      return true;
    default:
      DUMP_WILL_BE_NOTREACHED();
      return false;
  }
}

bool ConvertDeviceTypeToApi(const BluetoothDeviceType& input,
                            bluetooth::DeviceType* output) {
  switch (input) {
    case BluetoothDeviceType::UNKNOWN:
      *output = bluetooth::DeviceType::kNone;
      return true;
    case BluetoothDeviceType::COMPUTER:
      *output = bluetooth::DeviceType::kComputer;
      return true;
    case BluetoothDeviceType::PHONE:
      *output = bluetooth::DeviceType::kPhone;
      return true;
    case BluetoothDeviceType::MODEM:
      *output = bluetooth::DeviceType::kModem;
      return true;
    case BluetoothDeviceType::AUDIO:
      *output = bluetooth::DeviceType::kAudio;
      return true;
    case BluetoothDeviceType::CAR_AUDIO:
      *output = bluetooth::DeviceType::kCarAudio;
      return true;
    case BluetoothDeviceType::VIDEO:
      *output = bluetooth::DeviceType::kVideo;
      return true;
    case BluetoothDeviceType::PERIPHERAL:
      *output = bluetooth::DeviceType::kPeripheral;
      return true;
    case BluetoothDeviceType::JOYSTICK:
      *output = bluetooth::DeviceType::kJoystick;
      return true;
    case BluetoothDeviceType::GAMEPAD:
      *output = bluetooth::DeviceType::kGamepad;
      return true;
    case BluetoothDeviceType::KEYBOARD:
      *output = bluetooth::DeviceType::kKeyboard;
      return true;
    case BluetoothDeviceType::MOUSE:
      *output = bluetooth::DeviceType::kMouse;
      return true;
    case BluetoothDeviceType::TABLET:
      *output = bluetooth::DeviceType::kTablet;
      return true;
    case BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      *output = bluetooth::DeviceType::kKeyboardMouseCombo;
      return true;
    default:
      return false;
  }
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
bool ConvertTransportToApi(const BluetoothTransport& input,
                           bluetooth::Transport* output) {
  switch (input) {
    case BluetoothTransport::BLUETOOTH_TRANSPORT_INVALID:
      *output = bluetooth::Transport::kInvalid;
      return true;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC:
      *output = bluetooth::Transport::kClassic;
      return true;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_LE:
      *output = bluetooth::Transport::kLe;
      return true;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL:
      *output = bluetooth::Transport::kDual;
      return true;
    default:
      return false;
  }
}
#endif

}  // namespace

namespace extensions {
namespace api {
namespace bluetooth {

void BluetoothDeviceToApiDevice(const device::BluetoothDevice& device,
                                Device* out) {
  out->address = device.GetAddress();
  out->name = base::UTF16ToUTF8(device.GetNameForDisplay());
  out->device_class = device.GetBluetoothClass();

  // Only include the Device ID members when one exists for the device, and
  // always include all or none.
  if (ConvertVendorIDSourceToApi(device.GetVendorIDSource(),
                                 &(out->vendor_id_source)) &&
      out->vendor_id_source != VendorIdSource::kNone) {
    out->vendor_id = device.GetVendorID();
    out->product_id = device.GetProductID();
    out->device_id = device.GetDeviceID();
  }

  ConvertDeviceTypeToApi(device.GetDeviceType(), &(out->type));

  out->paired = device.IsPaired();
  out->connected = device.IsConnected();
  out->connecting = device.IsConnecting();
  out->connectable = device.IsConnectable();

  out->uuids.emplace();
  const device::BluetoothDevice::UUIDSet& uuids = device.GetUUIDs();
  for (const auto& uuid : uuids) {
    out->uuids->push_back(uuid.canonical_value());
  }

  if (device.GetInquiryRSSI()) {
    out->inquiry_rssi = device.GetInquiryRSSI().value();
  } else {
    out->inquiry_rssi.reset();
  }

  if (device.GetInquiryTxPower()) {
    out->inquiry_tx_power = device.GetInquiryTxPower().value();
  } else {
    out->inquiry_tx_power.reset();
  }

#if BUILDFLAG(IS_CHROMEOS)
  std::optional<device::BluetoothDevice::BatteryInfo> battery_info =
      device.GetBatteryInfo(device::BluetoothDevice::BatteryType::kDefault);

  if (battery_info && battery_info->percentage.has_value()) {
    out->battery_percentage = battery_info->percentage.value();
  } else {
    out->battery_percentage.reset();
  }
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ConvertTransportToApi(device.GetType(), &(out->transport));
#endif
}

void PopulateAdapterState(const device::BluetoothAdapter& adapter,
                          AdapterState* out) {
  out->discovering = adapter.IsDiscovering();
  out->available = adapter.IsPresent();
  out->powered = adapter.IsPowered();
  out->name = adapter.GetName();
  out->address = adapter.GetAddress();
}

#if BUILDFLAG(IS_CHROMEOS)
device::BluetoothFilterType ToBluetoothDeviceFilterType(FilterType type) {
  switch (type) {
    case FilterType::kNone:
    case FilterType::kAll:
      return device::BluetoothFilterType::ALL;
    case FilterType::kKnown:
      return device::BluetoothFilterType::KNOWN;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}
#endif

}  // namespace bluetooth
}  // namespace api
}  // namespace extensions

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
      *output = bluetooth::VENDOR_ID_SOURCE_NONE;
      return true;
    case BluetoothDevice::VENDOR_ID_BLUETOOTH:
      *output = bluetooth::VENDOR_ID_SOURCE_BLUETOOTH;
      return true;
    case BluetoothDevice::VENDOR_ID_USB:
      *output = bluetooth::VENDOR_ID_SOURCE_USB;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertDeviceTypeToApi(const BluetoothDeviceType& input,
                            bluetooth::DeviceType* output) {
  switch (input) {
    case BluetoothDeviceType::UNKNOWN:
      *output = bluetooth::DEVICE_TYPE_NONE;
      return true;
    case BluetoothDeviceType::COMPUTER:
      *output = bluetooth::DEVICE_TYPE_COMPUTER;
      return true;
    case BluetoothDeviceType::PHONE:
      *output = bluetooth::DEVICE_TYPE_PHONE;
      return true;
    case BluetoothDeviceType::MODEM:
      *output = bluetooth::DEVICE_TYPE_MODEM;
      return true;
    case BluetoothDeviceType::AUDIO:
      *output = bluetooth::DEVICE_TYPE_AUDIO;
      return true;
    case BluetoothDeviceType::CAR_AUDIO:
      *output = bluetooth::DEVICE_TYPE_CARAUDIO;
      return true;
    case BluetoothDeviceType::VIDEO:
      *output = bluetooth::DEVICE_TYPE_VIDEO;
      return true;
    case BluetoothDeviceType::PERIPHERAL:
      *output = bluetooth::DEVICE_TYPE_PERIPHERAL;
      return true;
    case BluetoothDeviceType::JOYSTICK:
      *output = bluetooth::DEVICE_TYPE_JOYSTICK;
      return true;
    case BluetoothDeviceType::GAMEPAD:
      *output = bluetooth::DEVICE_TYPE_GAMEPAD;
      return true;
    case BluetoothDeviceType::KEYBOARD:
      *output = bluetooth::DEVICE_TYPE_KEYBOARD;
      return true;
    case BluetoothDeviceType::MOUSE:
      *output = bluetooth::DEVICE_TYPE_MOUSE;
      return true;
    case BluetoothDeviceType::TABLET:
      *output = bluetooth::DEVICE_TYPE_TABLET;
      return true;
    case BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      *output = bluetooth::DEVICE_TYPE_KEYBOARDMOUSECOMBO;
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
      *output = bluetooth::TRANSPORT_INVALID;
      return true;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC:
      *output = bluetooth::TRANSPORT_CLASSIC;
      return true;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_LE:
      *output = bluetooth::TRANSPORT_LE;
      return true;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL:
      *output = bluetooth::TRANSPORT_DUAL;
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
      out->vendor_id_source != VENDOR_ID_SOURCE_NONE) {
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

  if (device.GetInquiryRSSI())
    out->inquiry_rssi = device.GetInquiryRSSI().value();
  else
    out->inquiry_rssi.reset();

  if (device.GetInquiryTxPower()) {
    out->inquiry_tx_power = device.GetInquiryTxPower().value();
  } else {
    out->inquiry_tx_power.reset();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  absl::optional<device::BluetoothDevice::BatteryInfo> battery_info =
      device.GetBatteryInfo(device::BluetoothDevice::BatteryType::kDefault);

  if (battery_info && battery_info->percentage.has_value())
    out->battery_percentage = battery_info->percentage.value();
  else
    out->battery_percentage.reset();
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
device::BluetoothFilterType ToBluetoothDeviceFilterType(FilterType type) {
  switch (type) {
    case FilterType::FILTER_TYPE_NONE:
    case FilterType::FILTER_TYPE_ALL:
      return device::BluetoothFilterType::ALL;
    case FilterType::FILTER_TYPE_KNOWN:
      return device::BluetoothFilterType::KNOWN;
    default:
      NOTREACHED();
  }
}
#endif

}  // namespace bluetooth
}  // namespace api
}  // namespace extensions

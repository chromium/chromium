// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_api_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/common/api/bluetooth.h"

namespace bluetooth = extensions::api::bluetooth;

using bluetooth::VendorIdSource;
using device::BluetoothDevice;
using device::BluetoothDeviceType;
#if defined(OS_LINUX)
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

#if defined(OS_LINUX)
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
  out->name.reset(
      new std::string(base::UTF16ToUTF8(device.GetNameForDisplay())));
  out->device_class.reset(new int(device.GetBluetoothClass()));

  // Only include the Device ID members when one exists for the device, and
  // always include all or none.
  if (ConvertVendorIDSourceToApi(device.GetVendorIDSource(),
                                 &(out->vendor_id_source)) &&
      out->vendor_id_source != VENDOR_ID_SOURCE_NONE) {
    out->vendor_id.reset(new int(device.GetVendorID()));
    out->product_id.reset(new int(device.GetProductID()));
    out->device_id.reset(new int(device.GetDeviceID()));
  }

  ConvertDeviceTypeToApi(device.GetDeviceType(), &(out->type));

  out->paired.reset(new bool(device.IsPaired()));
  out->connected.reset(new bool(device.IsConnected()));
  out->connecting.reset(new bool(device.IsConnecting()));
  out->connectable.reset(new bool(device.IsConnectable()));

  std::vector<std::string>* string_uuids = new std::vector<std::string>();
  const device::BluetoothDevice::UUIDSet& uuids = device.GetUUIDs();
  for (const auto& uuid : uuids) {
    string_uuids->push_back(uuid.canonical_value());
  }
  out->uuids.reset(string_uuids);

  if (device.GetInquiryRSSI())
    out->inquiry_rssi.reset(new int(device.GetInquiryRSSI().value()));
  else
    out->inquiry_rssi.reset();

  if (device.GetInquiryTxPower())
    out->inquiry_tx_power.reset(new int(device.GetInquiryTxPower().value()));
  else
    out->inquiry_tx_power.reset();

#if defined(OS_CHROMEOS)
  if (device.battery_percentage())
    out->battery_percentage.reset(new int(device.battery_percentage().value()));
  else
    out->battery_percentage.reset();
#endif

#if defined(OS_LINUX)
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

#if defined(OS_CHROMEOS)
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

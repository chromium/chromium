// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_classic_device_mac.h"

#include <string>

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_socket_mac.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

// Undocumented API for accessing the Bluetooth transmit power level.
// Similar to the API defined here [ http://goo.gl/20Q5vE ].
@interface IOBluetoothHostController (UndocumentedAPI)
- (IOReturn)
    BluetoothHCIReadTransmitPowerLevel:(BluetoothConnectionHandle)connection
                                inType:(BluetoothHCITransmitPowerLevelType)type
                 outTransmitPowerLevel:(BluetoothHCITransmitPowerLevel*)level;
@end

// A simple helper class that forwards Bluetooth device disconnect notification
// to its wrapped |_device|.
@interface BluetoothDeviceDisconnectListener : NSObject {
 @private
  // The BluetoothClassicDeviceMac that owns |self|.
  raw_ptr<device::BluetoothClassicDeviceMac> _device;

  // The OS mechanism used to subscribe to and unsubscribe from Bluetooth device
  // disconnect notification.
  IOBluetoothUserNotification* __weak _disconnectNotification;
}

- (instancetype)initWithDevice:(device::BluetoothClassicDeviceMac*)device;
- (void)deviceDisconnected:(IOBluetoothUserNotification*)notification
                    device:(IOBluetoothDevice*)device;
- (void)stopListening;

@end

@implementation BluetoothDeviceDisconnectListener

- (instancetype)initWithDevice:(device::BluetoothClassicDeviceMac*)device {
  if ((self = [super init])) {
    _device = device;

    _disconnectNotification = [device->device()
        registerForDisconnectNotification:self
                                 selector:@selector(deviceDisconnected:
                                                                device:)];
    if (!_disconnectNotification) {
      BLUETOOTH_LOG(ERROR) << "Failed to register for disconnect notification!";
    }
  }
  return self;
}

- (void)deviceDisconnected:(IOBluetoothUserNotification*)notification
                    device:(IOBluetoothDevice*)device {
  _device->OnDeviceDisconnected();
}

- (void)stopListening {
  [_disconnectNotification unregister];
}

@end

namespace device {
namespace {

const char kApiUnavailable[] = "This API is not implemented on this platform.";

BluetoothUUID GetUuid(IOBluetoothSDPUUID* sdp_uuid) {
  DCHECK(sdp_uuid);

  const uint8_t* uuid_bytes =
      reinterpret_cast<const uint8_t*>([sdp_uuid bytes]);
  std::string uuid_str = base::HexEncode(uuid_bytes, 16);
  DCHECK_EQ(uuid_str.size(), 32U);
  uuid_str.insert(8, "-");
  uuid_str.insert(13, "-");
  uuid_str.insert(18, "-");
  uuid_str.insert(23, "-");

  return BluetoothUUID(uuid_str);
}

// Returns the first (should be, only) UUID contained within the
// |service_class_data|. Returns an invalid (empty) UUID if none is found.
BluetoothUUID ExtractUuid(IOBluetoothSDPDataElement* service_class_data) {
  NSArray* inner_elements = [service_class_data getArrayValue];
  for (IOBluetoothSDPDataElement* inner_element in inner_elements) {
    if ([inner_element getTypeDescriptor] == kBluetoothSDPDataElementTypeUUID) {
      return GetUuid([[inner_element getUUIDValue] getUUIDWithLength:16]);
    }
  }

  return BluetoothUUID();
}

BluetoothDevice::UUIDList GetUuids(IOBluetoothDevice* device) {
  BluetoothDevice::UUIDList uuids;
  for (IOBluetoothSDPServiceRecord* service_record in [device services]) {
    IOBluetoothSDPDataElement* service_class_data =
        [service_record getAttributeDataElement:
                            kBluetoothSDPAttributeIdentifierServiceClassIDList];
    auto type_descriptor = [service_class_data getTypeDescriptor];
    if (type_descriptor == kBluetoothSDPDataElementTypeUUID) {
      IOBluetoothSDPUUID* sdp_uuid =
          [[service_class_data getUUIDValue] getUUIDWithLength:16];
      BluetoothUUID uuid = GetUuid(sdp_uuid);
      if (uuid.IsValid()) {
        uuids.push_back(uuid);
      }
    } else if (type_descriptor ==
               kBluetoothSDPDataElementTypeDataElementSequence) {
      BluetoothUUID uuid = ExtractUuid(service_class_data);
      if (uuid.IsValid()) {
        uuids.push_back(uuid);
      }
    }
  }
  return uuids;
}

}  // namespace

BluetoothClassicDeviceMac::BluetoothClassicDeviceMac(
    BluetoothAdapterMac* adapter,
    IOBluetoothDevice* device)
    : BluetoothDeviceMac(adapter), device_(device) {
  device_uuids_.ReplaceServiceUUIDs(GetUuids(device_));
  UpdateTimestamp();
}

BluetoothClassicDeviceMac::~BluetoothClassicDeviceMac() {
  [disconnect_listener_ stopListening];
  disconnect_listener_ = nil;
}

uint32_t BluetoothClassicDeviceMac::GetBluetoothClass() const {
  return [device_ classOfDevice];
}

void BluetoothClassicDeviceMac::CreateGattConnectionImpl(
    std::optional<BluetoothUUID> service_uuid) {
  // Classic devices do not support GATT connection.
  DidConnectGatt(ERROR_UNSUPPORTED_DEVICE);
}

void BluetoothClassicDeviceMac::DisconnectGatt() {}

std::string BluetoothClassicDeviceMac::GetAddress() const {
  return GetDeviceAddress(device_);
}

BluetoothDevice::AddressType BluetoothClassicDeviceMac::GetAddressType() const {
  NOTIMPLEMENTED();
  return ADDR_TYPE_UNKNOWN;
}

BluetoothDevice::VendorIDSource BluetoothClassicDeviceMac::GetVendorIDSource()
    const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothClassicDeviceMac::GetVendorID() const {
  return 0;
}

uint16_t BluetoothClassicDeviceMac::GetProductID() const {
  return 0;
}

uint16_t BluetoothClassicDeviceMac::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothClassicDeviceMac::GetAppearance() const {
  // TODO(crbug.com/41240161): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

std::optional<std::string> BluetoothClassicDeviceMac::GetName() const {
  if ([device_ name])
    return base::SysNSStringToUTF8([device_ name]);
  return std::nullopt;
}

bool BluetoothClassicDeviceMac::IsPaired() const {
  return [device_ isPaired];
}

bool BluetoothClassicDeviceMac::IsConnected() const {
  return [device_ isConnected];
}

bool BluetoothClassicDeviceMac::IsGattConnected() const {
  return false;  // Classic devices do not support GATT connection.
}

bool BluetoothClassicDeviceMac::IsConnectable() const {
  return false;
}

bool BluetoothClassicDeviceMac::IsConnecting() const {
  return false;
}

std::optional<int8_t> BluetoothClassicDeviceMac::GetInquiryRSSI() const {
  return std::nullopt;
}

std::optional<int8_t> BluetoothClassicDeviceMac::GetInquiryTxPower() const {
  return std::nullopt;
}

bool BluetoothClassicDeviceMac::ExpectingPinCode() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothClassicDeviceMac::ExpectingPasskey() const {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothClassicDeviceMac::ExpectingConfirmation() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothClassicDeviceMac::GetConnectionInfo(
    ConnectionInfoCallback callback) {
  ConnectionInfo connection_info;
  if (![device_ isConnected]) {
    std::move(callback).Run(connection_info);
    return;
  }

  connection_info.rssi = [device_ rawRSSI];
  // The API guarantees that +127 is returned in case the RSSI is not readable:
  // http://goo.gl/bpURYv
  if (connection_info.rssi == 127)
    connection_info.rssi = kUnknownPower;

  connection_info.transmit_power =
      GetHostTransmitPower(kReadCurrentTransmitPowerLevel);
  connection_info.max_transmit_power =
      GetHostTransmitPower(kReadMaximumTransmitPowerLevel);

  std::move(callback).Run(connection_info);
}

void BluetoothClassicDeviceMac::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::Connect(PairingDelegate* pairing_delegate,
                                        ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::Disconnect(base::OnceClosure callback,
                                           ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::Forget(base::OnceClosure callback,
                                       ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothClassicDeviceMac::ConnectToService(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->Connect(device_, uuid, base::BindOnce(std::move(callback), socket),
                  std::move(error_callback));
}

void BluetoothClassicDeviceMac::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  std::move(error_callback).Run(kApiUnavailable);
}

base::Time BluetoothClassicDeviceMac::GetLastUpdateTime() const {
  // getLastInquiryUpdate returns nil unpredictably so just use the
  // cross platform implementation of last update time.
  return last_update_time_;
}

int BluetoothClassicDeviceMac::GetHostTransmitPower(
    BluetoothHCITransmitPowerLevelType power_level_type) const {
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  // Bail if the undocumented API is unavailable on this machine.
  SEL selector = @selector(BluetoothHCIReadTransmitPowerLevel:
                                                       inType:
                                        outTransmitPowerLevel:);
  if (![controller respondsToSelector:selector])
    return kUnknownPower;

  BluetoothHCITransmitPowerLevel power_level;
  IOReturn result =
      [controller BluetoothHCIReadTransmitPowerLevel:[device_ connectionHandle]
                                              inType:power_level_type
                               outTransmitPowerLevel:&power_level];
  if (result != kIOReturnSuccess)
    return kUnknownPower;

  return power_level;
}

// static
std::string BluetoothClassicDeviceMac::GetDeviceAddress(
    IOBluetoothDevice* device) {
  return CanonicalizeBluetoothAddress(
      base::SysNSStringToUTF8([device addressString]));
}

bool BluetoothClassicDeviceMac::IsLowEnergyDevice() {
  return false;
}

void BluetoothClassicDeviceMac::OnDeviceDisconnected() {
  BLUETOOTH_LOG(EVENT) << "Device disconnected: name: "
                       << this->GetNameForDisplay()
                       << " address: " << this->GetAddress();
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothClassicDeviceMac::StartListeningDisconnectEvent() {
  if (!device_ || disconnect_listener_) {
    return;
  }
  disconnect_listener_ =
      [[BluetoothDeviceDisconnectListener alloc] initWithDevice:this];
}

}  // namespace device

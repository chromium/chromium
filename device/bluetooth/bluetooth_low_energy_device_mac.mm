// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#include <stddef.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"
#include "device/bluetooth/bluetooth_low_energy_peripheral_delegate.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace device {

BluetoothLowEnergyDeviceMac::BluetoothLowEnergyDeviceMac(
    BluetoothAdapter* adapter,
    CBPeripheral* peripheral)
    : BluetoothDeviceMac(adapter),
      peripheral_(peripheral),
      connected_(false),
      discovery_pending_count_(0) {
  DCHECK(peripheral_);
  peripheral_delegate_ = [[BluetoothLowEnergyPeripheralDelegate alloc]
      initWithBluetoothLowEnergyDeviceMac:this];
  [peripheral_ setDelegate:peripheral_delegate_];
  identifier_ = GetPeripheralIdentifier(peripheral);
  hash_address_ = GetPeripheralHashAddress(peripheral);
  UpdateTimestamp();
}

BluetoothLowEnergyDeviceMac::~BluetoothLowEnergyDeviceMac() {
  if (IsGattConnected()) {
    GetLowEnergyAdapter()->DisconnectGatt(this);
  }

  [peripheral_ setDelegate:nil];
}

std::string BluetoothLowEnergyDeviceMac::GetIdentifier() const {
  return identifier_;
}

uint32_t BluetoothLowEnergyDeviceMac::GetBluetoothClass() const {
  return 0x1F00;  // Unspecified Device Class
}

std::string BluetoothLowEnergyDeviceMac::GetAddress() const {
  return hash_address_;
}

BluetoothDevice::AddressType BluetoothLowEnergyDeviceMac::GetAddressType()
    const {
  NOTIMPLEMENTED();
  return ADDR_TYPE_UNKNOWN;
}

BluetoothDevice::VendorIDSource BluetoothLowEnergyDeviceMac::GetVendorIDSource()
    const {
  return VENDOR_ID_UNKNOWN;
}

uint16_t BluetoothLowEnergyDeviceMac::GetVendorID() const {
  return 0;
}

uint16_t BluetoothLowEnergyDeviceMac::GetProductID() const {
  return 0;
}

uint16_t BluetoothLowEnergyDeviceMac::GetDeviceID() const {
  return 0;
}

uint16_t BluetoothLowEnergyDeviceMac::GetAppearance() const {
  // TODO(crbug.com/41240161): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

std::optional<std::string> BluetoothLowEnergyDeviceMac::GetName() const {
  if ([peripheral_ name])
    return base::SysNSStringToUTF8([peripheral_ name]);
  return std::nullopt;
}

bool BluetoothLowEnergyDeviceMac::IsPaired() const {
  return GetLowEnergyAdapter()->IsBluetoothLowEnergyDeviceSystemPaired(
      identifier_);
}

bool BluetoothLowEnergyDeviceMac::IsConnected() const {
  return IsGattConnected();
}

bool BluetoothLowEnergyDeviceMac::IsGattConnected() const {
  // |connected_| can be false while |[peripheral_ state]| is
  // |CBPeripheralStateConnected|. This happens
  // BluetoothLowEnergyAdapterApple::DidConnectPeripheral() is called and
  // BluetoothLowEnergyDeviceMac::DidConnectGatt() has not been called yet.
  return connected_;
}

bool BluetoothLowEnergyDeviceMac::IsConnectable() const {
  // Only available for Chrome OS.
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothLowEnergyDeviceMac::IsConnecting() const {
  return ([peripheral_ state] == CBPeripheralStateConnecting);
}

bool BluetoothLowEnergyDeviceMac::ExpectingPinCode() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::ExpectingPasskey() const {
  return false;
}

bool BluetoothLowEnergyDeviceMac::ExpectingConfirmation() const {
  return false;
}

void BluetoothLowEnergyDeviceMac::GetConnectionInfo(
    ConnectionInfoCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetConnectionLatency(
    ConnectionLatency connection_latency,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Connect(PairingDelegate* pairing_delegate,
                                          ConnectCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetPinCode(const std::string& pincode) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetPasskey(uint32_t passkey) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConfirmPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::RejectPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::CancelPairing() {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Disconnect(base::OnceClosure callback,
                                             ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Forget(base::OnceClosure callback,
                                         ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToService(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    ConnectToServiceCallback callback,
    ConnectToServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothLowEnergyDeviceMac::IsLowEnergyDevice() {
  return true;
}

void BluetoothLowEnergyDeviceMac::CreateGattConnectionImpl(
    std::optional<BluetoothUUID> serivce_uuid) {
  if (!IsGattConnected()) {
    GetLowEnergyAdapter()->CreateGattConnection(this);
  }
}

void BluetoothLowEnergyDeviceMac::DisconnectGatt() {
  GetLowEnergyAdapter()->DisconnectGatt(this);
}

void BluetoothLowEnergyDeviceMac::DidDiscoverPrimaryServices(NSError* error) {
  --discovery_pending_count_;
  if (discovery_pending_count_ < 0) {
    // This should never happen, just in case it happens with a device,
    // discovery_pending_count_ is set back to 0.
    DVLOG(1) << *this
             << ": BluetoothLowEnergyDeviceMac::discovery_pending_count_ "
             << discovery_pending_count_;
    discovery_pending_count_ = 0;
    return;
  }
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    // a device services.
    DVLOG(1) << *this << ": Can't discover primary services: "
             << BluetoothLowEnergyAdapterApple::String(error);
    return;
  }

  if (!IsGattConnected()) {
    // Don't create services if the device disconnected.
    DVLOG(1) << *this << ": DidDiscoverPrimaryServices, gatt not connected.";
    return;
  }
  DVLOG(1) << *this << ": DidDiscoverPrimaryServices, pending count: "
           << discovery_pending_count_;

  for (CBService* cb_service in GetPeripheral().services) {
    BluetoothRemoteGattServiceMac* gatt_service =
        GetBluetoothRemoteGattServiceMac(cb_service);
    if (!gatt_service) {
      gatt_service = new BluetoothRemoteGattServiceMac(this, cb_service,
                                                       true /* is_primary */);
      auto result_iter = gatt_services_.insert(std::make_pair(
          gatt_service->GetIdentifier(), base::WrapUnique(gatt_service)));
      DCHECK(result_iter.second);
      DVLOG(1) << *gatt_service << ": New service.";
      adapter_->NotifyGattServiceAdded(gatt_service);
    } else {
      DVLOG(1) << *gatt_service << ": Known service.";
    }
  }
  if (discovery_pending_count_ == 0) {
    for (auto& it : gatt_services_) {
      BluetoothRemoteGattService* gatt_service = it.second.get();
      BluetoothRemoteGattServiceMac* gatt_service_mac =
          static_cast<BluetoothRemoteGattServiceMac*>(gatt_service);
      gatt_service_mac->DiscoverCharacteristics();
    }
    SendNotificationIfDiscoveryComplete();
  }
}

void BluetoothLowEnergyDeviceMac::DidDiscoverCharacteristics(
    CBService* cb_service,
    NSError* error) {
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    DVLOG(1) << *this << ": Can't discover characteristics: "
             << BluetoothLowEnergyAdapterApple::String(error);
    return;
  }

  if (!IsGattConnected()) {
    DVLOG(1) << *this << ": DidDiscoverCharacteristics, gatt disconnected.";
    // Don't create characteristics if the device disconnected.
    return;
  }
  if (IsGattServicesDiscoveryComplete()) {
    // This should never happen, just in case it happens with a device, this
    // notification should be ignored.
    DVLOG(1) << *this
             << ": Discovery complete, ignoring DidDiscoverCharacteristics.";
    return;
  }

  BluetoothRemoteGattServiceMac* gatt_service =
      GetBluetoothRemoteGattServiceMac(cb_service);
  DCHECK(gatt_service);
  gatt_service->DidDiscoverCharacteristics();
  SendNotificationIfDiscoveryComplete();
}

void BluetoothLowEnergyDeviceMac::DidModifyServices(
    NSArray* invalidatedServices) {
  DVLOG(1) << *this << ": DidModifyServices: "
           << " invalidated services "
           << base::SysNSStringToUTF8([invalidatedServices description]);
  for (CBService* cb_service in invalidatedServices) {
    BluetoothRemoteGattServiceMac* gatt_service =
        GetBluetoothRemoteGattServiceMac(cb_service);
    DCHECK(gatt_service);
    DVLOG(1) << gatt_service->GetUUID().canonical_value();
    std::unique_ptr<BluetoothRemoteGattService> scoped_service =
        std::move(gatt_services_[gatt_service->GetIdentifier()]);
    gatt_services_.erase(gatt_service->GetIdentifier());
    adapter_->NotifyGattServiceRemoved(scoped_service.get());
  }
  device_uuids_.ClearServiceUUIDs();
  SetGattServicesDiscoveryComplete(false);
  adapter_->NotifyDeviceChanged(this);
  DiscoverPrimaryServices();
}

void BluetoothLowEnergyDeviceMac::DidUpdateValue(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic_mac =
      GetBluetoothRemoteGattCharacteristicMac(characteristic);
  DCHECK(gatt_characteristic_mac);
  gatt_characteristic_mac->DidUpdateValue(error);
}

void BluetoothLowEnergyDeviceMac::DidWriteValue(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic_mac =
      GetBluetoothRemoteGattCharacteristicMac(characteristic);
  DCHECK(gatt_characteristic_mac);
  gatt_characteristic_mac->DidWriteValue(error);
}

void BluetoothLowEnergyDeviceMac::DidUpdateNotificationState(
    CBCharacteristic* characteristic,
    NSError* error) {
  BluetoothRemoteGattCharacteristicMac* gatt_characteristic_mac =
      GetBluetoothRemoteGattCharacteristicMac(characteristic);
  DCHECK(gatt_characteristic_mac);
  gatt_characteristic_mac->DidUpdateNotificationState(error);
}

void BluetoothLowEnergyDeviceMac::DidDiscoverDescriptors(
    CBCharacteristic* cb_characteristic,
    NSError* error) {
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    DVLOG(1) << *this << ": Can't discover descriptors: "
             << BluetoothLowEnergyAdapterApple::String(error);
    return;
  }
  if (!IsGattConnected()) {
    DVLOG(1) << *this << ": DidDiscoverDescriptors, disconnected.";
    // Don't discover descriptors if the device disconnected.
    return;
  }
  if (IsGattServicesDiscoveryComplete()) {
    // This should never happen, just in case it happens with a device, this
    // notification should be ignored.
    DVLOG(1) << *this
             << ": Discovery complete, ignoring DidDiscoverDescriptors.";
    return;
  }
  BluetoothRemoteGattServiceMac* gatt_service =
      GetBluetoothRemoteGattServiceMac(cb_characteristic.service);
  DCHECK(gatt_service);
  gatt_service->DidDiscoverDescriptors(cb_characteristic);
  SendNotificationIfDiscoveryComplete();
}

void BluetoothLowEnergyDeviceMac::DidUpdateValueForDescriptor(
    CBDescriptor* cb_descriptor,
    NSError* error) {
  BluetoothRemoteGattDescriptorMac* gatt_descriptor =
      GetBluetoothRemoteGattDescriptorMac(cb_descriptor);
  DCHECK(gatt_descriptor);
  gatt_descriptor->DidUpdateValueForDescriptor(error);
}

void BluetoothLowEnergyDeviceMac::DidWriteValueForDescriptor(
    CBDescriptor* cb_descriptor,
    NSError* error) {
  BluetoothRemoteGattDescriptorMac* gatt_descriptor =
      GetBluetoothRemoteGattDescriptorMac(cb_descriptor);
  DCHECK(gatt_descriptor);
  gatt_descriptor->DidWriteValueForDescriptor(error);
}

// static
std::string BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(
    CBPeripheral* peripheral) {
  NSUUID* uuid = [peripheral identifier];
  NSString* uuidString = [uuid UUIDString];
  return base::SysNSStringToUTF8(uuidString);
}

// static
std::string BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(
    CBPeripheral* peripheral) {
  return GetPeripheralHashAddress(GetPeripheralIdentifier(peripheral));
}

// static
std::string BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(
    std::string_view device_identifier) {
  const size_t kCanonicalAddressNumberOfBytes = 6;
  uint8_t raw[kCanonicalAddressNumberOfBytes];
  crypto::SHA256HashString(device_identifier, raw, sizeof(raw));
  return CanonicalizeBluetoothAddress(base::HexEncode(raw));
}

void BluetoothLowEnergyDeviceMac::DidConnectPeripheral() {
  DVLOG(1) << *this << ": GATT connected.";
  if (!connected_) {
    connected_ = true;
    DidConnectGatt(/*error_code=*/std::nullopt);
    DiscoverPrimaryServices();
  } else {
    // -[<CBCentralManagerDelegate> centralManager:didConnectPeripheral:] can be
    // called twice because of a macOS bug. This second call should be ignored.
    // See crbug.com/681414.
    DVLOG(1) << *this << ": Already connected, ignoring event.";
  }
}

void BluetoothLowEnergyDeviceMac::DiscoverPrimaryServices() {
  DVLOG(1) << *this << ": DiscoverPrimaryServices, pending count "
           << discovery_pending_count_;
  ++discovery_pending_count_;
  [GetPeripheral() discoverServices:nil];
}

void BluetoothLowEnergyDeviceMac::SendNotificationIfDiscoveryComplete() {
  DCHECK(!IsGattServicesDiscoveryComplete());
  // Notify when all services have been discovered.
  bool discovery_complete =
      discovery_pending_count_ == 0 &&
      base::ranges::all_of(
          gatt_services_, [](GattServiceMap::value_type& pair) {
            BluetoothRemoteGattService* gatt_service = pair.second.get();
            return static_cast<BluetoothRemoteGattServiceMac*>(gatt_service)
                ->IsDiscoveryComplete();
          });
  if (discovery_complete) {
    DVLOG(1) << *this << ": Discovery complete.";
    device_uuids_.ReplaceServiceUUIDs(gatt_services_);
    SetGattServicesDiscoveryComplete(true);
    adapter_->NotifyGattServicesDiscovered(this);
    adapter_->NotifyDeviceChanged(this);
  }
}

BluetoothLowEnergyAdapterApple*
BluetoothLowEnergyDeviceMac::GetLowEnergyAdapter() {
  return static_cast<BluetoothLowEnergyAdapterApple*>(this->adapter_);
}

BluetoothLowEnergyAdapterApple*
BluetoothLowEnergyDeviceMac::GetLowEnergyAdapter() const {
  return static_cast<BluetoothLowEnergyAdapterApple*>(this->adapter_);
}

CBPeripheral* BluetoothLowEnergyDeviceMac::GetPeripheral() {
  return peripheral_;
}

BluetoothRemoteGattServiceMac*
BluetoothLowEnergyDeviceMac::GetBluetoothRemoteGattServiceMac(
    CBService* cb_service) const {
  for (const auto& it : gatt_services_) {
    BluetoothRemoteGattService* gatt_service = it.second.get();
    BluetoothRemoteGattServiceMac* gatt_service_mac =
        static_cast<BluetoothRemoteGattServiceMac*>(gatt_service);
    if (gatt_service_mac->GetService() == cb_service)
      return gatt_service_mac;
  }
  return nullptr;
}

BluetoothRemoteGattCharacteristicMac*
BluetoothLowEnergyDeviceMac::GetBluetoothRemoteGattCharacteristicMac(
    CBCharacteristic* cb_characteristic) const {
  CBService* cb_service = [cb_characteristic service];
  BluetoothRemoteGattServiceMac* gatt_service_mac =
      GetBluetoothRemoteGattServiceMac(cb_service);
  if (!gatt_service_mac) {
    return nullptr;
  }
  return gatt_service_mac->GetBluetoothRemoteGattCharacteristicMac(
      cb_characteristic);
}

BluetoothRemoteGattDescriptorMac*
BluetoothLowEnergyDeviceMac::GetBluetoothRemoteGattDescriptorMac(
    CBDescriptor* cb_descriptor) const {
  CBService* cb_service = [[cb_descriptor characteristic] service];
  BluetoothRemoteGattServiceMac* gatt_service_mac =
      GetBluetoothRemoteGattServiceMac(cb_service);
  if (!gatt_service_mac) {
    return nullptr;
  }
  return gatt_service_mac->GetBluetoothRemoteGattDescriptorMac(cb_descriptor);
}

void BluetoothLowEnergyDeviceMac::DidDisconnectPeripheral(NSError* error) {
  connected_ = false;
  DVLOG(1) << *this << ": Disconnected from peripheral.";
  if (error) {
    DVLOG(1) << *this << ": Bluetooth error: "
             << BluetoothLowEnergyAdapterApple::String(error);
  }
  SetGattServicesDiscoveryComplete(false);
  // Removing all services at once to ensure that calling GetGattService on
  // removed service in GattServiceRemoved returns null.
  GattServiceMap gatt_services_swapped;
  gatt_services_swapped.swap(gatt_services_);
  gatt_services_swapped.clear();
  device_uuids_.ClearServiceUUIDs();
  // There are two cases in which this function will be called:
  //   1. When the connection to the device breaks (either because
  //      we closed it or the device closed it).
  //   2. When we cancel a pending connection request.
  if (create_gatt_connection_callbacks_.empty()) {
    // If there are no pending callbacks then the connection broke (#1).
    DidDisconnectGatt();
    return;
  }
  // Else we canceled the connection request (#2).
  // TODO(http://crbug.com/585897): Need to pass the error.
  DidConnectGatt(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
}

std::ostream& operator<<(std::ostream& out,
                         const BluetoothLowEnergyDeviceMac& device) {
  // TODO(crbug.com/40511884): Should use
  // BluetoothLowEnergyDeviceMac::GetNameForDisplay() instead.
  std::optional<std::string> name = device.GetName();
  const char* is_gatt_connected =
      device.IsGattConnected() ? "GATT connected" : "GATT disconnected";
  return out << "<BluetoothLowEnergyDeviceMac " << device.GetAddress() << "/"
             << &device << ", " << is_gatt_connected << ", \""
             << name.value_or("Unnamed device") << "\">";
}

}  // namespace device

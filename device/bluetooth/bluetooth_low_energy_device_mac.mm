// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_device_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_mac.h"
#include "device/bluetooth/bluetooth_adapter_mac_metrics.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_low_energy_peripheral_delegate.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor_mac.h"
#include "device/bluetooth/bluetooth_remote_gatt_service_mac.h"

// Remove when Chrome no longer supports 10.12.
#if defined(MAC_OS_X_VERSION_10_13)

// In the 10.13 SDK, CBPeripheral became a subclass of CBPeer, which defines
// -[CBPeer identifier] as partially available. Pretend it still exists on
// CBPeripheral. At runtime the implementation on CBPeer will be invoked.
@interface CBPeripheral (HighSierraSDK)
@property(readonly, nonatomic) NSUUID* identifier;
@end

#endif  // MAC_OS_X_VERSION_10_13

namespace device {

BluetoothLowEnergyDeviceMac::BluetoothLowEnergyDeviceMac(
    BluetoothAdapterMac* adapter,
    CBPeripheral* peripheral)
    : BluetoothDeviceMac(adapter),
      peripheral_(peripheral, base::scoped_policy::RETAIN),
      connected_(false),
      discovery_pending_count_(0) {
  DCHECK(peripheral_);
  peripheral_delegate_.reset([[BluetoothLowEnergyPeripheralDelegate alloc]
      initWithBluetoothLowEnergyDeviceMac:this]);
  [peripheral_ setDelegate:peripheral_delegate_];
  identifier_ = GetPeripheralIdentifier(peripheral);
  hash_address_ = GetPeripheralHashAddress(peripheral);
  UpdateTimestamp();
}

BluetoothLowEnergyDeviceMac::~BluetoothLowEnergyDeviceMac() {
  if (IsGattConnected()) {
    GetMacAdapter()->DisconnectGatt(this);
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
  // TODO(crbug.com/588083): Implementing GetAppearance()
  // on mac, win, and android platforms for chrome
  NOTIMPLEMENTED();
  return 0;
}

base::Optional<std::string> BluetoothLowEnergyDeviceMac::GetName() const {
  if ([peripheral_ name])
    return base::SysNSStringToUTF8([peripheral_ name]);
  return base::nullopt;
}

bool BluetoothLowEnergyDeviceMac::IsPaired() const {
  return GetMacAdapter()->IsBluetoothLowEnergyDeviceSystemPaired(identifier_);
}

bool BluetoothLowEnergyDeviceMac::IsConnected() const {
  return IsGattConnected();
}

bool BluetoothLowEnergyDeviceMac::IsGattConnected() const {
  // |connected_| can be false while |[peripheral_ state]| is
  // |CBPeripheralStateConnected|. This happens
  // BluetoothAdapterMac::DidConnectPeripheral() is called and
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
    const ConnectionInfoCallback& callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::SetConnectionLatency(
    ConnectionLatency connection_latency,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Connect(
    PairingDelegate* pairing_delegate,
    const base::Closure& callback,
    const ConnectErrorCallback& error_callback) {
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

void BluetoothLowEnergyDeviceMac::Disconnect(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::Forget(const base::Closure& callback,
                                         const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToService(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::ConnectToServiceInsecurely(
    const BluetoothUUID& uuid,
    const ConnectToServiceCallback& callback,
    const ConnectToServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothLowEnergyDeviceMac::CreateGattConnectionImpl() {
  if (!IsGattConnected()) {
    GetMacAdapter()->CreateGattConnection(this);
  }
}

void BluetoothLowEnergyDeviceMac::DisconnectGatt() {
  GetMacAdapter()->DisconnectGatt(this);
}

void BluetoothLowEnergyDeviceMac::DidDiscoverPrimaryServices(NSError* error) {
  --discovery_pending_count_;
  if (discovery_pending_count_ < 0) {
    // This should never happen, just in case it happens with a device,
    // discovery_pending_count_ is set back to 0.
    VLOG(1) << *this
            << ": BluetoothLowEnergyDeviceMac::discovery_pending_count_ "
            << discovery_pending_count_;
    discovery_pending_count_ = 0;
    return;
  }
  RecordDidDiscoverPrimaryServicesResult(error);
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    // a device services.
    VLOG(1) << *this << ": Can't discover primary services: "
            << BluetoothAdapterMac::String(error);
    return;
  }

  if (!IsGattConnected()) {
    // Don't create services if the device disconnected.
    VLOG(1) << *this << ": DidDiscoverPrimaryServices, gatt not connected.";
    return;
  }
  VLOG(1) << *this << ": DidDiscoverPrimaryServices, pending count: "
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
      VLOG(1) << *gatt_service << ": New service.";
      adapter_->NotifyGattServiceAdded(gatt_service);
    } else {
      VLOG(1) << *gatt_service << ": Known service.";
    }
  }
  if (discovery_pending_count_ == 0) {
    for (auto it = gatt_services_.begin(); it != gatt_services_.end(); ++it) {
      BluetoothRemoteGattService* gatt_service = it->second.get();
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
  RecordDidDiscoverCharacteristicsResult(error);
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    VLOG(1) << *this << ": Can't discover characteristics: "
            << BluetoothAdapterMac::String(error);
    return;
  }

  if (!IsGattConnected()) {
    VLOG(1) << *this << ": DidDiscoverCharacteristics, gatt disconnected.";
    // Don't create characteristics if the device disconnected.
    return;
  }
  if (IsGattServicesDiscoveryComplete()) {
    // This should never happen, just in case it happens with a device, this
    // notification should be ignored.
    VLOG(1) << *this
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
  VLOG(1) << *this << ": DidModifyServices: "
          << " invalidated services "
          << base::SysNSStringToUTF8([invalidatedServices description]);
  for (CBService* cb_service in invalidatedServices) {
    BluetoothRemoteGattServiceMac* gatt_service =
        GetBluetoothRemoteGattServiceMac(cb_service);
    DCHECK(gatt_service);
    VLOG(1) << gatt_service->GetUUID().canonical_value();
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
  RecordDidDiscoverDescriptorsResult(error);
  if (error) {
    // TODO(http://crbug.com/609320): Need to pass the error.
    // TODO(http://crbug.com/609844): Decide what to do if discover failed
    VLOG(1) << *this << ": Can't discover descriptors: "
            << BluetoothAdapterMac::String(error);
    return;
  }
  if (!IsGattConnected()) {
    VLOG(1) << *this << ": DidDiscoverDescriptors, disconnected.";
    // Don't discover descriptors if the device disconnected.
    return;
  }
  if (IsGattServicesDiscoveryComplete()) {
    // This should never happen, just in case it happens with a device, this
    // notification should be ignored.
    VLOG(1) << *this
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
    base::StringPiece device_identifier) {
  const size_t kCanonicalAddressNumberOfBytes = 6;
  char raw[kCanonicalAddressNumberOfBytes];
  crypto::SHA256HashString(device_identifier, raw, sizeof(raw));
  return BluetoothDevice::CanonicalizeAddress(
      base::HexEncode(raw, sizeof(raw)));
}

void BluetoothLowEnergyDeviceMac::DidConnectPeripheral() {
  VLOG(1) << *this << ": GATT connected.";
  if (!connected_) {
    connected_ = true;
    DidConnectGatt();
    DiscoverPrimaryServices();
  } else {
    // -[<CBCentralManagerDelegate> centralManager:didConnectPeripheral:] can be
    // called twice because of a macOS bug. This second call should be ignored.
    // See crbug.com/681414.
    VLOG(1) << *this << ": Already connected, ignoring event.";
  }
}

void BluetoothLowEnergyDeviceMac::DiscoverPrimaryServices() {
  VLOG(1) << *this << ": DiscoverPrimaryServices, pending count "
          << discovery_pending_count_;
  ++discovery_pending_count_;
  [GetPeripheral() discoverServices:nil];
}

void BluetoothLowEnergyDeviceMac::SendNotificationIfDiscoveryComplete() {
  DCHECK(!IsGattServicesDiscoveryComplete());
  // Notify when all services have been discovered.
  bool discovery_complete =
      discovery_pending_count_ == 0 &&
      std::find_if_not(
          gatt_services_.begin(),
          gatt_services_.end(), [](GattServiceMap::value_type & pair) {
            BluetoothRemoteGattService* gatt_service = pair.second.get();
            return static_cast<BluetoothRemoteGattServiceMac*>(gatt_service)
                ->IsDiscoveryComplete();
          }) == gatt_services_.end();
  if (discovery_complete) {
    VLOG(1) << *this << ": Discovery complete.";
    device_uuids_.ReplaceServiceUUIDs(gatt_services_);
    SetGattServicesDiscoveryComplete(true);
    adapter_->NotifyGattServicesDiscovered(this);
    adapter_->NotifyDeviceChanged(this);
  }
}

BluetoothAdapterMac* BluetoothLowEnergyDeviceMac::GetMacAdapter() {
  return static_cast<BluetoothAdapterMac*>(this->adapter_);
}

BluetoothAdapterMac* BluetoothLowEnergyDeviceMac::GetMacAdapter() const {
  return static_cast<BluetoothAdapterMac*>(this->adapter_);
}

CBPeripheral* BluetoothLowEnergyDeviceMac::GetPeripheral() {
  return peripheral_;
}

BluetoothRemoteGattServiceMac*
BluetoothLowEnergyDeviceMac::GetBluetoothRemoteGattServiceMac(
    CBService* cb_service) const {
  for (auto it = gatt_services_.begin(); it != gatt_services_.end(); ++it) {
    BluetoothRemoteGattService* gatt_service = it->second.get();
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
  VLOG(1) << *this << ": Disconnected from peripheral.";
  RecordDidDisconnectPeripheralResult(error);
  if (error) {
    VLOG(1) << *this
            << ": Bluetooth error: " << BluetoothAdapterMac::String(error);
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
  if (create_gatt_connection_error_callbacks_.empty()) {
    // If there are no pending callbacks then the connection broke (#1).
    DidDisconnectGatt();
    return;
  }
  // Else we canceled the connection request (#2).
  // TODO(http://crbug.com/585897): Need to pass the error.
  DidFailToConnectGatt(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
}

std::ostream& operator<<(std::ostream& out,
                         const BluetoothLowEnergyDeviceMac& device) {
  // TODO(crbug.com/703878): Should use
  // BluetoothLowEnergyDeviceMac::GetNameForDisplay() instead.
  base::Optional<std::string> name = device.GetName();
  const char* is_gatt_connected =
      device.IsGattConnected() ? "GATT connected" : "GATT disconnected";
  return out << "<BluetoothLowEnergyDeviceMac " << device.GetAddress() << "/"
             << &device << ", " << is_gatt_connected << ", \""
             << name.value_or("Unnamed device") << "\">";
}

}  // namespace device

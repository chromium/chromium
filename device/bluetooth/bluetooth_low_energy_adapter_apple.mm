// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/bluetooth_low_energy_adapter_apple.h"

#import <CoreBluetooth/CBManager.h>
#include <CoreFoundation/CFNumber.h>
#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_advertisement_mac.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_low_energy_central_manager_delegate.h"
#include "device/bluetooth/bluetooth_low_energy_device_watcher_mac.h"
#include "device/bluetooth/bluetooth_low_energy_peripheral_manager_delegate.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace device {

// static
BluetoothUUID BluetoothLowEnergyAdapterApple::BluetoothUUIDWithCBUUID(
    CBUUID* uuid) {
  std::string uuid_c_string = base::SysNSStringToUTF8([uuid UUIDString]);
  return device::BluetoothUUID(uuid_c_string);
}

// static
std::string BluetoothLowEnergyAdapterApple::String(NSError* error) {
  if (!error) {
    return "no error";
  }
  return std::string("error domain: ") + base::SysNSStringToUTF8(error.domain) +
         ", code: " + base::NumberToString(error.code) + ", description: " +
         base::SysNSStringToUTF8(error.localizedDescription);
}

BluetoothLowEnergyAdapterApple::BluetoothLowEnergyAdapterApple()
    : low_energy_discovery_manager_(
          BluetoothLowEnergyDiscoveryManagerMac::Create(this)),
      low_energy_advertisement_manager_(
          std::make_unique<BluetoothLowEnergyAdvertisementManagerMac>()),
      low_energy_central_manager_delegate_(
          [[BluetoothLowEnergyCentralManagerDelegate alloc]
              initWithDiscoveryManager:low_energy_discovery_manager_.get()
                            andAdapter:this]),
      low_energy_peripheral_manager_delegate_(
          [[BluetoothLowEnergyPeripheralManagerDelegate alloc]
              initWithAdvertisementManager:
                  low_energy_advertisement_manager_.get()]) {
  DCHECK(low_energy_discovery_manager_);
}

BluetoothLowEnergyAdapterApple::~BluetoothLowEnergyAdapterApple() {
  FlushRequestSystemPermissionCallbacks();
  // When devices will be destroyed, they will need this current instance to
  // disconnect the gatt connection. To make sure they don't use the mac
  // adapter, they should be explicitly destroyed here.
  devices_.clear();
  // Explicitly clear out delegates, which might outlive the Adapter.
  [low_energy_peripheral_manager_ setDelegate:nil];
  [low_energy_central_manager_ setDelegate:nil];
  // Set low_energy_central_manager_ to nil so no devices will try to use it
  // while being destroyed after this method. |devices_| is owned by
  // BluetoothAdapter.
  low_energy_central_manager_ = nil;
}

std::string BluetoothLowEnergyAdapterApple::GetAddress() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string BluetoothLowEnergyAdapterApple::GetName() const {
  NOTIMPLEMENTED();
  return std::string();
}

void BluetoothLowEnergyAdapterApple::SetName(const std::string& name,
                                             base::OnceClosure callback,
                                             ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothLowEnergyAdapterApple::IsInitialized() const {
  // Initialize() does nothing and the lazy initialization state is hidden.
  return true;
}

bool BluetoothLowEnergyAdapterApple::IsPresent() const {
  // CoreBluetooth doesn't have a state to obtain an address.
  return true;
}

BluetoothAdapter::PermissionStatus
BluetoothLowEnergyAdapterApple::GetOsPermissionStatus() const {
  switch (CBCentralManager.authorization) {
    case CBManagerAuthorizationNotDetermined:
      return PermissionStatus::kUndetermined;
    case CBManagerAuthorizationRestricted:
    case CBManagerAuthorizationDenied:
      return PermissionStatus::kDenied;
    case CBManagerAuthorizationAllowedAlways:
      return PermissionStatus::kAllowed;
  }
}

void BluetoothLowEnergyAdapterApple::RequestSystemPermission(
    BluetoothAdapter::RequestSystemPermissionCallback callback) {
  auto status = GetOsPermissionStatus();
  if (status == PermissionStatus::kUndetermined) {
    request_system_permission_callbacks_.push_back(std::move(callback));
    // Set up CBCentralManager for getting state update.
    if (!low_energy_central_manager_) {
      low_energy_central_manager_ = [[CBCentralManager alloc]
          initWithDelegate:low_energy_central_manager_delegate_
                     queue:dispatch_get_main_queue()];
    }
    TriggerSystemPermissionPrompt();
  } else {
    ui_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), status));
  }
}

bool BluetoothLowEnergyAdapterApple::IsPowered() const {
  const_cast<BluetoothLowEnergyAdapterApple*>(this)->LazyInitialize();
  return IsLowEnergyPowered();
}

// TODO(krstnmnlsn): If this information is retrievable form IOBluetooth we
// should return the discoverable status.
bool BluetoothLowEnergyAdapterApple::IsDiscoverable() const {
  return false;
}

void BluetoothLowEnergyAdapterApple::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothLowEnergyAdapterApple::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    CreateServiceCallback callback,
    CreateServiceErrorCallback error_callback) {
  NOTIMPLEMENTED();
  std::move(error_callback).Run("Not Implemented");
}

void BluetoothLowEnergyAdapterApple::SetDiscoverable(
    bool discoverable,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothLowEnergyAdapterApple::IsDiscovering() const {
  return low_energy_discovery_manager_->IsDiscovering();
}

std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
BluetoothLowEnergyAdapterApple::RetrieveGattConnectedDevicesWithDiscoveryFilter(
    const BluetoothDiscoveryFilter& discovery_filter) {
  LazyInitialize();

  std::unordered_map<BluetoothDevice*, BluetoothDevice::UUIDSet>
      connected_devices;
  std::set<device::BluetoothUUID> uuids;
  discovery_filter.GetUUIDs(uuids);
  if (uuids.empty()) {
    for (BluetoothDevice* device :
         RetrieveGattConnectedDevicesWithService(nullptr)) {
      connected_devices[device] = BluetoothDevice::UUIDSet();
    }
    return connected_devices;
  }
  for (const BluetoothUUID& uuid : uuids) {
    for (BluetoothDevice* device :
         RetrieveGattConnectedDevicesWithService(&uuid)) {
      connected_devices[device].insert(uuid);
    }
  }
  return connected_devices;
}

BluetoothAdapter::UUIDList BluetoothLowEnergyAdapterApple::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothLowEnergyAdapterApple::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    CreateAdvertisementCallback callback,
    AdvertisementErrorCallback error_callback) {
  LazyInitialize();
  low_energy_advertisement_manager_->RegisterAdvertisement(
      std::move(advertisement_data), std::move(callback),
      std::move(error_callback));
}

BluetoothLocalGattService* BluetoothLowEnergyAdapterApple::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

BluetoothAdapter::DeviceList BluetoothLowEnergyAdapterApple::GetDevices() {
  LazyInitialize();
  return BluetoothAdapter::GetDevices();
}

BluetoothAdapter::ConstDeviceList BluetoothLowEnergyAdapterApple::GetDevices()
    const {
  const_cast<BluetoothLowEnergyAdapterApple*>(this)->LazyInitialize();
  return BluetoothAdapter::GetDevices();
}

void BluetoothLowEnergyAdapterApple::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {}

void BluetoothLowEnergyAdapterApple::LazyInitialize() {
  if (lazy_initialized_) {
    return;
  }

  // `low_energy_central_manager_` can possibly be initialized earlier in
  // `RequestSystemPermission`.
  if (!low_energy_central_manager_) {
    low_energy_central_manager_ = [[CBCentralManager alloc]
        initWithDelegate:low_energy_central_manager_delegate_
                   queue:dispatch_get_main_queue()];
  }
  low_energy_discovery_manager_->SetCentralManager(low_energy_central_manager_);

  low_energy_peripheral_manager_ = [[CBPeripheralManager alloc]
      initWithDelegate:low_energy_peripheral_manager_delegate_
                 queue:dispatch_get_main_queue()];

  lazy_initialized_ = true;

  low_energy_advertisement_manager_->Init(ui_task_runner_,
                                          low_energy_peripheral_manager_);

  // To obtain list of low energy devices known to the system, we need to parse
  // and watch system property list file for paired device addresses.
  bluetooth_low_energy_device_watcher_ =
      BluetoothLowEnergyDeviceWatcherMac::CreateAndStartWatching(
          ui_task_runner_,
          base::BindRepeating(
              &BluetoothLowEnergyAdapterApple::UpdateKnownLowEnergyDevices,
              GetLowEnergyWeakPtr()));

  bluetooth_low_energy_device_watcher_->ReadBluetoothPropertyListFile();
}

void BluetoothLowEnergyAdapterApple::UpdateKnownLowEnergyDevices(
    DevicesInfo updated_low_energy_devices_info) {
  DevicesInfo changed_devices;
  // Notify DeviceChanged() to devices that have been newly paired as well as to
  // devices that have been removed from the pairing list.
  std::set_symmetric_difference(
      updated_low_energy_devices_info.begin(),
      updated_low_energy_devices_info.end(), low_energy_devices_info_.begin(),
      low_energy_devices_info_.end(),
      std::inserter(changed_devices, changed_devices.end()));

  low_energy_devices_info_ = std::move(updated_low_energy_devices_info);
  for (const auto& info : changed_devices) {
    auto it = devices_.find(
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(info.first));
    if (it == devices_.end()) {
      continue;
    }

    NotifyDeviceChanged(it->second.get());
  }
}

void BluetoothLowEnergyAdapterApple::SetCentralManagerForTesting(
    CBCentralManager* central_manager) {
  central_manager.delegate = low_energy_central_manager_delegate_;
  low_energy_central_manager_ = central_manager;
  low_energy_discovery_manager_->SetCentralManager(low_energy_central_manager_);
}

void BluetoothLowEnergyAdapterApple::StartScanWithFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  BluetoothTransport transport = BLUETOOTH_TRANSPORT_LE;
  if (discovery_filter) {
    transport = discovery_filter->GetTransport();
  }

  if (transport & BLUETOOTH_TRANSPORT_LE) {
    StartScanLowEnergy();
  }
  for (auto& observer : observers_) {
    observer.AdapterDiscoveringChanged(this, true);
  }
  DCHECK(callback);
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), /*is_error=*/false,
                                UMABluetoothDiscoverySessionOutcome::SUCCESS));
}

void BluetoothLowEnergyAdapterApple::StopScan(
    DiscoverySessionResultCallback callback) {
  StopScanLowEnergy();

  DVLOG(1) << "Discovery stopped";
  std::move(callback).Run(/*is_error=*/false,
                          UMABluetoothDiscoverySessionOutcome::SUCCESS);
}

CBCentralManager* BluetoothLowEnergyAdapterApple::GetCentralManager() {
  return low_energy_central_manager_;
}

CBPeripheralManager* BluetoothLowEnergyAdapterApple::GetPeripheralManager() {
  return low_energy_peripheral_manager_;
}

void BluetoothLowEnergyAdapterApple::SetLowEnergyDeviceWatcherForTesting(
    scoped_refptr<BluetoothLowEnergyDeviceWatcherMac>
        bluetooth_low_energy_device_watcher) {
  bluetooth_low_energy_device_watcher_ =
      std::move(bluetooth_low_energy_device_watcher);
  bluetooth_low_energy_device_watcher_->Init();
}

void BluetoothLowEnergyAdapterApple::UpdateFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    DiscoverySessionResultCallback callback) {
  // In Mac the start scan handles all updates automatically
  StartScanWithFilter(std::move(discovery_filter), std::move(callback));
}

void BluetoothLowEnergyAdapterApple::StartScanLowEnergy() {
  low_energy_discovery_manager_->StartDiscovery(BluetoothDevice::UUIDList());
}

void BluetoothLowEnergyAdapterApple::StopScanLowEnergy() {
  low_energy_discovery_manager_->StopDiscovery();
  for (const auto& device_id_object_pair : devices_) {
    device_id_object_pair.second->ClearAdvertisementData();
  }
}

void BluetoothLowEnergyAdapterApple::Initialize(base::OnceClosure callback) {
  // Real initialization is deferred to LazyInitialize().
  ui_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  std::move(callback).Run();
}

void BluetoothLowEnergyAdapterApple::InitForTest(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
  lazy_initialized_ = true;
}

BluetoothLowEnergyAdapterApple::GetDevicePairedStatusCallback
BluetoothLowEnergyAdapterApple::GetDevicePairedStatus() const {
  return base::NullCallbackAs<bool(const std::string&)>();
}

bool BluetoothLowEnergyAdapterApple::SetPoweredImpl(bool powered) {
  NOTIMPLEMENTED();
  return false;
}

bool BluetoothLowEnergyAdapterApple::IsLowEnergyPowered() const {
  return [low_energy_central_manager_ state] == CBManagerStatePoweredOn;
}

void BluetoothLowEnergyAdapterApple::LowEnergyDeviceUpdated(
    CBPeripheral* peripheral,
    NSDictionary* advertisement_data,
    int rssi) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  // If has no entry in the map, create new device and insert into |devices_|,
  // otherwise update the existing device.
  const bool is_new_device = device_mac == nullptr;
  if (is_new_device) {
    // A new device has been found.
    device_mac = new BluetoothLowEnergyDeviceMac(this, peripheral);
    DVLOG(1) << *device_mac << ": New Device.";
  } else if (DoesCollideWithKnownDevice(peripheral, device_mac)) {
    return;
  }

  DCHECK(device_mac);
  DVLOG(3) << *device_mac << ": Device updated with "
           << base::SysNSStringToUTF8([advertisement_data description]);

  // Get Advertised UUIDs
  // Core Specification Supplement (CSS) v7, Part 1.1
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdataserviceuuidskey
  BluetoothDevice::UUIDList advertised_uuids;
  NSArray* service_uuids =
      advertisement_data[CBAdvertisementDataServiceUUIDsKey];
  for (CBUUID* uuid in service_uuids) {
    advertised_uuids.push_back(BluetoothUUIDWithCBUUID(uuid));
  }
  NSArray* overflow_service_uuids =
      advertisement_data[CBAdvertisementDataOverflowServiceUUIDsKey];
  for (CBUUID* uuid in overflow_service_uuids) {
    advertised_uuids.push_back(BluetoothUUIDWithCBUUID(uuid));
  }

  // Get Service Data.
  // Core Specification Supplement (CSS) v7, Part 1.11
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdataservicedatakey
  BluetoothDevice::ServiceDataMap service_data_map;
  NSDictionary* service_data =
      advertisement_data[CBAdvertisementDataServiceDataKey];
  for (CBUUID* uuid in service_data) {
    NSData* data = service_data[uuid];
    const uint8_t* bytes = static_cast<const uint8_t*>([data bytes]);
    size_t length = [data length];
    service_data_map.emplace(BluetoothUUIDWithCBUUID(uuid),
                             std::vector<uint8_t>(bytes, bytes + length));
  }

  // Get Manufacturer Data.
  // "Size: 2 or more octets
  // The first 2 octets contain the Company Identifier Code followed
  // by additional manufacturer specific data"
  // Core Specification Supplement (CSS) v7, Part 1.4
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdatamanufacturerdatakey
  //
  BluetoothDevice::ManufacturerDataMap manufacturer_data_map;
  NSData* manufacturer_data =
      advertisement_data[CBAdvertisementDataManufacturerDataKey];
  const uint8_t* bytes = static_cast<const uint8_t*>([manufacturer_data bytes]);
  size_t length = [manufacturer_data length];
  if (length > 1) {
    const uint16_t manufacturer_id = bytes[0] | bytes[1] << 8;
    manufacturer_data_map.emplace(
        manufacturer_id, std::vector<uint8_t>(bytes + 2, bytes + length));
  }

  // Get Tx Power.
  // "Size: 1 octet
  //  0xXX: -127 to +127 dBm"
  // Core Specification Supplement (CSS) v7, Part 1.5
  // https://developer.apple.com/documentation/corebluetooth/cbadvertisementdatatxpowerlevelkey
  NSNumber* tx_power = advertisement_data[CBAdvertisementDataTxPowerLevelKey];
  int8_t clamped_tx_power = BluetoothDevice::ClampPower([tx_power intValue]);

  // Get the Advertising name
  NSString* local_name = advertisement_data[CBAdvertisementDataLocalNameKey];

  for (auto& observer : observers_) {
    std::optional<std::string> device_name_opt = device_mac->GetName();
    std::optional<std::string> local_name_opt =
        base::SysNSStringToUTF8(local_name);

    observer.DeviceAdvertisementReceived(
        device_mac->GetAddress(), device_name_opt,
        local_name == nil ? std::nullopt : local_name_opt, rssi,
        tx_power == nil ? std::nullopt : std::make_optional(clamped_tx_power),
        std::nullopt, /* TODO(crbug.com/41240161) Implement appearance */
        advertised_uuids, service_data_map, manufacturer_data_map);
  }

  device_mac->UpdateAdvertisementData(
      BluetoothDevice::ClampPower(rssi), std::nullopt /* flags */,
      std::move(advertised_uuids),
      tx_power == nil ? std::nullopt : std::make_optional(clamped_tx_power),
      std::move(service_data_map), std::move(manufacturer_data_map));

  if (is_new_device) {
    std::string device_address =
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
    devices_[device_address] = base::WrapUnique(device_mac);
    for (auto& observer : observers_) {
      observer.DeviceAdded(this, device_mac);
    }
  } else {
    for (auto& observer : observers_) {
      observer.DeviceChanged(this, device_mac);
    }
  }
}

void BluetoothLowEnergyAdapterApple::LowEnergyCentralManagerUpdatedState() {
  auto state = [low_energy_central_manager_ state];

  // Flush out system permission requesting callbacks except
  // CBManagerStateResetting state. When it is in CBManagerStateResetting state,
  // there should be another state update soon that is not
  // CBManagerStateResetting state.
  if (state != CBManagerStateResetting) {
    FlushRequestSystemPermissionCallbacks();
  }

  // A state with a value lower than CBManagerStatePoweredOn implies that
  // scanning has stopped and that any connected peripherals have been
  // disconnected. Call DidDisconnectPeripheral manually to update the devices'
  // states since macOS doesn't call it.
  // See
  // https://developer.apple.com/reference/corebluetooth/cbcentralmanagerdelegate/1518888-centralmanagerdidupdatestate?language=objc
  if (state < CBManagerStatePoweredOn) {
    DVLOG(1)
        << "Central no longer powered on. Notifying of device disconnection.";
    for (BluetoothDevice* device : GetDevices()) {
      // GetDevices() returns instances of BluetoothClassicDeviceMac and
      // BluetoothLowEnergyDeviceMac. The DidDisconnectPeripheral() method is
      // only available on BluetoothLowEnergyDeviceMac.
      if (!device->IsLowEnergyDevice()) {
        continue;
      }
      BluetoothLowEnergyDeviceMac* device_mac =
          static_cast<BluetoothLowEnergyDeviceMac*>(device);
      if (device_mac->IsGattConnected()) {
        device_mac->DidDisconnectPeripheral(nullptr);
      }
    }
  }
}

std::vector<BluetoothDevice*>
BluetoothLowEnergyAdapterApple::RetrieveGattConnectedDevicesWithService(
    const BluetoothUUID* uuid) {
  NSArray* cbUUIDs = nil;
  if (!uuid) {
    DVLOG(1) << "Retrieving all connected devices.";
    // It is not possible to ask for all connected peripherals with
    // -[CBCentralManager retrieveConnectedPeripheralsWithServices:] by passing
    // nil. To try to get most of the peripherals, the search is done with
    // Generic Access service.
    CBUUID* genericAccessServiceUUID = [CBUUID UUIDWithString:@"1800"];
    cbUUIDs = @[ genericAccessServiceUUID ];
  } else {
    DVLOG(1) << "Retrieving connected devices with UUID: "
             << uuid->canonical_value();
    NSString* uuidString =
        base::SysUTF8ToNSString(uuid->canonical_value().c_str());
    cbUUIDs = @[ [CBUUID UUIDWithString:uuidString] ];
  }
  NSArray* peripherals = [low_energy_central_manager_
      retrieveConnectedPeripheralsWithServices:cbUUIDs];
  std::vector<BluetoothDevice*> connected_devices;
  for (CBPeripheral* peripheral in peripherals) {
    BluetoothLowEnergyDeviceMac* device_mac =
        GetBluetoothLowEnergyDeviceMac(peripheral);
    const bool is_new_device = device_mac == nullptr;

    if (!is_new_device && DoesCollideWithKnownDevice(peripheral, device_mac)) {
      continue;
    }
    if (is_new_device) {
      device_mac = new BluetoothLowEnergyDeviceMac(this, peripheral);
      std::string device_address =
          BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
      devices_[device_address] = base::WrapUnique(device_mac);
      for (auto& observer : observers_) {
        observer.DeviceAdded(this, device_mac);
      }
    }
    connected_devices.push_back(device_mac);
    DVLOG(1) << *device_mac << ": New connected device.";
  }
  return connected_devices;
}

void BluetoothLowEnergyAdapterApple::CreateGattConnection(
    BluetoothLowEnergyDeviceMac* device_mac) {
  DVLOG(1) << *device_mac << ": Create gatt connection.";
  [low_energy_central_manager_ connectPeripheral:device_mac->peripheral_
                                         options:nil];
}

void BluetoothLowEnergyAdapterApple::DisconnectGatt(
    BluetoothLowEnergyDeviceMac* device_mac) {
  DVLOG(1) << *device_mac << ": Disconnect gatt.";
  [low_energy_central_manager_
      cancelPeripheralConnection:device_mac->peripheral_];
}

void BluetoothLowEnergyAdapterApple::DidConnectPeripheral(
    CBPeripheral* peripheral) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  device_mac->DidConnectPeripheral();
}

void BluetoothLowEnergyAdapterApple::DidFailToConnectPeripheral(
    CBPeripheral* peripheral,
    NSError* error) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  BluetoothDevice::ConnectErrorCode error_code =
      BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN;
  if (error) {
    error_code = BluetoothDeviceMac::GetConnectErrorCodeFromNSError(error);
  }
  DVLOG(1) << *device_mac << ": Failed to connect to peripheral with error "
           << BluetoothLowEnergyAdapterApple::String(error)
           << ", error code: " << error_code;
  device_mac->DidConnectGatt(error_code);
}

void BluetoothLowEnergyAdapterApple::DidDisconnectPeripheral(
    CBPeripheral* peripheral,
    NSError* error) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  device_mac->DidDisconnectPeripheral(error);
}

bool BluetoothLowEnergyAdapterApple::IsBluetoothLowEnergyDeviceSystemPaired(
    std::string_view device_identifier) const {
  auto it = base::ranges::find(low_energy_devices_info_, device_identifier,
                               &DevicesInfo::value_type::first);
  if (it == low_energy_devices_info_.end()) {
    return false;
  }

  if (GetDevicePairedStatus()) {
    return GetDevicePairedStatus().Run(it->second);
  }
  return true;
}

BluetoothLowEnergyDeviceMac*
BluetoothLowEnergyAdapterApple::GetBluetoothLowEnergyDeviceMac(
    CBPeripheral* peripheral) {
  std::string device_address =
      BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  auto iter = devices_.find(device_address);
  if (iter == devices_.end()) {
    return nullptr;
  }
  // device_mac can be BluetoothClassicDeviceMac* or
  // BluetoothLowEnergyDeviceMac* To return valid BluetoothLowEnergyDeviceMac*
  // we need to first check with IsLowEnergyDevice()
  BluetoothDevice* device = iter->second.get();
  return device->IsLowEnergyDevice()
             ? static_cast<BluetoothLowEnergyDeviceMac*>(device)
             : nullptr;
}

bool BluetoothLowEnergyAdapterApple::DoesCollideWithKnownDevice(
    CBPeripheral* peripheral,
    BluetoothLowEnergyDeviceMac* device_mac) {
  // Check that there are no collisions.
  std::string stored_device_id = device_mac->GetIdentifier();
  std::string updated_device_id =
      BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(peripheral);
  if (stored_device_id != updated_device_id) {
    DVLOG(1) << "LowEnergyDeviceUpdated stored_device_id != updated_device_id: "
             << std::endl
             << "  " << stored_device_id << std::endl
             << "  " << updated_device_id;
    // Collision, two identifiers map to the same hash address.  With a 48 bit
    // hash the probability of this occuring with 10,000 devices
    // simultaneously present is 1e-6 (see
    // https://en.wikipedia.org/wiki/Birthday_problem#Probability_table).  We
    // ignore the second device by returning.
    return true;
  }
  return false;
}

void BluetoothLowEnergyAdapterApple::FlushRequestSystemPermissionCallbacks() {
  auto callbacks = std::move(request_system_permission_callbacks_);
  auto status = GetOsPermissionStatus();
  for (auto& cb : callbacks) {
    std::move(cb).Run(status);
  }
}

}  // namespace device

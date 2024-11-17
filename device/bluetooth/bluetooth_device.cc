// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_device.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/string_util_icu.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/no_destructor.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace device {

#if BUILDFLAG(IS_CHROMEOS)
// See Bluetooth Assigned Numbers - 3.3 SDP Service Class and Profile
// Identifiers
const base::NoDestructor<std::vector<BluetoothUUID>> kAudioUUIDs([] {
  return std::vector<BluetoothUUID>({
      BluetoothUUID("0x110B"),  // Audio sink
      BluetoothUUID("0x111E"),  // Hands free
  });
}());
#endif  // BUILDFLAG(IS_CHROMEOS)

using BatteryInfo = BluetoothDevice::BatteryInfo;
using BatteryType = BluetoothDevice::BatteryType;

BluetoothDevice::DeviceUUIDs::DeviceUUIDs() = default;

BluetoothDevice::DeviceUUIDs::~DeviceUUIDs() = default;

BluetoothDevice::DeviceUUIDs::DeviceUUIDs(const DeviceUUIDs& other) = default;

BluetoothDevice::DeviceUUIDs& BluetoothDevice::DeviceUUIDs::operator=(
    const DeviceUUIDs& other) = default;

void BluetoothDevice::DeviceUUIDs::ReplaceAdvertisedUUIDs(
    UUIDList new_advertised_uuids) {
  advertised_uuids_.clear();
  for (auto& it : new_advertised_uuids) {
    advertised_uuids_.insert(std::move(it));
  }
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ClearAdvertisedUUIDs() {
  advertised_uuids_.clear();
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ReplaceServiceUUIDs(
    const GattServiceMap& gatt_services) {
  service_uuids_.clear();
  for (const auto& gatt_service_pair : gatt_services)
    service_uuids_.insert(gatt_service_pair.second->GetUUID());
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ReplaceServiceUUIDs(
    UUIDList new_service_uuids) {
  service_uuids_.clear();
  for (auto& it : new_service_uuids) {
    service_uuids_.insert(std::move(it));
  }
  UpdateDeviceUUIDs();
}

void BluetoothDevice::DeviceUUIDs::ClearServiceUUIDs() {
  service_uuids_.clear();
  UpdateDeviceUUIDs();
}

const BluetoothDevice::UUIDSet& BluetoothDevice::DeviceUUIDs::GetUUIDs() const {
  return device_uuids_;
}

void BluetoothDevice::DeviceUUIDs::UpdateDeviceUUIDs() {
  device_uuids_ = base::STLSetUnion<BluetoothDevice::UUIDSet>(advertised_uuids_,
                                                              service_uuids_);
}

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(adapter),
      gatt_services_discovery_complete_(false),
      last_update_time_(base::Time()) {}

BluetoothDevice::~BluetoothDevice() {
  for (BluetoothGattConnection* connection : gatt_connections_) {
    connection->InvalidateConnectionReference();
  }
}

BluetoothDevice::ConnectionInfo::ConnectionInfo()
    : rssi(kUnknownPower),
      transmit_power(kUnknownPower),
      max_transmit_power(kUnknownPower) {}

BluetoothDevice::ConnectionInfo::ConnectionInfo(int rssi,
                                                int transmit_power,
                                                int max_transmit_power)
    : rssi(rssi),
      transmit_power(transmit_power),
      max_transmit_power(max_transmit_power) {}

BluetoothDevice::ConnectionInfo::~ConnectionInfo() = default;

BatteryInfo::BatteryInfo() : BatteryInfo(BatteryType::kDefault, std::nullopt) {}

BatteryInfo::BatteryInfo(BatteryType type, std::optional<uint8_t> percentage)
    : BatteryInfo(type, percentage, BatteryInfo::ChargeState::kUnknown) {}

BatteryInfo::BatteryInfo(BatteryType type,
                         std::optional<uint8_t> percentage,
                         ChargeState charge_state)
    : type(type),
      percentage(std::move(percentage)),
      charge_state(charge_state) {}

BatteryInfo::BatteryInfo(const BatteryInfo&) = default;

BatteryInfo& BatteryInfo::operator=(const BatteryInfo&) = default;

BatteryInfo::BatteryInfo(BatteryInfo&&) = default;

BatteryInfo& BatteryInfo::operator=(BatteryInfo&&) = default;

bool BatteryInfo::operator==(const BatteryInfo& other) {
  return type == other.type && percentage == other.percentage &&
         charge_state == other.charge_state;
}

BatteryInfo::~BatteryInfo() = default;

std::u16string BluetoothDevice::GetNameForDisplay() const {
  std::optional<std::string> name = GetName();
  if (name && HasGraphicCharacter(name.value())) {
    return base::UTF8ToUTF16(name.value());
  } else {
    return GetAddressWithLocalizedDeviceTypeName();
  }
}

std::u16string BluetoothDevice::GetAddressWithLocalizedDeviceTypeName() const {
  std::u16string address_utf16 = base::UTF8ToUTF16(GetAddress());
  BluetoothDeviceType device_type = GetDeviceType();
  switch (device_type) {
    case BluetoothDeviceType::COMPUTER:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case BluetoothDeviceType::PHONE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case BluetoothDeviceType::MODEM:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case BluetoothDeviceType::AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case BluetoothDeviceType::CAR_AUDIO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case BluetoothDeviceType::VIDEO:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case BluetoothDeviceType::JOYSTICK:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case BluetoothDeviceType::GAMEPAD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case BluetoothDeviceType::KEYBOARD:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case BluetoothDeviceType::MOUSE:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case BluetoothDeviceType::TABLET:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case BluetoothDeviceType::KEYBOARD_MOUSE_COMBO:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
    default:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
  }
}

BluetoothDeviceType BluetoothDevice::GetDeviceType() const {
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32_t bluetooth_class = GetBluetoothClass();
  switch ((bluetooth_class & 0x1f00) >> 8) {
    case 0x01:
      // Computer major device class.
      return BluetoothDeviceType::COMPUTER;
    case 0x02:
      // Phone major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x01:
        case 0x02:
        case 0x03:
          // Cellular, cordless and smart phones.
          return BluetoothDeviceType::PHONE;
        case 0x04:
        case 0x05:
          // Modems: wired or voice gateway and common ISDN access.
          return BluetoothDeviceType::MODEM;
      }
      break;
    case 0x04:
      // Audio major device class.
      switch ((bluetooth_class & 0xfc) >> 2) {
        case 0x08:
          // Car audio.
          return BluetoothDeviceType::CAR_AUDIO;
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x010:
          // Video devices.
          return BluetoothDeviceType::VIDEO;
        default:
          return BluetoothDeviceType::AUDIO;
      }
    case 0x05:
      // Peripheral major device class.
      switch ((bluetooth_class & 0xc0) >> 6) {
        case 0x00:
          // "Not a keyboard or pointing device."
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x01:
              // Joystick.
              return BluetoothDeviceType::JOYSTICK;
            case 0x02:
              // Gamepad.
              return BluetoothDeviceType::GAMEPAD;
            default:
              return BluetoothDeviceType::PERIPHERAL;
          }
        case 0x01:
          // Keyboard.
          return BluetoothDeviceType::KEYBOARD;
        case 0x02:
          // Pointing device.
          switch ((bluetooth_class & 0x01e) >> 2) {
            case 0x05:
              // Digitizer tablet.
              return BluetoothDeviceType::TABLET;
            default:
              // Mouse.
              return BluetoothDeviceType::MOUSE;
          }
        case 0x03:
          // Combo device.
          return BluetoothDeviceType::KEYBOARD_MOUSE_COMBO;
      }
      break;
  }

  // Some bluetooth devices, e.g., Microsoft Universal Foldable Keyboard,
  // do not expose its bluetooth class. Use its appearance as a work-around.
  // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
  uint16_t appearance = GetAppearance();
  // appearance: 10-bit category and 6-bit sub-category
  switch ((appearance & 0xffc0) >> 6) {
    case 0x01:
      // Generic phone
      return BluetoothDeviceType::PHONE;
    case 0x02:
      // Generic computer
      return BluetoothDeviceType::COMPUTER;
    case 0x0f:
      // HID subtype
      switch (appearance & 0x3f) {
        case 0x01:
          // Keyboard.
          return BluetoothDeviceType::KEYBOARD;
        case 0x02:
          // Mouse
          return BluetoothDeviceType::MOUSE;
        case 0x03:
          // Joystick
          return BluetoothDeviceType::JOYSTICK;
        case 0x04:
          // Gamepad
          return BluetoothDeviceType::GAMEPAD;
        case 0x05:
          // Digitizer tablet
          return BluetoothDeviceType::TABLET;
      }
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Some bluetooth devices paired via Fast Pair, e.g., JBL TUNE230NC,
  // do not expose its bluetooth class or its appearance. Use UUIDs as
  // last workaround.
  UUIDSet uuids = GetUUIDs();
  for (const auto& audio_uuid : *kAudioUUIDs) {
    if (uuids.contains(audio_uuid)) {
      return BluetoothDeviceType::AUDIO;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return BluetoothDeviceType::UNKNOWN;
}

bool BluetoothDevice::IsPairable() const {
  BluetoothDeviceType type = GetDeviceType();

  // Get the vendor part of the address: "00:11:22" for "00:11:22:33:44:55"
  std::string vendor = GetOuiPortionOfBluetoothAddress();

  // Verbatim "Bluetooth Mouse", model 96674
  if (type == BluetoothDeviceType::MOUSE && vendor == "00:12:A1")
    return false;
  // Microsoft "Microsoft Bluetooth Notebook Mouse 5000", model X807028-001
  if (type == BluetoothDeviceType::MOUSE && vendor == "7C:ED:8D")
    return false;

  // TODO: Move this database into a config file.

  return true;
}

BluetoothDevice::UUIDSet BluetoothDevice::GetUUIDs() const {
  return device_uuids_.GetUUIDs();
}

#if BUILDFLAG(IS_CHROMEOS)
void BluetoothDevice::SetIsBlockedByPolicy(bool is_blocked_by_policy) {
  if (is_blocked_by_policy_ == is_blocked_by_policy) {
    return;
  }
  is_blocked_by_policy_ = is_blocked_by_policy;
  GetAdapter()->NotifyDeviceIsBlockedByPolicyChanged(this,
                                                     is_blocked_by_policy);
}

bool BluetoothDevice::IsBlockedByPolicy() const {
  return is_blocked_by_policy_;
}
#endif

const BluetoothDevice::ServiceDataMap& BluetoothDevice::GetServiceData() const {
  return service_data_;
}

BluetoothDevice::UUIDSet BluetoothDevice::GetServiceDataUUIDs() const {
  UUIDSet service_data_uuids;
  for (const auto& uuid_service_data_pair : service_data_) {
    service_data_uuids.insert(uuid_service_data_pair.first);
  }
  return service_data_uuids;
}

const std::vector<uint8_t>* BluetoothDevice::GetServiceDataForUUID(
    const BluetoothUUID& uuid) const {
  auto it = service_data_.find(uuid);
  if (it != service_data_.end()) {
    return &it->second;
  }
  return nullptr;
}

const BluetoothDevice::ManufacturerDataMap&
BluetoothDevice::GetManufacturerData() const {
  return manufacturer_data_;
}

BluetoothDevice::ManufacturerIDSet BluetoothDevice::GetManufacturerDataIDs()
    const {
  ManufacturerIDSet manufacturer_data_ids;
  for (const auto& manufacturer_data_pair : manufacturer_data_) {
    manufacturer_data_ids.insert(manufacturer_data_pair.first);
  }
  return manufacturer_data_ids;
}

const std::vector<uint8_t>* BluetoothDevice::GetManufacturerDataForID(
    const ManufacturerId manufacturerID) const {
  auto it = manufacturer_data_.find(manufacturerID);
  if (it != manufacturer_data_.end()) {
    return &it->second;
  }
  return nullptr;
}

std::optional<int8_t> BluetoothDevice::GetInquiryRSSI() const {
  return inquiry_rssi_;
}

std::optional<uint8_t> BluetoothDevice::GetAdvertisingDataFlags() const {
  return advertising_data_flags_;
}

std::optional<int8_t> BluetoothDevice::GetInquiryTxPower() const {
  return inquiry_tx_power_;
}

void BluetoothDevice::CreateGattConnection(
    GattConnectionCallback callback,
    std::optional<BluetoothUUID> service_uuid) {
  if (!supports_service_specific_discovery_)
    service_uuid.reset();

  const bool connection_already_pending =
      !create_gatt_connection_callbacks_.empty();

  create_gatt_connection_callbacks_.push_back(std::move(callback));

  // If a service-specific discovery was originally requested, but this request
  // is for a different or non-specific discovery, then the previous discovery
  // needs to be redone.
  if (target_service_.has_value() && target_service_ != service_uuid) {
    DCHECK(IsGattConnected() || connection_already_pending);
    target_service_ = service_uuid;
    UpgradeToFullDiscovery();
  }

  if (IsGattConnected()) {
    DCHECK(!connection_already_pending);
    return DidConnectGatt(/*error_code=*/std::nullopt);
  }

  if (connection_already_pending) {
    // The correct callback will be run when the existing connection attempt
    // completes.
    return;
  }

  target_service_ = service_uuid;
  CreateGattConnectionImpl(std::move(service_uuid));
}

void BluetoothDevice::SetGattServicesDiscoveryComplete(bool complete) {
  gatt_services_discovery_complete_ = complete;
}

bool BluetoothDevice::IsGattServicesDiscoveryComplete() const {
  return !target_service_ && gatt_services_discovery_complete_;
}

std::vector<BluetoothRemoteGattService*> BluetoothDevice::GetGattServices()
    const {
  std::vector<BluetoothRemoteGattService*> services;
  for (const auto& iter : gatt_services_)
    services.push_back(iter.second.get());
  return services;
}

BluetoothRemoteGattService* BluetoothDevice::GetGattService(
    const std::string& identifier) const {
  auto it = gatt_services_.find(identifier);
  if (it == gatt_services_.end())
    return nullptr;
  return it->second.get();
}

std::string BluetoothDevice::GetIdentifier() const {
  return GetAddress();
}

std::string BluetoothDevice::GetOuiPortionOfBluetoothAddress() const {
  // Get the vendor part of the address: "00:11:22" for "00:11:22:33:44:55".
  return GetAddress().substr(0, 8);
}

void BluetoothDevice::UpdateAdvertisementData(
    int8_t rssi,
    std::optional<uint8_t> flags,
    UUIDList advertised_uuids,
    std::optional<int8_t> tx_power,
    ServiceDataMap service_data,
    ManufacturerDataMap manufacturer_data) {
  UpdateTimestamp();

  inquiry_rssi_ = rssi;
  advertising_data_flags_ = std::move(flags);
  device_uuids_.ReplaceAdvertisedUUIDs(std::move(advertised_uuids));
  inquiry_tx_power_ = std::move(tx_power);
  service_data_ = std::move(service_data);
  manufacturer_data_ = std::move(manufacturer_data);
}

void BluetoothDevice::ClearAdvertisementData() {
  inquiry_rssi_.reset();
  advertising_data_flags_.reset();
  inquiry_tx_power_.reset();
  device_uuids_.ClearAdvertisedUUIDs();
  service_data_.clear();
  manufacturer_data_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

std::vector<BluetoothRemoteGattService*> BluetoothDevice::GetPrimaryServices() {
  std::vector<BluetoothRemoteGattService*> services;
  DVLOG(2) << "Looking for services.";
  for (BluetoothRemoteGattService* service : GetGattServices()) {
    DVLOG(2) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

std::vector<BluetoothRemoteGattService*>
BluetoothDevice::GetPrimaryServicesByUUID(const BluetoothUUID& service_uuid) {
  std::vector<BluetoothRemoteGattService*> services;
  DVLOG(2) << "Looking for service: " << service_uuid.canonical_value();
  for (BluetoothRemoteGattService* service : GetGattServices()) {
    DVLOG(2) << "Service in cache: " << service->GetUUID().canonical_value();
    if (service->GetUUID() == service_uuid && service->IsPrimary()) {
      services.push_back(service);
    }
  }
  return services;
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
void BluetoothDevice::SetBatteryInfo(const BatteryInfo& info) {
  if (info.percentage) {
    DCHECK_GE(info.percentage.value(), 0);
    DCHECK_LE(info.percentage.value(), 100);
  }

  auto result = battery_info_map_.emplace(info.type, info);

  // New info was inserted.
  if (result.second) {
    GetAdapter()->NotifyDeviceBatteryChanged(this, info.type);
    return;
  }

  DCHECK_EQ(result.first->first, info.type);

  // Existing item is the same as the item we are inserting, return early.
  if (result.first->second == info)
    return;

  // Otherwise override existing element.
  result.first->second = info;
  GetAdapter()->NotifyDeviceBatteryChanged(this, info.type);
}

bool BluetoothDevice::RemoveBatteryInfo(const BatteryType& type) {
  if (battery_info_map_.erase(type)) {
    GetAdapter()->NotifyDeviceBatteryChanged(this, type);
    return true;
  }

  return false;
}

std::optional<BatteryInfo> BluetoothDevice::GetBatteryInfo(
    const BatteryType& type) const {
  auto it = battery_info_map_.find(type);

  if (it == battery_info_map_.end())
    return std::nullopt;

  return it->second;
}

std::vector<BatteryType> BluetoothDevice::GetAvailableBatteryTypes() {
  std::vector<BatteryType> types;

  for (auto& key_value : battery_info_map_) {
    types.push_back(key_value.first);
  }

  return types;
}
#endif

bool BluetoothDevice::supports_service_specific_discovery() const {
  return supports_service_specific_discovery_;
}

void BluetoothDevice::UpgradeToFullDiscovery() {
  // Must be overridden by any subclass that sets
  // |supports_service_specific_discovery_|.
  NOTREACHED();
}

std::unique_ptr<BluetoothGattConnection>
BluetoothDevice::CreateBluetoothGattConnectionObject() {
  return std::make_unique<BluetoothGattConnection>(adapter_, GetAddress());
}

void BluetoothDevice::DidConnectGatt(std::optional<ConnectErrorCode> error) {
  if (error.has_value()) {
    // Connection request should only be made if there are no active
    // connections.
    DCHECK(gatt_connections_.empty());

    target_service_.reset();

    // Callbacks may call back into this code. Move the callback list onto the
    // stack to avoid potential re-entrancy bugs.
    auto callbacks = std::move(create_gatt_connection_callbacks_);
    for (auto& callback : callbacks)
      std::move(callback).Run(/*connection=*/nullptr, error.value());
    return;
  }

  // Callbacks may call back into this code. Move the callback list onto the
  // stack to avoid potential re-entrancy bugs.
  auto callbacks = std::move(create_gatt_connection_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(CreateBluetoothGattConnectionObject(),
                            /*error_code=*/std::nullopt);
  }

  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::DidDisconnectGatt() {
  // Pending calls to connect GATT are not expected, if they were then
  // DidConnectGatt should have been called.
  DCHECK(create_gatt_connection_callbacks_.empty());

  target_service_.reset();

  // Invalidate all BluetoothGattConnection objects.
  for (BluetoothGattConnection* connection : gatt_connections_) {
    connection->InvalidateConnectionReference();
  }
  gatt_connections_.clear();
  GetAdapter()->NotifyDeviceChanged(this);
}

void BluetoothDevice::AddGattConnection(BluetoothGattConnection* connection) {
  auto result = gatt_connections_.insert(connection);
  DCHECK(result.second);  // Check insert happened; there was no duplicate.
}

void BluetoothDevice::RemoveGattConnection(
    BluetoothGattConnection* connection) {
  size_t erased_count = gatt_connections_.erase(connection);
  DCHECK(erased_count);
  if (gatt_connections_.size() == 0)
    DisconnectGatt();
}

void BluetoothDevice::SetAsExpiredForTesting() {
  last_update_time_ = base::Time::NowFromSystemTime() -
                      (BluetoothAdapter::timeoutSec + base::Seconds(1));
}

void BluetoothDevice::Pair(PairingDelegate* pairing_delegate,
                           ConnectCallback callback) {
  NOTREACHED();
}

void BluetoothDevice::UpdateTimestamp() {
  last_update_time_ = base::Time::NowFromSystemTime();
}

base::Time BluetoothDevice::GetLastUpdateTime() const {
  return last_update_time_;
}

// static
int8_t BluetoothDevice::ClampPower(int power) {
  if (power < INT8_MIN) {
    return INT8_MIN;
  }
  if (power > INT8_MAX) {
    return INT8_MAX;
  }
  return static_cast<int8_t>(power);
}

}  // namespace device

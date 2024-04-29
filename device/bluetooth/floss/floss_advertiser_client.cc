// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/bluetooth/floss/floss_advertiser_client.h"

#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/floss/floss_gatt_manager_client.h"

namespace floss {
namespace {
constexpr char kDiscoverable[] = "discoverable";
constexpr char kConnectable[] = "connectable";
constexpr char kScannable[] = "scannable";
constexpr char kIsLegacy[] = "is_legacy";
constexpr char kIsAnonymous[] = "is_anonymous";
constexpr char kIncludeTxPower[] = "include_tx_power";
constexpr char kPrimaryPhy[] = "primary_phy";
constexpr char kSecondaryPhy[] = "secondary_phy";
constexpr char kInterval[] = "interval";
constexpr char kTxPowerLevel[] = "tx_power_level";
constexpr char kOwnAddressType[] = "own_address_type";

constexpr char kServiceUuids[] = "service_uuids";
constexpr char kSolicitUuids[] = "solicit_uuids";
constexpr char kTransportDiscoveryData[] = "transport_discovery_data";
constexpr char kManufacturerData[] = "manufacturer_data";
constexpr char kServiceData[] = "service_data";
constexpr char kIncludeTxPowerLevel[] = "include_tx_power_level";
constexpr char kIncludeDeviceName[] = "include_device_name";
}  // namespace

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const OwnAddressType& type) {
  int32_t value = static_cast<int32_t>(type);
  WriteDBusParam(writer, value);
}

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const AdvertisingSetParametersOld& params) {
  dbus::MessageWriter array(nullptr);

  writer->OpenArray("{sv}", &array);

  WriteDictEntry(&array, kConnectable, params.connectable);
  WriteDictEntry(&array, kScannable, params.scannable);
  WriteDictEntry(&array, kIsLegacy, params.is_legacy);
  WriteDictEntry(&array, kIsAnonymous, params.is_anonymous);
  WriteDictEntry(&array, kIncludeTxPower, params.include_tx_power);
  WriteDictEntry(&array, kPrimaryPhy, params.primary_phy);
  WriteDictEntry(&array, kSecondaryPhy, params.secondary_phy);
  WriteDictEntry(&array, kInterval, params.interval);
  WriteDictEntry(&array, kTxPowerLevel, params.tx_power_level);
  WriteDictEntry(&array, kOwnAddressType, params.own_address_type);
  writer->CloseContainer(&array);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const AdvertisingSetParameters& params) {
  dbus::MessageWriter array(nullptr);

  writer->OpenArray("{sv}", &array);

  WriteDictEntry(&array, kDiscoverable, params.discoverable);
  WriteDictEntry(&array, kConnectable, params.connectable);
  WriteDictEntry(&array, kScannable, params.scannable);
  WriteDictEntry(&array, kIsLegacy, params.is_legacy);
  WriteDictEntry(&array, kIsAnonymous, params.is_anonymous);
  WriteDictEntry(&array, kIncludeTxPower, params.include_tx_power);
  WriteDictEntry(&array, kPrimaryPhy, params.primary_phy);
  WriteDictEntry(&array, kSecondaryPhy, params.secondary_phy);
  WriteDictEntry(&array, kInterval, params.interval);
  WriteDictEntry(&array, kTxPowerLevel, params.tx_power_level);
  WriteDictEntry(&array, kOwnAddressType, params.own_address_type);
  writer->CloseContainer(&array);
}

template <>
void FlossDBusClient::WriteDBusParam(dbus::MessageWriter* writer,
                                     const AdvertiseData& data) {
  dbus::MessageWriter array(nullptr);

  writer->OpenArray("{sv}", &array);
  WriteDictEntry(&array, kServiceUuids, data.service_uuids);
  WriteDictEntry(&array, kSolicitUuids, data.solicit_uuids);
  WriteDictEntry(&array, kTransportDiscoveryData,
                 data.transport_discovery_data);
  WriteDictEntry(&array, kManufacturerData, data.manufacturer_data);
  WriteDictEntry(&array, kServiceData, data.service_data);
  WriteDictEntry(&array, kIncludeTxPowerLevel, data.include_tx_power_level);
  WriteDictEntry(&array, kIncludeDeviceName, data.include_device_name);
  writer->CloseContainer(&array);
}

template <>
void FlossDBusClient::WriteDBusParam(
    dbus::MessageWriter* writer,
    const PeriodicAdvertisingParameters& params) {
  dbus::MessageWriter array(nullptr);

  WriteDictEntry(&array, kIncludeTxPowerLevel, params.include_tx_power_level);
  WriteDictEntry(&array, kInterval, params.interval);
  writer->CloseContainer(&array);
}

template <>
bool FlossDBusClient::ReadDBusParam(dbus::MessageReader* reader,
                                    AdvertisingStatus* status) {
  uint32_t value;
  if (FlossDBusClient::ReadDBusParam(reader, &value)) {
    *status = static_cast<AdvertisingStatus>(value);
    return true;
  }

  return false;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const OwnAddressType*) {
  static DBusTypeInfo info{"i", "OwnAddressType"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const AdvertisingSetParametersOld*) {
  static DBusTypeInfo info{"a{sv}", "AdvertisingSetParametersOld"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const AdvertisingSetParameters*) {
  static DBusTypeInfo info{"a{sv}", "AdvertisingSetParameters"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const AdvertiseData*) {
  static DBusTypeInfo info{"a{sv}", "AdvertiseData"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const PeriodicAdvertisingParameters*) {
  static DBusTypeInfo info{"a{sv}", "PeriodicAdvertisingParameters"};
  return info;
}

template <>
const DBusTypeInfo& GetDBusTypeInfo(const AdvertisingStatus*) {
  static DBusTypeInfo info{"u", "AdvertisingStatus*"};
  return info;
}

// static
std::unique_ptr<FlossAdvertiserClient> FlossAdvertiserClient::Create() {
  return std::make_unique<FlossAdvertiserClient>();
}

AdvertiseData::AdvertiseData() = default;
AdvertiseData::AdvertiseData(const AdvertiseData&) = default;
AdvertiseData::~AdvertiseData() = default;

FlossAdvertiserClient::FlossAdvertiserClient() = default;

FlossAdvertiserClient::~FlossAdvertiserClient() {
  for (auto& [_, callbacks] : start_advertising_set_callbacks_) {
    std::move(callbacks.second)
        .Run(device::BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT);
  }
  start_advertising_set_callbacks_.clear();
  for (auto& [_, callbacks] : stop_advertising_set_callbacks_) {
    std::move(callbacks.second)
        .Run(device::BluetoothAdvertisement::ERROR_RESET_ADVERTISING);
  }
  stop_advertising_set_callbacks_.clear();
  for (auto& [_, callbacks] : set_advertising_params_callbacks_) {
    std::move(callbacks.second)
        .Run(device::BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT);
  }
  set_advertising_params_callbacks_.clear();
  CallAdvertisingMethod<bool>(
      base::BindOnce(&FlossAdvertiserClient::CompleteUnregisterCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      advertiser::kUnregisterCallback, callback_id_);
  if (bus_) {
    exported_callback_manager_.UnexportCallback(
        dbus::ObjectPath(kAdvertisingSetCallbackPath));
  }
}

void FlossAdvertiserClient::Init(dbus::Bus* bus,
                                 const std::string& service_name,
                                 const int adapter_index,
                                 base::Version version,
                                 base::OnceClosure on_ready) {
  bus_ = bus;
  service_name_ = service_name;
  gatt_adapter_path_ = GenerateGattPath(adapter_index);
  version_ = version;

  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, gatt_adapter_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossAdvertiserClient couldn't init. Object proxy was null.";
    return;
  }

  exported_callback_manager_.Init(bus_.get());
  exported_callback_manager_.AddMethod(
      advertiser::kOnAdvertisingSetStarted,
      &FlossAdvertiserClientObserver::OnAdvertisingSetStarted);
  exported_callback_manager_.AddMethod(
      advertiser::kOnOwnAddressRead,
      &FlossAdvertiserClientObserver::OnOwnAddressRead);
  exported_callback_manager_.AddMethod(
      advertiser::kOnAdvertisingSetStopped,
      &FlossAdvertiserClientObserver::OnAdvertisingSetStopped);
  exported_callback_manager_.AddMethod(
      advertiser::kOnAdvertisingEnabled,
      &FlossAdvertiserClientObserver::OnAdvertisingEnabled);
  exported_callback_manager_.AddMethod(
      advertiser::kOnAdvertisingDataSet,
      &FlossAdvertiserClientObserver::OnAdvertisingDataSet);
  exported_callback_manager_.AddMethod(
      advertiser::kOnScanResponseDataSet,
      &FlossAdvertiserClientObserver::OnScanResponseDataSet);
  exported_callback_manager_.AddMethod(
      advertiser::kOnAdvertisingParametersUpdated,
      &FlossAdvertiserClientObserver::OnAdvertisingParametersUpdated);
  exported_callback_manager_.AddMethod(
      advertiser::kOnPeriodicAdvertisingParametersUpdated,
      &FlossAdvertiserClientObserver::OnPeriodicAdvertisingParametersUpdated);
  exported_callback_manager_.AddMethod(
      advertiser::kOnPeriodicAdvertisingDataSet,
      &FlossAdvertiserClientObserver::OnPeriodicAdvertisingDataSet);
  exported_callback_manager_.AddMethod(
      advertiser::kOnPeriodicAdvertisingEnabled,
      &FlossAdvertiserClientObserver::OnPeriodicAdvertisingEnabled);

  if (!exported_callback_manager_.ExportCallback(
          dbus::ObjectPath(kAdvertisingSetCallbackPath),
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&FlossAdvertiserClient::OnMethodsExported,
                         weak_ptr_factory_.GetWeakPtr()))) {
    LOG(ERROR)
        << "Unable to successfully export FlossAdvertiserClientObserver.";
    return;
  }

  on_ready_ = std::move(on_ready);
}

void FlossAdvertiserClient::AddObserver(
    FlossAdvertiserClientObserver* observer) {
  observers_.AddObserver(observer);
}

void FlossAdvertiserClient::RemoveObserver(
    FlossAdvertiserClientObserver* observer) {
  observers_.RemoveObserver(observer);
}

void FlossAdvertiserClient::StartAdvertisingSet(
    const AdvertisingSetParameters& params,
    const AdvertiseData& adv_data,
    const std::optional<AdvertiseData> scan_rsp,
    const std::optional<PeriodicAdvertisingParameters> periodic_params,
    const std::optional<AdvertiseData> periodic_data,
    const int32_t duration,
    const int32_t max_ext_adv_events,
    StartSuccessCallback success_callback,
    ErrorCallback error_callback) {
  if (version_ >= base::Version("0.5")) {
    CallAdvertisingMethod(
        base::BindOnce(
            &FlossAdvertiserClient::CompleteStartAdvertisingSetCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
            std::move(error_callback)),
        advertiser::kStartAdvertisingSet, params, adv_data, scan_rsp,
        periodic_params, periodic_data, duration, max_ext_adv_events,
        callback_id_);
  } else {
    AdvertisingSetParametersOld params_old = {
        params.connectable,      params.scannable,        params.is_legacy,
        params.is_anonymous,     params.include_tx_power, params.primary_phy,
        params.secondary_phy,    params.interval,         params.tx_power_level,
        params.own_address_type,
    };

    CallAdvertisingMethod(
        base::BindOnce(
            &FlossAdvertiserClient::CompleteStartAdvertisingSetCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
            std::move(error_callback)),
        advertiser::kStartAdvertisingSet, params_old, adv_data, scan_rsp,
        periodic_params, periodic_data, duration, max_ext_adv_events,
        callback_id_);
  }
}

void FlossAdvertiserClient::StopAdvertisingSet(
    const AdvertiserId adv_id,
    StopSuccessCallback success_callback,
    ErrorCallback error_callback) {
  if (stop_advertising_set_callbacks_.contains(adv_id)) {
    // Stop already called for this adv_id.
    std::move(error_callback)
        .Run(device::BluetoothAdvertisement::ERROR_RESET_ADVERTISING);
    return;
  }

  CallAdvertisingMethod(
      base::BindOnce(&FlossAdvertiserClient::CompleteStopAdvertisingSetCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback),
                     adv_id),
      advertiser::kStopAdvertisingSet, adv_id);
}

void FlossAdvertiserClient::SetAdvertisingParameters(
    const AdvertiserId adv_id,
    const AdvertisingSetParameters& params,
    SetAdvParamsSuccessCallback success_callback,
    ErrorCallback error_callback) {
  CallAdvertisingMethod(
      base::BindOnce(
          &FlossAdvertiserClient::CompleteSetAdvertisingParametersCallback,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
          std::move(error_callback), adv_id),
      advertiser::kSetAdvertisingParameters, adv_id, params);
}

void FlossAdvertiserClient::OnMethodsExported() {
  dbus::ObjectProxy* object_proxy =
      bus_->GetObjectProxy(service_name_, gatt_adapter_path_);
  if (!object_proxy) {
    LOG(ERROR) << "FlossAdvertiserClient couldn't init. Object proxy was null.";
    return;
  }

  // Registering callbacks. We will get the callback id in
  // |CompleteRegisterCallback| for later use.
  dbus::MethodCall register_callback(kGattInterface,
                                     advertiser::kRegisterCallback);
  dbus::MessageWriter writer(&register_callback);
  writer.AppendObjectPath(dbus::ObjectPath(kAdvertisingSetCallbackPath));
  object_proxy->CallMethodWithErrorResponse(
      &register_callback, kDBusTimeoutMs,
      base::BindOnce(&FlossAdvertiserClient::CompleteRegisterCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FlossAdvertiserClient::CompleteRegisterCallback(
    dbus::Response* response,
    dbus::ErrorResponse* error_response) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": error_response=" << error_response;
  if (error_response) {
    FlossDBusClient::LogErrorResponse(
        "AdvertisingManager::RegisterAdvertiserCallback", error_response);
  } else {
    dbus::MessageReader reader(response);
    uint32_t result;
    if (!reader.PopUint32(&result)) {
      LOG(ERROR) << "No callback id provided for "
                    "AdvertisingManager::RegisterAdvertiserCallback";
      return;
    }

    callback_id_ = result;
    BLUETOOTH_LOG(EVENT) << __func__ << ": callback_id_ = " << callback_id_;

    if (on_ready_) {
      std::move(on_ready_).Run();
    }
  }
}

void FlossAdvertiserClient::CompleteUnregisterCallback(DBusResult<bool> ret) {
  if (!ret.has_value() || *ret == false) {
    LOG(WARNING) << __func__ << ": Failed to unregister callback";
  }
}

void FlossAdvertiserClient::CompleteStartAdvertisingSetCallback(
    StartSuccessCallback success_callback,
    ErrorCallback error_callback,
    DBusResult<RegId> ret) {
  if (!ret.has_value()) {
    LOG(ERROR) << "Error on StartAdvertisingSet: " << ret.error();
    std::move(error_callback)
        .Run(device::BluetoothAdvertisement::ERROR_STARTING_ADVERTISEMENT);
    return;
  }

  RegId reg_id = *ret;
  start_advertising_set_callbacks_.insert(
      {reg_id,
       std::make_pair(std::move(success_callback), std::move(error_callback))});
}

void FlossAdvertiserClient::CompleteStopAdvertisingSetCallback(
    StopSuccessCallback success_callback,
    ErrorCallback error_callback,
    const AdvertiserId adv_id,
    DBusResult<Void> ret) {
  if (!ret.has_value()) {
    std::move(error_callback)
        .Run(device::BluetoothAdvertisement::ERROR_RESET_ADVERTISING);
    return;
  }

  auto found = stop_advertising_set_callbacks_.find(adv_id);
  if (found != stop_advertising_set_callbacks_.end()) {
    // |OnAdvertisingSetStopped| has already completed
    std::move(success_callback).Run();
    stop_advertising_set_callbacks_.erase(found);
  } else {
    stop_advertising_set_callbacks_.insert(
        {adv_id, std::make_pair(std::move(success_callback),
                                std::move(error_callback))});
  }
}

void FlossAdvertiserClient::CompleteSetAdvertisingParametersCallback(
    SetAdvParamsSuccessCallback success_callback,
    ErrorCallback error_callback,
    const AdvertiserId adv_id,
    DBusResult<Void> ret) {
  set_advertising_params_callbacks_.insert(
      {adv_id,
       std::make_pair(std::move(success_callback), std::move(error_callback))});
}

void FlossAdvertiserClient::OnAdvertisingSetStarted(RegId reg_id,
                                                    AdvertiserId adv_id,
                                                    int32_t tx_power,
                                                    AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": reg_id=" << reg_id
                       << ", adv_id=" << adv_id << ", tx_power=" << tx_power
                       << ", status=" << static_cast<uint32_t>(status);

  auto found = start_advertising_set_callbacks_.find(reg_id);
  if (found != start_advertising_set_callbacks_.end()) {
    auto& [success_callback, error_callback] = found->second;
    if (status == AdvertisingStatus::kSuccess) {
      std::move(success_callback).Run(adv_id);
    } else {
      std::move(error_callback).Run(GetErrorCode(status));
    }
    start_advertising_set_callbacks_.erase(found);
  }
}

void FlossAdvertiserClient::OnOwnAddressRead(AdvertiserId adv_id,
                                             int32_t address_type,
                                             std::string address) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", address_type=" << address_type
                       << ", address=" << address;
}

void FlossAdvertiserClient::OnAdvertisingSetStopped(AdvertiserId adv_id) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id;

  auto found = stop_advertising_set_callbacks_.find(adv_id);
  if (found != stop_advertising_set_callbacks_.end()) {
    auto& [success_callback, error_callback] = found->second;
    std::move(success_callback).Run();
    stop_advertising_set_callbacks_.erase(found);
  } else {
    // We have seen instances where we will get |OnAdvertisingSetStopped|
    // before |CompleteStopAdvertisingSetCallback|. In that case, put a
    // placeholder in the map to signal that we should run
    // corresponding callbacks in |CompleteStopAdvertisingSetCallback|
    stop_advertising_set_callbacks_.insert(
        {adv_id, std::make_pair(base::DoNothing(), base::DoNothing())});
  }
}

void FlossAdvertiserClient::OnAdvertisingEnabled(AdvertiserId adv_id,
                                                 bool enable,
                                                 AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", enable=" << enable
                       << ", status=" << static_cast<uint32_t>(status);
}

void FlossAdvertiserClient::OnAdvertisingDataSet(AdvertiserId adv_id,
                                                 AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", status=" << static_cast<uint32_t>(status);
}

void FlossAdvertiserClient::OnScanResponseDataSet(AdvertiserId adv_id,
                                                  AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", status=" << static_cast<uint32_t>(status);
}

void FlossAdvertiserClient::OnAdvertisingParametersUpdated(
    AdvertiserId adv_id,
    int32_t tx_power,
    AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", tx_power=" << tx_power
                       << ", status=" << static_cast<uint32_t>(status);

  auto found = set_advertising_params_callbacks_.find(adv_id);
  if (found != set_advertising_params_callbacks_.end()) {
    auto& [success_callback, error_callback] = found->second;

    if (status == AdvertisingStatus::kSuccess) {
      std::move(success_callback).Run();
    } else {
      std::move(error_callback).Run(GetErrorCode(status));
    }
    set_advertising_params_callbacks_.erase(found);
  }
}

void FlossAdvertiserClient::OnPeriodicAdvertisingParametersUpdated(
    AdvertiserId adv_id,
    AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", status=" << static_cast<uint32_t>(status);
}

void FlossAdvertiserClient::OnPeriodicAdvertisingDataSet(
    AdvertiserId adv_id,
    AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", status=" << static_cast<uint32_t>(status);
}

void FlossAdvertiserClient::OnPeriodicAdvertisingEnabled(
    AdvertiserId adv_id,
    bool enable,
    AdvertisingStatus status) {
  BLUETOOTH_LOG(EVENT) << __func__ << ": adv_id=" << adv_id
                       << ", enable=" << enable
                       << ", status=" << static_cast<uint32_t>(status);
}

device::BluetoothAdvertisement::ErrorCode FlossAdvertiserClient::GetErrorCode(
    AdvertisingStatus status) {
  switch (status) {
    case AdvertisingStatus::kSuccess:
      return device::BluetoothAdvertisement::INVALID_ADVERTISEMENT_ERROR_CODE;
    case AdvertisingStatus::kDataTooLarge:
      return device::BluetoothAdvertisement::ERROR_ADVERTISEMENT_INVALID_LENGTH;
    case AdvertisingStatus::kAlreadyStarted:
      return device::BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS;
    default:
      return device::BluetoothAdvertisement::
          ERROR_INVALID_ADVERTISEMENT_INTERVAL;
  }
}

}  // namespace floss

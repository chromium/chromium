// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_advertisement_floss.h"

#include <iomanip>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/dbus/bluetooth_le_advertising_manager_client.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {
namespace {

constexpr FlossAdvertiserClient::AdvertiserId kInvalidAdvId = -1;
constexpr uint8_t kServiceData16BitUuid = 0x16;
constexpr int8_t kTxPowerNoPreference = 0x7f;
constexpr int32_t kUnlimitedDuration = 0;
constexpr int32_t kUnlimitedAdvEvents = 0;

void UnregisterFailure(device::BluetoothAdvertisement::ErrorCode error) {
  LOG(ERROR)
      << "BluetoothAdvertisementFloss::Unregister failed with error code = "
      << error;
}
}  // namespace

BluetoothAdvertisementFloss::BluetoothAdvertisementFloss(
    std::unique_ptr<device::BluetoothAdvertisement::Data> data,
    const uint16_t interval_ms,
    scoped_refptr<BluetoothAdapterFloss> adapter) {
  // Initializing advertising set parameters.
  params_.discoverable = LeDiscoverableMode::kGeneralDiscoverable;
  params_.connectable =
      (data->type() ==
       device::BluetoothAdvertisement::ADVERTISEMENT_TYPE_PERIPHERAL);
  params_.scannable = false;
  params_.is_legacy = true;
  params_.is_anonymous = false;
  // TODO: check BluetoothAdvertisement::Data.
  params_.include_tx_power = false;
  params_.primary_phy =
      LePhy::kPhy1m;  // For Legacy advertisement compatibility.
  params_.secondary_phy = LePhy::kPhy1m;
  params_.interval = (interval_ms * 8) / 5;  // in 0.625 ms unit.
  params_.tx_power_level = kTxPowerNoPreference;
  params_.own_address_type =
      params_.connectable ? OwnAddressType::kPublic : OwnAddressType::kRandom;

  // Initializing advertise data.
  std::optional<UUIDList> service_uuids = data->service_uuids();
  if (service_uuids) {
    for (auto& uuid : *service_uuids) {
      adv_data_.service_uuids.emplace_back(uuid);
    }
  }
  std::optional<ManufacturerData> manuf_data = data->manufacturer_data();
  if (manuf_data) {
    for (auto& [key, val] : *manuf_data) {
      adv_data_.manufacturer_data.emplace(key, std::move(val));
    }
  }
  std::optional<UUIDList> solicit_uuids = data->solicit_uuids();
  if (solicit_uuids) {
    for (auto& uuid : *service_uuids) {
      adv_data_.solicit_uuids.emplace_back(uuid);
    }
  }
  std::optional<ServiceData> service_data = data->service_data();
  if (service_data) {
    for (auto& [key, val] : *service_data) {
      adv_data_.service_data.emplace(std::move(key), std::move(val));
    }
  }
  // TODO: check BluetoothAdvertisement::Data.
  adv_data_.include_tx_power_level = false;
  adv_data_.include_device_name = false;

  // Initializing scan response data.
  std::optional<ScanResponseData> scan_response_data =
      data->scan_response_data();
  if (scan_response_data) {
    params_.scannable = true;
    for (auto& [type, val] : *scan_response_data) {
      if (type == kServiceData16BitUuid) {
        if (val.size() < 2)
          continue;

        uint16_t id = (val[1] << 8) | val[0];
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(4) << std::hex << id;
        device::BluetoothUUID uuid(stream.str());
        std::vector<uint8_t> bytes(val.begin() + 2, val.end());
        scan_rsp_.service_data.emplace(uuid.canonical_value(),
                                       std::move(bytes));
      } else {
        BLUETOOTH_LOG(ERROR) << "Unsupported type: " << type;
      }
    }
    scan_rsp_.include_tx_power_level = false;
    scan_rsp_.include_device_name = false;
  }

  adv_id_ = kInvalidAdvId;
}

BluetoothAdvertisementFloss::~BluetoothAdvertisementFloss() {
  Unregister(base::DoNothing(), base::BindOnce(&UnregisterFailure));
}

void BluetoothAdvertisementFloss::Unregister(SuccessCallback success_callback,
                                             ErrorCallback error_callback) {
  Stop(std::move(success_callback), std::move(error_callback));
}

void BluetoothAdvertisementFloss::Start(
    SuccessCallback success_callback,
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback) {
  if (adv_id_ != kInvalidAdvId) {
    std::move(error_callback)
        .Run(
            device::BluetoothAdvertisement::ERROR_ADVERTISEMENT_ALREADY_EXISTS);
    return;
  }

  FlossDBusManager::Get()->GetAdvertiserClient()->StartAdvertisingSet(
      params_, adv_data_,
      (params_.scannable ? std::optional<AdvertiseData>(scan_rsp_)
                         : std::nullopt),
      std::nullopt, std::nullopt, kUnlimitedDuration, kUnlimitedAdvEvents,
      base::BindOnce(&BluetoothAdvertisementFloss::OnStartSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback)),
      std::move(error_callback));
}

void BluetoothAdvertisementFloss::Stop(SuccessCallback success_callback,
                                       ErrorCallback error_callback) {
  if (adv_id_ == kInvalidAdvId) {
    std::move(error_callback)
        .Run(
            device::BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }

  FlossDBusManager::Get()->GetAdvertiserClient()->StopAdvertisingSet(
      adv_id_,
      base::BindOnce(&BluetoothAdvertisementFloss::OnStopSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback)),
      std::move(error_callback));
}

void BluetoothAdvertisementFloss::SetAdvertisingInterval(
    const uint16_t interval_ms,
    SuccessCallback success_callback,
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback) {
  if (adv_id_ == kInvalidAdvId) {
    std::move(error_callback)
        .Run(
            device::BluetoothAdvertisement::ERROR_ADVERTISEMENT_DOES_NOT_EXIST);
    return;
  }

  params_.interval = (interval_ms * 8) / 5;  // in 0.625 ms unit.

  FlossDBusManager::Get()->GetAdvertiserClient()->SetAdvertisingParameters(
      adv_id_, params_,
      base::BindOnce(
          &BluetoothAdvertisementFloss::OnSetAdvertisingIntervalSuccess,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback)),
      std::move(error_callback));
}

void BluetoothAdvertisementFloss::OnStartSuccess(
    SuccessCallback success_callback,
    FlossAdvertiserClient::AdvertiserId adv_id) {
  adv_id_ = adv_id;
  std::move(success_callback).Run();
}

void BluetoothAdvertisementFloss::OnStopSuccess(
    SuccessCallback success_callback) {
  std::move(success_callback).Run();
  adv_id_ = kInvalidAdvId;
}

void BluetoothAdvertisementFloss::OnSetAdvertisingIntervalSuccess(
    SuccessCallback success_callback) {
  std::move(success_callback).Run();
}

}  // namespace floss

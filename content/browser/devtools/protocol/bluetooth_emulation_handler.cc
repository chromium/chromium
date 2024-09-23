// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/bluetooth_emulation_handler.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/devtools/protocol/bluetooth_emulation.h"
#include "device/bluetooth/emulation/fake_central.h"

using device::BluetoothAdapterFactory;

namespace content::protocol {

namespace {

base::flat_map<uint16_t, std::vector<uint8_t>> ToManufacturerData(
    protocol::Array<protocol::BluetoothEmulation::ManufacturerData>*
        in_manufacturer_data) {
  base::flat_map<uint16_t, std::vector<uint8_t>> out_manufacturer_data;
  for (auto& data : *in_manufacturer_data) {
    auto span = base::span<const uint8_t>(data->GetData());
    out_manufacturer_data[data->GetKey()] =
        std::vector<uint8_t>(span.begin(), span.end());
  }
  return out_manufacturer_data;
}

std::vector<device::BluetoothUUID> ToBluetoothUUIDs(
    protocol::Array<protocol::String>* in_bluetooth_ids) {
  std::vector<device::BluetoothUUID> out_bluetooth_ids;
  for (auto& uuid : *in_bluetooth_ids) {
    out_bluetooth_ids.emplace_back(uuid);
  }
  return out_bluetooth_ids;
}

mojo::StructPtr<bluetooth::mojom::ScanRecord> ToScanRecord(
    BluetoothEmulation::ScanRecord* in_record) {
  mojo::StructPtr<bluetooth::mojom::ScanRecord> out_record =
      bluetooth::mojom::ScanRecord::New();

  if (in_record->HasUuids()) {
    out_record->uuids = ToBluetoothUUIDs(in_record->GetUuids(nullptr));
  }

  // Convert Appearance
  if (in_record->HasAppearance()) {
    out_record->appearance = bluetooth::mojom::Appearance::New(
        in_record->HasAppearance(), in_record->GetAppearance(0));
  }

  // Convert TX Power
  if (in_record->HasTxPower()) {
    out_record->tx_power = bluetooth::mojom::Power::New(
        in_record->HasTxPower(), in_record->GetTxPower(0));
  }

  if (in_record->HasManufacturerData()) {
    out_record->manufacturer_data =
        ToManufacturerData(in_record->GetManufacturerData(nullptr));
  }

  return out_record;
}

bluetooth::mojom::CentralState ToCentralState(const String& state_string) {
  if (state_string ==
      protocol::BluetoothEmulation::CentralStateEnum::PoweredOff) {
    return bluetooth::mojom::CentralState::POWERED_OFF;
  } else if (state_string ==
             protocol::BluetoothEmulation::CentralStateEnum::PoweredOn) {
    return bluetooth::mojom::CentralState::POWERED_ON;
  }
  return bluetooth::mojom::CentralState::ABSENT;
}

}  // namespace

BluetoothEmulationHandler::BluetoothEmulationHandler()
    : DevToolsDomainHandler(BluetoothEmulation::Metainfo::domainName) {}

BluetoothEmulationHandler::~BluetoothEmulationHandler() = default;

void BluetoothEmulationHandler::Wire(UberDispatcher* dispatcher) {
  BluetoothEmulation::Dispatcher::wire(dispatcher, this);
}

Response BluetoothEmulationHandler::Enable(const String& in_state) {
  if (emulation_enabled_ || fake_central_.is_bound()) {
    return Response::ServerError("BluetoothEmulation already enabled");
  }

  emulation_enabled_ = true;
  content::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(
      base::MakeRefCounted<bluetooth::FakeCentral>(
          ToCentralState(in_state),
          fake_central_.BindNewPipeAndPassReceiver()));
  return Response::Success();
}

Response BluetoothEmulationHandler::Disable() {
  if (fake_central_.is_bound()) {
    CHECK(emulation_enabled_);
    fake_central_.reset();
    // reset this only if this is the instance holding the bound central.
    content::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(
        nullptr);
    emulation_enabled_ = false;
  }
  return Response::Success();
}

void BluetoothEmulationHandler::SimulatePreconnectedPeripheral(
    const String& in_address,
    const String& in_name,
    std::unique_ptr<
        protocol::Array<protocol::BluetoothEmulation::ManufacturerData>>
        in_manufacturer_data,
    std::unique_ptr<protocol::Array<String>> in_known_service_uuids,
    std::unique_ptr<SimulatePreconnectedPeripheralCallback> callback) {
  if (!fake_central_.is_bound()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }
  fake_central_->SimulatePreconnectedPeripheral(
      in_address, in_name, ToManufacturerData(in_manufacturer_data.get()),
      ToBluetoothUUIDs(in_known_service_uuids.get()),
      base::BindOnce(&SimulatePreconnectedPeripheralCallback::sendSuccess,
                     std::move(callback)));
}

void BluetoothEmulationHandler::SimulateAdvertisement(
    std::unique_ptr<protocol::BluetoothEmulation::ScanEntry> in_entry,
    std::unique_ptr<SimulateAdvertisementCallback> callback) {
  if (!fake_central_.is_bound()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }
  bluetooth::mojom::ScanResultPtr payload = bluetooth::mojom::ScanResult::New(
      in_entry->GetDeviceAddress(), in_entry->GetRssi(),
      ToScanRecord(in_entry->GetScanRecord()));
  fake_central_->SimulateAdvertisementReceived(
      std::move(payload),
      base::BindOnce(&SimulateAdvertisementCallback::sendSuccess,
                     std::move(callback)));
}

}  // namespace content::protocol

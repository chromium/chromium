// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/bluetooth_emulation_handler.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
    out_manufacturer_data[data->GetKey()] = base::ToVector(data->GetData());
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

mojo::StructPtr<bluetooth::mojom::CharacteristicProperties>
ToCharacteristicProperties(
    BluetoothEmulation::CharacteristicProperties* in_properties) {
  mojo::StructPtr<bluetooth::mojom::CharacteristicProperties> out_properties =
      bluetooth::mojom::CharacteristicProperties::New();
  out_properties->broadcast = in_properties->GetBroadcast().value_or(false);
  out_properties->read = in_properties->GetRead().value_or(false);
  out_properties->write_without_response =
      in_properties->GetWriteWithoutResponse().value_or(false);
  out_properties->write = in_properties->GetWrite().value_or(false);
  out_properties->notify = in_properties->GetNotify().value_or(false);
  out_properties->indicate = in_properties->GetIndicate().value_or(false);
  out_properties->authenticated_signed_writes =
      in_properties->GetAuthenticatedSignedWrites().value_or(false);
  out_properties->extended_properties =
      in_properties->GetExtendedProperties().value_or(false);
  return out_properties;
}

bluetooth::mojom::CentralState ToCentralState(const std::string& state_string) {
  if (state_string ==
      protocol::BluetoothEmulation::CentralStateEnum::PoweredOff) {
    return bluetooth::mojom::CentralState::POWERED_OFF;
  } else if (state_string ==
             protocol::BluetoothEmulation::CentralStateEnum::PoweredOn) {
    return bluetooth::mojom::CentralState::POWERED_ON;
  }
  return bluetooth::mojom::CentralState::ABSENT;
}

constexpr std::string_view ToGATTOperation(
    bluetooth::mojom::GATTOperationType type) {
  switch (type) {
    case bluetooth::mojom::GATTOperationType::kConnect:
      return BluetoothEmulation::GATTOperationTypeEnum::Connection;
    case bluetooth::mojom::GATTOperationType::kDiscovery:
      return BluetoothEmulation::GATTOperationTypeEnum::Discovery;
  }
}

std::optional<bluetooth::mojom::GATTOperationType> ToGATTOperation(
    std::string_view type) {
  if (type == BluetoothEmulation::GATTOperationTypeEnum::Connection) {
    return bluetooth::mojom::GATTOperationType::kConnect;
  } else if (type == BluetoothEmulation::GATTOperationTypeEnum::Discovery) {
    return bluetooth::mojom::GATTOperationType::kDiscovery;
  } else {
    return std::nullopt;
  }
}

constexpr std::string_view ToCharacteristicOperation(
    bluetooth::mojom::CharacteristicOperationType type) {
  switch (type) {
    case bluetooth::mojom::CharacteristicOperationType::kRead:
      return BluetoothEmulation::CharacteristicOperationTypeEnum::Read;
    case bluetooth::mojom::CharacteristicOperationType::kWrite:
      return BluetoothEmulation::CharacteristicOperationTypeEnum::Write;
    case bluetooth::mojom::CharacteristicOperationType::
        kSubscribeToNotifications:
      return BluetoothEmulation::CharacteristicOperationTypeEnum::
          SubscribeToNotifications;
    case bluetooth::mojom::CharacteristicOperationType::
        kUnsubscribeFromNotifications:
      return BluetoothEmulation::CharacteristicOperationTypeEnum::
          UnsubscribeFromNotifications;
  }
}

std::optional<bluetooth::mojom::CharacteristicOperationType>
ToCharacteristicOperation(std::string_view type) {
  if (type == BluetoothEmulation::CharacteristicOperationTypeEnum::Read) {
    return bluetooth::mojom::CharacteristicOperationType::kRead;
  } else if (type ==
             BluetoothEmulation::CharacteristicOperationTypeEnum::Write) {
    return bluetooth::mojom::CharacteristicOperationType::kWrite;
  } else if (type == BluetoothEmulation::CharacteristicOperationTypeEnum::
                         SubscribeToNotifications) {
    return bluetooth::mojom::CharacteristicOperationType::
        kSubscribeToNotifications;
  } else if (type == BluetoothEmulation::CharacteristicOperationTypeEnum::
                         UnsubscribeFromNotifications) {
    return bluetooth::mojom::CharacteristicOperationType::
        kUnsubscribeFromNotifications;
  } else {
    return std::nullopt;
  }
}

constexpr std::string_view ToCharacteristicWriteType(
    bluetooth::mojom::WriteType type) {
  switch (type) {
    case bluetooth::mojom::WriteType::kNone:
      NOTREACHED();
    case bluetooth::mojom::WriteType::kWriteDefaultDeprecated:
      return BluetoothEmulation::CharacteristicWriteTypeEnum::
          WriteDefaultDeprecated;
    case bluetooth::mojom::WriteType::kWriteWithResponse:
      return BluetoothEmulation::CharacteristicWriteTypeEnum::WriteWithResponse;
    case bluetooth::mojom::WriteType::kWriteWithoutResponse:
      return BluetoothEmulation::CharacteristicWriteTypeEnum::
          WriteWithoutResponse;
  }
}

constexpr std::string_view ToDescriptorOperation(
    bluetooth::mojom::DescriptorOperationType type) {
  switch (type) {
    case bluetooth::mojom::DescriptorOperationType::kRead:
      return BluetoothEmulation::CharacteristicOperationTypeEnum::Read;
    case bluetooth::mojom::DescriptorOperationType::kWrite:
      return BluetoothEmulation::DescriptorOperationTypeEnum::Write;
  }
}

std::optional<bluetooth::mojom::DescriptorOperationType> ToDescriptorOperation(
    std::string_view type) {
  if (type == BluetoothEmulation::DescriptorOperationTypeEnum::Read) {
    return bluetooth::mojom::DescriptorOperationType::kRead;
  } else if (type == BluetoothEmulation::DescriptorOperationTypeEnum::Write) {
    return bluetooth::mojom::DescriptorOperationType::kWrite;
  } else {
    return std::nullopt;
  }
}

std::string getParentId(const std::string& id) {
  // This decoding mechanism aligns with the identifier formatting mechanism in
  // FakePeripheral::AddFakeService,
  // FakeRemoteGattService::AddFakeCharacteristic, and
  // FakeRemoteGattCharacteristic::AddFakeDescriptor.
  size_t lastUnderscorePos = id.rfind('_');
  if (lastUnderscorePos != std::string::npos) {
    return id.substr(0, lastUnderscorePos);
  }
  return "";
}

bool IsValidIdString(const std::string& str) {
  if (str.empty()) {
    return false;
  }
  // Id is expected to start from 1.
  if (str[0] < '1' || str[0] > '9') {
    return false;
  }
  return std::ranges::all_of(str.begin() + 1, str.end(), ::isdigit);
}

bool IsValidServiceId(const std::string& id) {
  size_t lastUnderscorePos = id.rfind('_');
  return IsValidIdString(id.substr(lastUnderscorePos + 1));
}

bool IsValidCharacteristicId(const std::string& id) {
  size_t lastUnderscorePos = id.rfind('_');
  return IsValidIdString(id.substr(lastUnderscorePos + 1)) &&
         IsValidServiceId(id.substr(0, lastUnderscorePos));
}

bool IsValidDescriptorId(const std::string& id) {
  size_t lastUnderscorePos = id.rfind('_');
  return IsValidIdString(id.substr(lastUnderscorePos + 1)) &&
         IsValidCharacteristicId(id.substr(0, lastUnderscorePos));
}

}  // namespace

// static
std::vector<BluetoothEmulationHandler*> BluetoothEmulationHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<BluetoothEmulationHandler>(
      BluetoothEmulation::Metainfo::domainName);
}

BluetoothEmulationHandler::BluetoothEmulationHandler()
    : DevToolsDomainHandler(BluetoothEmulation::Metainfo::domainName) {}

BluetoothEmulationHandler::~BluetoothEmulationHandler() = default;

void BluetoothEmulationHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.emplace(dispatcher->channel());
  BluetoothEmulation::Dispatcher::wire(dispatcher, this);
}

Response BluetoothEmulationHandler::Enable(const std::string& in_state,
                                           bool in_le_supported) {
  if (emulation_enabled_) {
    return Response::ServerError("BluetoothEmulation already enabled");
  }

  CHECK(!is_enabled());
  CHECK(!client_receiver_.is_bound());
  emulation_enabled_ = true;
  global_factory_values_ =
      BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  global_factory_values_->SetLESupported(in_le_supported);
  if (in_le_supported) {
    content::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(
        base::MakeRefCounted<bluetooth::FakeCentral>(
            ToCentralState(in_state),
            fake_central_.BindNewPipeAndPassReceiver()));
    fake_central_.reset_on_disconnect();
    // While there's a possibility the client might not be fully settled on the
    // fake central side upon return, this is acceptable. Client events are
    // expected to be delivered only after at least one peripheral has been
    // simulated. By that time, the client should be properly settled on the
    // fake central side.
    fake_central_->SetClient(client_receiver_.BindNewEndpointAndPassRemote());
    client_receiver_.reset_on_disconnect();
  }
  return Response::Success();
}

Response BluetoothEmulationHandler::Disable() {
  if (is_enabled()) {
    CHECK(emulation_enabled_);
    client_receiver_.reset();
    fake_central_.reset();
    // reset this only if this is the instance holding the bound central.
    content::BluetoothAdapterFactoryWrapper::Get().SetBluetoothAdapterOverride(
        nullptr);
  }
  // The instance that has a valid `global_factory_values_` is the one that sets
  // `emulation_enabled_`. Therefore, only that instance can reset
  // `emulation_enabled_`.
  if (global_factory_values_) {
    global_factory_values_.reset();
    emulation_enabled_ = false;
  }
  return Response::Success();
}

void BluetoothEmulationHandler::SetSimulatedCentralState(
    const std::string& in_state,
    std::unique_ptr<SetSimulatedCentralStateCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }
  fake_central_->SetState(
      ToCentralState(in_state),
      base::BindOnce(&SetSimulatedCentralStateCallback::sendSuccess,
                     std::move(callback)));
}

void BluetoothEmulationHandler::SimulatePreconnectedPeripheral(
    const std::string& in_address,
    const std::string& in_name,
    std::unique_ptr<
        protocol::Array<protocol::BluetoothEmulation::ManufacturerData>>
        in_manufacturer_data,
    std::unique_ptr<protocol::Array<String>> in_known_service_uuids,
    std::unique_ptr<SimulatePreconnectedPeripheralCallback> callback) {
  if (!is_enabled()) {
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
  if (!is_enabled()) {
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

void BluetoothEmulationHandler::SimulateGATTOperationResponse(
    const std::string& in_address,
    const std::string& in_type,
    int in_code,
    std::unique_ptr<SimulateGATTOperationResponseCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }
  auto gatt_operation_type = ToGATTOperation(in_type);
  if (!gatt_operation_type) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({"Unknown GATT operation type ", in_type})));
    return;
  }

  fake_central_->SimulateGATTOperationResponse(
      *gatt_operation_type, in_address, in_code,
      base::BindOnce(
          [](std::unique_ptr<SimulateGATTOperationResponseCallback> callback,
             const std::string& type, bool success) {
            if (!success) {
              std::move(callback)->sendFailure(
                  Response::ServerError(base::StrCat(
                      {"Failed to simulate GATT response for operation type ",
                       type})));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback), in_type));
}

void BluetoothEmulationHandler::SimulateCharacteristicOperationResponse(
    const std::string& characteristic_id,
    const std::string& in_type,
    int in_code,
    std::optional<Binary> in_data,
    std::unique_ptr<SimulateCharacteristicOperationResponseCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }
  auto operation_type = ToCharacteristicOperation(in_type);
  if (!operation_type) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({"Unknown characteristic operation type ", in_type})));
    return;
  }
  if (operation_type == bluetooth::mojom::CharacteristicOperationType::kRead &&
      ((in_code == bluetooth::mojom::kGATTSuccess) != in_data.has_value())) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({"Characteristic operation type ", in_type, " with code ",
                      base::NumberToString(in_code),
                      in_data ? " does not expect" : " expects", " data"})));
    return;
  }

  std::string service_id = getParentId(characteristic_id);
  std::string address = getParentId(service_id);
  fake_central_->SimulateCharacteristicOperationResponse(
      *operation_type, characteristic_id, service_id, address, in_code,
      in_data ? std::optional(base::ToVector(*in_data)) : std::nullopt,
      base::BindOnce(
          [](std::unique_ptr<SimulateCharacteristicOperationResponseCallback>
                 callback,
             const std::string& error_message, bool success) {
            if (!success) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback),
          base::StrCat(
              {"Failed to simulate characteristic response for operation type ",
               in_type})));
}

void BluetoothEmulationHandler::SimulateDescriptorOperationResponse(
    const std::string& descriptor_id,
    const std::string& in_type,
    int in_code,
    std::optional<Binary> in_data,
    std::unique_ptr<SimulateDescriptorOperationResponseCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }
  auto operation_type = ToDescriptorOperation(in_type);
  if (!operation_type) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({"Unknown descriptor operation type ", in_type})));
    return;
  }
  if (operation_type == bluetooth::mojom::DescriptorOperationType::kRead &&
      ((in_code == bluetooth::mojom::kGATTSuccess) != in_data.has_value())) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({"Descriptor operation type ", in_type, " with code ",
                      base::NumberToString(in_code),
                      in_data ? " does not expect" : " expects", " data"})));
    return;
  }

  std::string characteristic_id = getParentId(descriptor_id);
  std::string service_id = getParentId(characteristic_id);
  std::string address = getParentId(service_id);
  fake_central_->SimulateDescriptorOperationResponse(
      *operation_type, descriptor_id, characteristic_id, service_id, address,
      in_code, in_data ? std::optional(base::ToVector(*in_data)) : std::nullopt,
      base::BindOnce(
          [](std::unique_ptr<SimulateDescriptorOperationResponseCallback>
                 callback,
             const std::string& error_message, bool success) {
            if (!success) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback),
          base::StrCat(
              {"Failed to simulate descriptor response for operation type ",
               in_type})));
}

void BluetoothEmulationHandler::AddService(
    const std::string& in_address,
    const std::string& service_uuid,
    std::unique_ptr<AddServiceCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  device::BluetoothUUID uuid(service_uuid);
  if (!uuid.IsValid()) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({service_uuid, " is not a valid UUID"})));
    return;
  }

  fake_central_->AddFakeService(
      in_address, uuid,
      base::BindOnce(
          [](std::unique_ptr<AddServiceCallback> callback,
             const std::string& error_message,
             const std::optional<std::string>& identifier) {
            if (!identifier) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            DCHECK(IsValidServiceId(*identifier));
            std::move(callback)->sendSuccess(*identifier);
          },
          std::move(callback),
          base::StrCat({"Failed to add service ", service_uuid,
                        " to peripheral ", in_address})));
}

void BluetoothEmulationHandler::RemoveService(
    const std::string& service_id,
    std::unique_ptr<RemoveServiceCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  std::string address = getParentId(service_id);
  fake_central_->RemoveFakeService(
      service_id, address,
      base::BindOnce(
          [](std::unique_ptr<RemoveServiceCallback> callback,
             const std::string& error_message, bool success) {
            if (!success) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback),
          base::StrCat(
              {"Failed to remove service represented by ", service_id})));
}

void BluetoothEmulationHandler::AddCharacteristic(
    const std::string& service_id,
    const std::string& characteristic_uuid,
    std::unique_ptr<protocol::BluetoothEmulation::CharacteristicProperties>
        in_properties,
    std::unique_ptr<AddCharacteristicCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  device::BluetoothUUID uuid(characteristic_uuid);
  if (!uuid.IsValid()) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({characteristic_uuid, " is not a valid UUID"})));
    return;
  }

  std::string address = getParentId(service_id);
  fake_central_->AddFakeCharacteristic(
      uuid, ToCharacteristicProperties(in_properties.get()), service_id,
      address,
      base::BindOnce(
          [](std::unique_ptr<AddCharacteristicCallback> callback,
             const std::string& error_message,
             const std::optional<std::string>& identifier) {
            if (!identifier) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            DCHECK(IsValidCharacteristicId(*identifier));
            std::move(callback)->sendSuccess(*identifier);
          },
          std::move(callback),
          base::StrCat({"Failed to add characteristic ", characteristic_uuid,
                        " to service ", service_id})));
}

void BluetoothEmulationHandler::RemoveCharacteristic(
    const std::string& characteristic_id,
    std::unique_ptr<RemoveCharacteristicCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  std::string service_id = getParentId(characteristic_id);
  std::string address = getParentId(service_id);
  fake_central_->RemoveFakeCharacteristic(
      characteristic_id, service_id, address,
      base::BindOnce(
          [](std::unique_ptr<RemoveCharacteristicCallback> callback,
             const std::string& error_message, bool success) {
            if (!success) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback),
          base::StrCat({"Failed to remove characteristic represented by ",
                        characteristic_id})));
}

void BluetoothEmulationHandler::AddDescriptor(
    const std::string& characteristic_id,
    const std::string& descriptor_uuid,
    std::unique_ptr<AddDescriptorCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  device::BluetoothUUID uuid(descriptor_uuid);
  if (!uuid.IsValid()) {
    std::move(callback)->sendFailure(Response::InvalidParams(
        base::StrCat({descriptor_uuid, " is not a valid UUID"})));
    return;
  }

  std::string service_id = getParentId(characteristic_id);
  std::string address = getParentId(service_id);
  fake_central_->AddFakeDescriptor(
      uuid, characteristic_id, service_id, address,
      base::BindOnce(
          [](std::unique_ptr<AddDescriptorCallback> callback,
             const std::string& error_message,
             const std::optional<std::string>& identifier) {
            if (!identifier) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            DCHECK(IsValidDescriptorId(*identifier));
            std::move(callback)->sendSuccess(*identifier);
          },
          std::move(callback),
          base::StrCat({"Failed to add descriptor ", descriptor_uuid,
                        " to characteristic ", characteristic_id})));
}

void BluetoothEmulationHandler::RemoveDescriptor(
    const std::string& descriptor_id,
    std::unique_ptr<RemoveDescriptorCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  std::string characteristic_id = getParentId(descriptor_id);
  std::string service_id = getParentId(characteristic_id);
  std::string address = getParentId(service_id);
  fake_central_->RemoveFakeDescriptor(
      descriptor_id, characteristic_id, service_id, address,
      base::BindOnce(
          [](std::unique_ptr<RemoveDescriptorCallback> callback,
             const std::string& error_message, bool success) {
            if (!success) {
              std::move(callback)->sendFailure(
                  Response::ServerError(error_message));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback),
          base::StrCat(
              {"Failed to remove descriptor represented by ", descriptor_id})));
}

void BluetoothEmulationHandler::SimulateGATTDisconnection(
    const std::string& address,
    std::unique_ptr<SimulateGATTDisconnectionCallback> callback) {
  if (!is_enabled()) {
    std::move(callback)->sendFailure(
        Response::ServerError("BluetoothEmulation not enabled"));
    return;
  }

  fake_central_->SimulateGATTDisconnection(
      address,
      base::BindOnce(
          [](std::unique_ptr<SimulateGATTDisconnectionCallback> callback,
             bool success) {
            if (!success) {
              std::move(callback)->sendFailure(Response::ServerError(
                  "Failed to simulate GATT disconnection"));
              return;
            }
            std::move(callback)->sendSuccess();
          },
          std::move(callback)));
}

void BluetoothEmulationHandler::DispatchGATTOperationEvent(
    bluetooth::mojom::GATTOperationType type,
    const std::string& peripheral_address) {
  frontend_->GattOperationReceived(peripheral_address,
                                   std::string(ToGATTOperation(type)));
}

void BluetoothEmulationHandler::DispatchCharacteristicOperationEvent(
    bluetooth::mojom::CharacteristicOperationType type,
    const std::optional<std::vector<uint8_t>>& data,
    const std::optional<bluetooth::mojom::WriteType> write_type,
    const std::string& characteristic_id) {
  frontend_->CharacteristicOperationReceived(
      characteristic_id, std::string(ToCharacteristicOperation(type)),
      data ? std::optional(Binary::fromVector(*data)) : std::nullopt,
      write_type
          ? std::optional(std::string(ToCharacteristicWriteType(*write_type)))
          : std::nullopt);
}

void BluetoothEmulationHandler::DispatchDescriptorOperationEvent(
    bluetooth::mojom::DescriptorOperationType type,
    const std::optional<std::vector<uint8_t>>& data,
    const std::string& descriptor_id) {
  frontend_->DescriptorOperationReceived(
      descriptor_id, std::string(ToDescriptorOperation(type)),
      data ? std::optional(Binary::fromVector(*data)) : std::nullopt);
}

}  // namespace content::protocol

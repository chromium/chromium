// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/digital_credentials/cross_device_transaction_impl.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "content/browser/webid/digital_credentials/cross_device_request_dispatcher.h"
#include "content/public/browser/digital_credentials_cross_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/ble_adapter_manager.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/fido_cable_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"

#if BUILDFLAG(IS_MAC)
#include "base/process/process_info.h"
#endif

namespace content::digital_credentials::cross_device {

namespace {

std::optional<Error> CheckConfiguration() {
#if BUILDFLAG(IS_MAC)
  if (!device::BluetoothAdapterFactory::HasSharedInstanceForTesting() &&
      !base::IsProcessSelfResponsible()) {
    FIDO_LOG(EVENT)
        << "Cannot use Bluetooth because process is not self-responsible. "
           "Launch from Finder or with `open` instead.";
    return SystemError::kNotSelfResponsible;
  }
#endif

  if (!device::BluetoothAdapterFactory::Get()->IsLowEnergySupported()) {
    FIDO_LOG(EVENT) << "No BLE support.";
    return SystemError::kNoBleSupport;
  }

  return std::nullopt;
}

}  // namespace

Transaction::~Transaction() = default;

std::unique_ptr<Transaction> Transaction::New(
    url::Origin origin,
    base::Value request,
    std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key,
    device::NetworkContextFactory network_context_factory,
    EventCallback event_callback,
    CompletionCallback callback) {
  return std::make_unique<TransactionImpl>(
      std::move(origin), std::move(request), qr_generator_key,
      std::move(network_context_factory), std::move(event_callback),
      std::move(callback));
}

TransactionImpl::TransactionImpl(
    url::Origin origin,
    base::Value request,
    std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key,
    device::NetworkContextFactory network_context_factory,
    Transaction::EventCallback event_callback,
    Transaction::CompletionCallback callback)
    : origin_(std::move(origin)),
      request_(std::move(request)),
      event_callback_(std::move(event_callback)),
      callback_(std::move(callback)) {
  std::optional<Error> error = CheckConfiguration();
  if (error) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), base::unexpected(*error)));
    return;
  }

  auto v1_discovery = std::make_unique<device::FidoCableDiscovery>(
      std::vector<device::CableDiscoveryData>());
  auto v2_discovery = std::make_unique<device::cablev2::Discovery>(
      // This request type argument is unused. It only applies to the payload
      // sent for state-assisted transactions, but those aren't supported for
      // digital credentials.
      device::FidoRequestType::kGetAssertion, network_context_factory,
      qr_generator_key, v1_discovery->GetV2AdvertStream(),
      /*contact_device_stream=*/nullptr,
      std::vector<device::CableDiscoveryData>(),
      /*pairing_callback=*/std::nullopt,
      /*invalidated_pairing_callback=*/std::nullopt,
      base::BindRepeating(&TransactionImpl::OnCableEvent,
                          weak_ptr_factory_.GetWeakPtr()),
      /*must_support_ctap=*/false);
  dispatcher_ = std::make_unique<RequestDispatcher>(
      std::move(v1_discovery), std::move(v2_discovery), origin_,
      std::move(request_),
      base::BindOnce(&TransactionImpl::OnHaveResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &TransactionImpl::OnHaveAdapter, weak_ptr_factory_.GetWeakPtr()));
}

TransactionImpl::~TransactionImpl() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void TransactionImpl::PowerBluetoothAdapter() {
  CHECK(callback_);
  CHECK(waiting_for_power_);
  FIDO_LOG(EVENT) << "Powering BLE adapter";
  adapter_->SetPowered(true, base::DoNothing(), base::DoNothing());
}

void TransactionImpl::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                            bool powered) {
  CHECK(adapter_);

  if (!callback_) {
    return;
  }
  if (!powered && !waiting_for_power_) {
    FIDO_LOG(EVENT) << "Lost BLE power during digital identity transaction.";
    std::move(callback_).Run(base::unexpected(SystemError::kLostPower));
    return;
  }
  if (powered && waiting_for_power_) {
    FIDO_LOG(EVENT) << "BLE adapter was powered.";
    waiting_for_power_ = false;
  }

  MaybeSignalReady();
}

void TransactionImpl::OnCableEvent(device::cablev2::Event event) {
  if (!callback_) {
    return;
  }
  event_callback_.Run(event);
}

void TransactionImpl::OnHaveAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  CHECK(!adapter_);
  adapter_ = adapter;
  adapter_->AddObserver(this);

  if (!adapter_->IsPresent()) {
    FIDO_LOG(EVENT) << "No BLE adapter.";
    std::move(callback_).Run(base::unexpected(SystemError::kNoBleSupport));
    return;
  }

  switch (adapter_->GetOsPermissionStatus()) {
    case device::BluetoothAdapter::PermissionStatus::kUndetermined:
      FIDO_LOG(EVENT) << "Need BLE permission.";
      waiting_for_permission_ = true;
      event_callback_.Run(SystemEvent::kNeedPermission);
      adapter_->RequestSystemPermission(
          base::BindOnce(&TransactionImpl::OnHaveBluetoothPermission,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    case device::BluetoothAdapter::PermissionStatus::kDenied:
      FIDO_LOG(EVENT) << "BLE permission denied.";
      std::move(callback_).Run(
          base::unexpected(SystemError::kPermissionDenied));
      return;
    case device::BluetoothAdapter::PermissionStatus::kAllowed:
      break;
  }

  ConsiderPowerState();
}

void TransactionImpl::ConsiderPowerState() {
  if (!adapter_->IsPowered()) {
    FIDO_LOG(EVENT) << "BLE adapter not powered.";
    waiting_for_power_ = true;
    event_callback_.Run(SystemEvent::kBluetoothNotPowered);
    return;
  }

  MaybeSignalReady();
}

void TransactionImpl::OnHaveBluetoothPermission(
    device::BluetoothAdapter::PermissionStatus status) {
  if (status == device::BluetoothAdapter::PermissionStatus::kDenied) {
    FIDO_LOG(EVENT) << "BLE permission denied.";
    if (callback_) {
      std::move(callback_).Run(
          base::unexpected(SystemError::kPermissionDenied));
    }
    return;
  }

  waiting_for_permission_ = false;
  ConsiderPowerState();
}

void TransactionImpl::MaybeSignalReady() {
  if (!running_signaled_ && !waiting_for_permission_ && !waiting_for_power_) {
    FIDO_LOG(EVENT) << "Digital identity request ready.";
    running_signaled_ = true;
    event_callback_.Run(SystemEvent::kReady);
  }
}

void TransactionImpl::OnHaveResponse(
    base::expected<Response, RequestDispatcher::Error> response) {
  if (!callback_) {
    return;
  }

  if (response.has_value()) {
    FIDO_LOG(EVENT) << "Have response from digital identity request.";
    std::move(callback_).Run(std::move(response).value());
  } else {
    absl::visit(base::Overloaded{
                    [this](ProtocolError error) {
                      FIDO_LOG(EVENT)
                          << "Protocol error from digital identity request: "
                          << static_cast<int>(error);
                      std::move(callback_).Run(base::unexpected(error));
                    },
                    [this](RemoteError error) {
                      FIDO_LOG(EVENT)
                          << "Remote error from digital identity request: "
                          << static_cast<int>(error);
                      std::move(callback_).Run(base::unexpected(error));
                    },
                },
                response.error());
  }
}

}  // namespace content::digital_credentials::cross_device

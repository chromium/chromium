// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_request_handler_base.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/ble_adapter_manager.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_discovery_factory.h"

#if defined(OS_WIN)
#include "device/fido/win/authenticator.h"
#endif

namespace device {

// FidoRequestHandlerBase::TransportAvailabilityInfo --------------------------

FidoRequestHandlerBase::TransportAvailabilityInfo::TransportAvailabilityInfo() =
    default;

FidoRequestHandlerBase::TransportAvailabilityInfo::TransportAvailabilityInfo(
    const TransportAvailabilityInfo& data) = default;

FidoRequestHandlerBase::TransportAvailabilityInfo&
FidoRequestHandlerBase::TransportAvailabilityInfo::operator=(
    const TransportAvailabilityInfo& other) = default;

FidoRequestHandlerBase::TransportAvailabilityInfo::
    ~TransportAvailabilityInfo() = default;

// FidoRequestHandlerBase::Observer -------------------------------------------

FidoRequestHandlerBase::Observer::~Observer() = default;

// FidoRequestHandlerBase -----------------------------------------------------

FidoRequestHandlerBase::FidoRequestHandlerBase() = default;

FidoRequestHandlerBase::FidoRequestHandlerBase(
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& available_transports) {
  InitDiscoveries(fido_discovery_factory, available_transports);
}

void FidoRequestHandlerBase::InitDiscoveries(
    FidoDiscoveryFactory* fido_discovery_factory,
    base::flat_set<FidoTransportProtocol> available_transports) {
#if defined(OS_WIN)
  // Try to instantiate the discovery for proxying requests to the native
  // Windows WebAuthn API; or fall back to using the regular device transport
  // discoveries if the API is unavailable.
  auto win_discovery =
      fido_discovery_factory->MaybeCreateWinWebAuthnApiDiscovery();
  if (win_discovery) {
    // The Windows WebAuthn API is available. On this platform, communicating
    // with authenticator devices directly is blocked by the OS, so we need to
    // go through the native API instead. No device discoveries may be
    // instantiated.
    win_discovery->set_observer(this);
    discoveries_.push_back(std::move(win_discovery));

    //  Setting |has_win_native_api_authenticator| ensures
    //  NotifyObserverTransportAvailability() will not be invoked before
    //  Windows Authenticator has been added. The embedder will be
    //  responsible for dispatch of the authenticator and whether they
    //  display any UI in addition to the one provided by the OS.
    transport_availability_info_.has_win_native_api_authenticator = true;

    // Allow caBLE as a potential additional transport if requested by
    // the implementing class because it is not subject to the OS'
    // device communication block (only GetAssertionRequestHandler uses
    // caBLE). Otherwise, do not instantiate any other transports.
    base::EraseIf(available_transports, [](auto transport) {
      return transport !=
             FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy;
    });
  }
#endif  // defined(OS_WIN)

  transport_availability_info_.available_transports = available_transports;
  for (const auto transport : available_transports) {
    std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries =
        fido_discovery_factory->Create(transport);
    if (discoveries.empty()) {
      // This can occur in tests when a ScopedVirtualU2fDevice is in effect and
      // HID transports are not configured or when caBLE discovery data isn't
      // available.
      transport_availability_info_.available_transports.erase(transport);
      continue;
    }

    for (auto& discovery : discoveries) {
      discovery->set_observer(this);
      discoveries_.emplace_back(std::move(discovery));
    }
  }

  // Check if the platform supports BLE before trying to get a power manager.
  // CaBLE might be in |available_transports| without actual BLE support under
  // the virtual environment.
  // TODO(nsatragno): Move the BLE power manager logic to CableDiscoveryFactory
  // so we don't need this additional check.
  bool has_ble = false;
  if (device::BluetoothAdapterFactory::Get()->IsLowEnergySupported() &&
      base::Contains(transport_availability_info_.available_transports,
                     FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy)) {
    has_ble = true;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FidoRequestHandlerBase::ConstructBleAdapterPowerManager,
                       weak_factory_.GetWeakPtr()));
  }

  bool has_platform =
      base::Contains(transport_availability_info_.available_transports,
                     FidoTransportProtocol::kInternal);

  // Initialize |notify_observer_callback_| with the number of times it has to
  // be invoked before Observer::OnTransportAvailabilityEnumerated is
  // dispatched.
  // Essentially this is used to wait until the parts
  // |transport_availability_info_| are filled out; the
  // |notify_observer_callback_| is invoked once for each discovery once it is
  // ready, and additionally:
  //
  // 1) [If BLE or caBLE are enabled] once BLE adapters have been enumerated
  // 2) [If platform transport requested] When
  //    OnHasRecognizedPlatformCredentialFilled() runs.
  // 3) When |observer_| is set, so that OnTransportAvailabilityEnumerated is
  //    never called before it is set.
  notify_observer_callback_ = base::BarrierClosure(
      discoveries_.size() + has_ble + has_platform + 1,
      base::BindOnce(
          &FidoRequestHandlerBase::NotifyObserverTransportAvailability,
          weak_factory_.GetWeakPtr()));
}

FidoRequestHandlerBase::~FidoRequestHandlerBase() {
  CancelActiveAuthenticators();
}

void FidoRequestHandlerBase::StartAuthenticatorRequest(
    const std::string& authenticator_id) {
  InitializeAuthenticatorAndDispatchRequest(authenticator_id);
}

void FidoRequestHandlerBase::CancelActiveAuthenticators(
    base::StringPiece exclude_device_id) {
  for (auto task_it = active_authenticators_.begin();
       task_it != active_authenticators_.end();) {
    DCHECK(!task_it->first.empty());
    if (task_it->first != exclude_device_id) {
      DCHECK(task_it->second);
      task_it->second->Cancel();

      // Note that the pointer being erased is non-owning. The actual
      // FidoAuthenticator instance is owned by its discovery (which in turn is
      // owned by |discoveries_|.
      task_it = active_authenticators_.erase(task_it);
    } else {
      ++task_it;
    }
  }
}

void FidoRequestHandlerBase::OnBluetoothAdapterEnumerated(
    bool is_present,
    bool is_powered_on,
    bool can_power_on,
    bool is_peripheral_role_supported) {
  if (!is_present) {
    transport_availability_info_.available_transports.erase(
        FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  }

  transport_availability_info_.is_ble_powered = is_powered_on;
  transport_availability_info_.can_power_on_ble_adapter = can_power_on;
  notify_observer_callback_.Run();
}

void FidoRequestHandlerBase::OnBluetoothAdapterPowerChanged(
    bool is_powered_on) {
  transport_availability_info_.is_ble_powered = is_powered_on;

  if (observer_)
    observer_->BluetoothAdapterPowerChanged(is_powered_on);
}

void FidoRequestHandlerBase::PowerOnBluetoothAdapter() {
  if (!bluetooth_adapter_manager_)
    return;

  bluetooth_adapter_manager_->SetAdapterPower(true /* set_power_on */);
}

base::WeakPtr<FidoRequestHandlerBase> FidoRequestHandlerBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FidoRequestHandlerBase::Start() {
  for (const auto& discovery : discoveries_)
    discovery->Start();
}

void FidoRequestHandlerBase::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  // Device connection has been lost or device has already been removed.
  // Thus, calling CancelTask() is not necessary. Also, below
  // ongoing_tasks_.erase() will have no effect for the devices that have been
  // already removed due to processing error or due to invocation of
  // CancelOngoingTasks().
  auto authenticator_it = active_authenticators_.find(authenticator->GetId());
  if (authenticator_it == active_authenticators_.end()) {
    NOTREACHED();
    FIDO_LOG(ERROR) << "AuthenticatorRemoved() for unknown authenticator "
                    << authenticator->GetId();
    return;
  }
  DCHECK_EQ(authenticator_it->second, authenticator);
  active_authenticators_.erase(authenticator_it);
  if (observer_) {
    observer_->FidoAuthenticatorRemoved(authenticator->GetId());
  }
}

void FidoRequestHandlerBase::DiscoveryStarted(
    FidoDiscoveryBase* discovery,
    bool success,
    std::vector<FidoAuthenticator*> authenticators) {
  if (!success) {
    transport_availability_info_.available_transports.erase(
        discovery->transport());
  } else {
    for (auto* authenticator : authenticators) {
      AuthenticatorAdded(discovery, authenticator);
    }
  }

  // Allow GetAssertionRequestHandler to asynchronously check for known platform
  // credentials and defer |notify_observer_callback_| until that check is done.
  // (But don't bother if platform discovery failed to start.)
  if (discovery->transport() == FidoTransportProtocol::kInternal) {
    if (success) {
      FillHasRecognizedPlatformCredential(base::BindOnce(
          &FidoRequestHandlerBase::OnHasRecognizedPlatformCredentialFilled,
          GetWeakPtr()));
    } else {
      FidoRequestHandlerBase::OnHasRecognizedPlatformCredentialFilled();
    }
  }

  DCHECK(notify_observer_callback_);
  notify_observer_callback_.Run();
}

void FidoRequestHandlerBase::AuthenticatorAdded(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK(!authenticator->GetId().empty());
  bool was_inserted;
  std::tie(std::ignore, was_inserted) =
      active_authenticators_.insert({authenticator->GetId(), authenticator});
  if (!was_inserted) {
    NOTREACHED();
    FIDO_LOG(ERROR) << "Authenticator with duplicate ID "
                    << authenticator->GetId();
    return;
  }

  // If |observer_| exists, dispatching request to |authenticator| is
  // delegated to |observer_|. Else, dispatch request to |authenticator|
  // immediately.
  bool embedder_controls_dispatch = false;
  if (observer_) {
    embedder_controls_dispatch =
        observer_->EmbedderControlsAuthenticatorDispatch(*authenticator);
    observer_->FidoAuthenticatorAdded(*authenticator);
  }

  if (!embedder_controls_dispatch) {
    // Post |InitializeAuthenticatorAndDispatchRequest| into its own task. This
    // avoids hairpinning, even if the authenticator immediately invokes the
    // request callback.
    VLOG(2)
        << "Request handler dispatching request to authenticator immediately.";
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FidoRequestHandlerBase::InitializeAuthenticatorAndDispatchRequest,
            GetWeakPtr(), authenticator->GetId()));
  } else {
    VLOG(2) << "Embedder controls the dispatch.";
  }

#if defined(OS_WIN)
  if (authenticator->IsWinNativeApiAuthenticator()) {
    DCHECK(transport_availability_info_.has_win_native_api_authenticator);
    transport_availability_info_.win_native_api_authenticator_id =
        authenticator->GetId();
    transport_availability_info_
        .win_native_ui_shows_resident_credential_notice =
        static_cast<WinWebAuthnApiAuthenticator*>(authenticator)
            ->ShowsPrivacyNotice();
  }
#endif  // defined(OS_WIN)
}

bool FidoRequestHandlerBase::HasAuthenticator(
    const std::string& authenticator_id) const {
  return base::Contains(active_authenticators_, authenticator_id);
}

void FidoRequestHandlerBase::FillHasRecognizedPlatformCredential(
    base::OnceCallback<void()> done_callback) {
  std::move(done_callback).Run();
}

void FidoRequestHandlerBase::NotifyObserverTransportAvailability() {
  DCHECK(observer_);
  observer_->OnTransportAvailabilityEnumerated(transport_availability_info_);
}

void FidoRequestHandlerBase::InitializeAuthenticatorAndDispatchRequest(
    const std::string& authenticator_id) {
  auto authenticator_it = active_authenticators_.find(authenticator_id);
  if (authenticator_it == active_authenticators_.end()) {
    return;
  }
  FidoAuthenticator* authenticator = authenticator_it->second;
  authenticator->InitializeAuthenticator(
      base::BindOnce(&FidoRequestHandlerBase::DispatchRequest,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void FidoRequestHandlerBase::ConstructBleAdapterPowerManager() {
  bluetooth_adapter_manager_ = std::make_unique<BleAdapterManager>(this);
}

void FidoRequestHandlerBase::OnHasRecognizedPlatformCredentialFilled() {
  notify_observer_callback_.Run();
}

void FidoRequestHandlerBase::StopDiscoveries() {
  for (const auto& discovery : discoveries_) {
    discovery->MaybeStop();
  }
}

constexpr base::TimeDelta
    FidoRequestHandlerBase::kMinExpectedAuthenticatorResponseTime;

}  // namespace device

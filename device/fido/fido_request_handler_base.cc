// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_request_handler_base.h"

#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/ble_adapter_manager.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/mac/icloud_keychain.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/util.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/process/process_info.h"
#include "device/fido/mac/util.h"
#endif

namespace device {

namespace {

bool IsGpmPasskeyAuthenticator(const FidoAuthenticator& authenticator) {
  switch (authenticator.GetType()) {
    case AuthenticatorType::kWinNative:
    case AuthenticatorType::kTouchID:
    case AuthenticatorType::kChromeOS:
    case AuthenticatorType::kPhone:
    case AuthenticatorType::kICloudKeychain:
    case AuthenticatorType::kOther:
      return false;
    case AuthenticatorType::kEnclave:
    case AuthenticatorType::kChromeOSPasskeys:
      return true;
  }
  NOTREACHED();
}

void MaybeRecordPlatformCredentialStatus(AuthenticatorType type,
                                         base::TimeDelta elapsed_time) {
  std::string metric_name;

  switch (type) {
    case AuthenticatorType::kWinNative:
      metric_name = "WebAuthentication.CredentialFetchDuration.WinHello";
      break;
    case AuthenticatorType::kTouchID:
      metric_name = "WebAuthentication.CredentialFetchDuration.TouchId";
      break;
    case AuthenticatorType::kChromeOS:
      metric_name = "WebAuthentication.CredentialFetchDuration.ChromeOS";
      break;
    case AuthenticatorType::kICloudKeychain:
      metric_name = "WebAuthentication.CredentialFetchDuration.ICloudKeychain";
      break;
    default:
      return;
  }

  base::UmaHistogramTimes(metric_name, elapsed_time);
}

}  // namespace

// TransportAvailabilityCallbackReadiness stores state that tracks whether
// |FidoRequestHandlerBase| is ready to call
// |OnTransportAvailabilityEnumerated|.
struct TransportAvailabilityCallbackReadiness {
  // callback_made is true if the |OnTransportAvailabilityEnumerated| callback
  // has been made.
  bool callback_made = false;

  // ble_information_pending is true if the |OnTransportAvailabilityEnumerated|
  // callback is pending BLE status information.
  bool ble_information_pending = false;

  // num_platform_credential_checks_pending is true if the
  // |OnTransportAvailabilityEnumerated| callback is pending
  // |OnHasRecognizedPlatformCredentialFilled| being called after the platform
  // authenticator has decided if it has credentials that are responsive to the
  // request.
  unsigned num_platform_credential_checks_pending = 0;

  // win_is_uvpaa_check_pending is true if |OnTransportAvailabilityEnumerated|
  // callback is pending |OnIsUvpaa| being called.
  bool win_is_uvpaa_check_pending = false;

  // platform_biometrics_check_pending is set if an asynchronous check for
  // local biometric availability is pending.
  bool platform_biometrics_check_pending = false;

  // num_discoveries_pending is the number of discoveries that are still yet to
  // signal that they have started.
  unsigned num_discoveries_pending = 0;

  // This separately counts for platform discoveries in order to track whether
  // at least one discovery succeeded, for situations where there is more than
  // one platform authenticator available.
  unsigned num_platform_discoveries_pending = 0;
  bool platform_discovery_succeeded = false;

  bool CanMakeCallback() const {
    return !callback_made && !ble_information_pending &&
           num_platform_credential_checks_pending == 0 &&
           !win_is_uvpaa_check_pending && !platform_biometrics_check_pending &&
           num_discoveries_pending == 0;
  }
};

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

namespace {

std::string TransportsToString(
    const base::flat_set<device::FidoTransportProtocol>& transports) {
  std::vector<std::string> strings;
  base::ranges::transform(transports, std::back_inserter(strings), [](auto t) {
    return base::NumberToString(static_cast<int>(t));
  });
  return base::JoinString(strings, ",");
}

std::ostream& operator<<(std::ostream& os,
                         const device::DiscoverableCredentialMetadata& cred) {
  return os << "{" << static_cast<int>(cred.source) << ","
            << base::HexEncode(cred.cred_id) << "}";
}

}  // namespace

// TODO b/366128135: Revert the CL that introduced this and all associated
// logging once root cause for this bug has been established.
std::ostream& operator<<(
    std::ostream& os,
    const FidoRequestHandlerBase::TransportAvailabilityInfo& t) {
  os << "{available_transports={" << TransportsToString(t.available_transports)
     << "}, has_platform_authenticator_credential="
     << static_cast<int>(t.has_platform_authenticator_credential)
     << ", recognized_credentials=(" << t.recognized_credentials.size() << "){";
  for (const device::DiscoverableCredentialMetadata& cred :
       t.recognized_credentials) {
    os << cred << ",";
  }
  return os << "}}";
}

// FidoRequestHandlerBase::Observer -------------------------------------------

FidoRequestHandlerBase::Observer::~Observer() = default;

// FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls --------------------------

static bool g_always_allow_ble_calls = false;

FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls::ScopedAlwaysAllowBLECalls() {
  CHECK(!g_always_allow_ble_calls);
  g_always_allow_ble_calls = true;
}

FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls::
    ~ScopedAlwaysAllowBLECalls() {
  CHECK(g_always_allow_ble_calls);
  g_always_allow_ble_calls = false;
}

// FidoRequestHandlerBase -----------------------------------------------------

FidoRequestHandlerBase::FidoRequestHandlerBase()
    : transport_availability_callback_readiness_(
          new TransportAvailabilityCallbackReadiness) {}

FidoRequestHandlerBase::FidoRequestHandlerBase(
    FidoDiscoveryFactory* fido_discovery_factory,
    const base::flat_set<FidoTransportProtocol>& available_transports)
    : device::FidoRequestHandlerBase(fido_discovery_factory,
                                     /*additional_discoveries=*/{},
                                     available_transports) {}

FidoRequestHandlerBase::FidoRequestHandlerBase(
    FidoDiscoveryFactory* fido_discovery_factory,
    std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
    const base::flat_set<FidoTransportProtocol>& available_transports)
    : FidoRequestHandlerBase() {
  InitDiscoveries(fido_discovery_factory, std::move(additional_discoveries),
                  available_transports,
                  /*consider_enclave=*/true);
}

void FidoRequestHandlerBase::InitDiscoveries(
    FidoDiscoveryFactory* fido_discovery_factory,
    std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
    base::flat_set<FidoTransportProtocol> available_transports,
    bool consider_enclave) {
  FIDO_LOG(DEBUG) << "InitDiscoveries() transports="
                  << TransportsToString(available_transports);

#if BUILDFLAG(IS_WIN)
  // Try to instantiate the discovery for proxying requests to the native
  // Windows WebAuthn API; or fall back to using the regular device transport
  // discoveries if the API is unavailable.
  auto win_discovery =
      fido_discovery_factory->MaybeCreateWinWebAuthnApiDiscovery();
  if (win_discovery) {
    // The Windows WebAuthn API is available. On this platform, communicating
    // with authenticator devices directly is blocked by the OS, so we need to
    // go through the native API instead. No device discoveries may be
    // instantiated. The embedder will be responsible for dispatch of the
    // authenticator and whether they display any UI in addition to the one
    // provided by the OS.
    win_discovery->set_observer(this);
    discoveries_.push_back(std::move(win_discovery));

    transport_availability_info_.has_win_native_api_authenticator = true;
    transport_availability_callback_readiness_->win_is_uvpaa_check_pending =
        true;
    WinWebAuthnApiAuthenticator::IsUserVerifyingPlatformAuthenticatorAvailable(
        device::WinWebAuthnApi::GetDefault(),
        base::BindOnce(&FidoRequestHandlerBase::OnWinIsUvpaa,
                       weak_factory_.GetWeakPtr()));

    // Allow caBLE as a potential additional transport if requested by
    // the implementing class because it is not subject to the OS'
    // device communication block (only GetAssertionRequestHandler uses
    // caBLE). Otherwise, do not instantiate any other transports.
    base::EraseIf(available_transports, [](auto transport) {
      return transport != FidoTransportProtocol::kHybrid;
    });
  }
#endif  // BUILDFLAG(IS_WIN)

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

  // `additional_discoveries` are injected by
  // AuthenticatorRequestClientDelegate.
  for (auto& discovery : additional_discoveries) {
    // TODO: Make this work better for non-standard discoveries like Windows,
    // which currently pretends to be `kInternal`.
    if (!base::Contains(available_transports, discovery->transport())) {
      continue;
    }
    discovery->set_observer(this);
    discoveries_.emplace_back(std::move(discovery));
  }

  if (consider_enclave) {
    std::optional<std::unique_ptr<FidoDiscoveryBase>> enclave_discovery =
        fido_discovery_factory->MaybeCreateEnclaveDiscovery();
    if (enclave_discovery) {
      enclave_discovery.value()->set_observer(this);
      discoveries_.emplace_back(std::move(*enclave_discovery));
    }
  }

  for (auto& discovery : discoveries_) {
    if (discovery->transport() == FidoTransportProtocol::kInternal) {
      transport_availability_callback_readiness_
          ->num_platform_discoveries_pending++;
    }
  }
  transport_availability_callback_readiness_->num_discoveries_pending =
      discoveries_.size();

#if BUILDFLAG(IS_MAC)
  // On recent macOS a process must have listed Bluetooth metadata in its
  // Info.plist in order to call Bluetooth APIs. Failure to do so results in
  // the system killing with process with SIGABRT once Bluetooth calls are
  // made.
  //
  // However, unless Chromium is started from the Finder, or with special
  // posix_spawn flags, then the responsible process—the one that needs to have
  // the right Info.plist—is one of the parent processes, often the terminal
  // emulator. This can lead to Chromium getting killed when trying to do
  // WebAuthn. This also affects layout tests.
  //
  // Thus, if the responsible process is not Chromium itself, then we do not
  // make any Bluetooth API calls.
  const bool can_call_ble_apis =
      g_always_allow_ble_calls || base::IsProcessSelfResponsible();
  if (!can_call_ble_apis) {
    FIDO_LOG(ERROR) << "Cannot test Bluetooth power status because process is "
                       "not self-responsible. Launch from Finder to fix.";
  }
#else
  const bool can_call_ble_apis = true;
#endif

  // Check if the platform supports BLE before trying to get a power manager.
  // CaBLE might be in |available_transports| without actual BLE support under
  // the virtual environment.
  // TODO(nsatragno): Move the BLE power manager logic to CableDiscoveryFactory
  // so we don't need this additional check.
  if (can_call_ble_apis &&
      device::BluetoothAdapterFactory::Get()->IsLowEnergySupported() &&
      base::Contains(transport_availability_info_.available_transports,
                     FidoTransportProtocol::kHybrid)) {
    transport_availability_callback_readiness_->ble_information_pending = true;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FidoRequestHandlerBase::ConstructBleAdapterPowerManager,
                       weak_factory_.GetWeakPtr()));
  }

  FIDO_LOG(DEBUG) << "InitDiscoveries() complete "
                  << transport_availability_info_;

#if BUILDFLAG(IS_MAC)
  transport_availability_info_.platform_has_biometrics =
      device::fido::mac::DeviceHasBiometricsAvailable();
  MaybeSignalTransportsEnumerated();
#elif BUILDFLAG(IS_WIN)
  transport_availability_callback_readiness_
      ->platform_biometrics_check_pending = true;
  device::fido::win::DeviceHasBiometricsAvailable(base::BindOnce(
      [](base::WeakPtr<FidoRequestHandlerBase> handler,
         bool biometrics_available) {
        if (!handler) {
          return;
        }
        handler->transport_availability_info_.platform_has_biometrics =
            biometrics_available;
        handler->transport_availability_callback_readiness_
            ->platform_biometrics_check_pending = false;
        handler->MaybeSignalTransportsEnumerated();
      },
      GetWeakPtr()));
#else
  MaybeSignalTransportsEnumerated();
#endif
}

FidoRequestHandlerBase::~FidoRequestHandlerBase() {
  CancelActiveAuthenticators();
}

void FidoRequestHandlerBase::StartAuthenticatorRequest(
    const std::string& authenticator_id) {
  InitializeAuthenticatorAndDispatchRequest(authenticator_id);
}

void FidoRequestHandlerBase::CancelActiveAuthenticators(
    std::string_view exclude_device_id) {
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
    BleStatus ble_status,
    bool can_power_on,
    bool is_peripheral_role_supported) {
  if (!is_present) {
    transport_availability_info_.available_transports.erase(
        FidoTransportProtocol::kHybrid);
  }

  transport_availability_callback_readiness_->ble_information_pending = false;
  transport_availability_info_.ble_status = ble_status;
  transport_availability_info_.can_power_on_ble_adapter = can_power_on;
  MaybeSignalTransportsEnumerated();
}

void FidoRequestHandlerBase::OnBluetoothAdapterStatusChanged(
    BleStatus ble_status) {
  transport_availability_info_.ble_status = ble_status;

  if (observer_) {
    observer_->BluetoothAdapterStatusChanged(ble_status);
  }
}

void FidoRequestHandlerBase::PowerOnBluetoothAdapter() {
  if (!bluetooth_adapter_manager_) {
    return;
  }

  bluetooth_adapter_manager_->SetAdapterPower(true /* set_power_on */);
}

void FidoRequestHandlerBase::RequestBluetoothPermission(
    BlePermissionCallback callback) {
  return bluetooth_adapter_manager_->RequestBluetoothPermission(
      std::move(callback));
}

base::WeakPtr<FidoRequestHandlerBase> FidoRequestHandlerBase::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void FidoRequestHandlerBase::set_observer(
    FidoRequestHandlerBase::Observer* observer) {
  DCHECK(!observer_) << "Only one observer is supported.";
  observer_ = observer;

  MaybeSignalTransportsEnumerated();
}

void FidoRequestHandlerBase::Start() {
  for (const auto& discovery : discoveries_) {
    discovery->Start();
  }
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
  transport_availability_callback_readiness_->num_discoveries_pending--;

  bool is_platform_discovery =
      discovery->transport() == FidoTransportProtocol::kInternal;
  if (is_platform_discovery) {
    CHECK(transport_availability_callback_readiness_
              ->num_platform_discoveries_pending > 0);
    transport_availability_callback_readiness_
        ->num_platform_discoveries_pending--;
  }

  if (!success) {
    if (!is_platform_discovery ||
        (transport_availability_callback_readiness_
                 ->num_platform_discoveries_pending == 0 &&
         !transport_availability_callback_readiness_
              ->platform_discovery_succeeded)) {
      transport_availability_info_.available_transports.erase(
          discovery->transport());
    }
  } else {
    for (auto* authenticator : authenticators) {
      AuthenticatorAdded(discovery, authenticator);
    }

    // Allow GetAssertionRequestHandler to asynchronously check for known
    // platform credentials and defer |OnTransportAvailabilityEnumerated| until
    // that check is done.
    // |authenticators| can be empty in tests.
    if (is_platform_discovery && !authenticators.empty()) {
      transport_availability_callback_readiness_->platform_discovery_succeeded =
          true;
      for (FidoAuthenticator* platform_authenticator : authenticators) {
        if (IsGpmPasskeyAuthenticator(*platform_authenticator)) {
          // GPM credential availability is checked in
          // ChromeAuthenticatorRequestDelegate, so the authenticators don't
          // implement GetPlatformCredentialStatus.
          continue;
        }
        transport_availability_info_.has_icloud_keychain |=
            platform_authenticator->GetType() ==
            AuthenticatorType::kICloudKeychain;
        transport_availability_callback_readiness_
            ->num_platform_credential_checks_pending++;
        GetPlatformCredentialStatus(platform_authenticator);
      }
    }
  }

  MaybeSignalTransportsEnumerated();
}

void FidoRequestHandlerBase::AuthenticatorAdded(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK(!authenticator->GetId().empty());
  bool was_inserted;
  std::tie(std::ignore, was_inserted) =
      active_authenticators_.insert({authenticator->GetId(), authenticator});
  if (!was_inserted) {
    NOTREACHED() << "Authenticator with duplicate ID "
                 << authenticator->GetId();
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FidoRequestHandlerBase::InitializeAuthenticatorAndDispatchRequest,
            GetWeakPtr(), authenticator->GetId()));
  } else {
    VLOG(2) << "Embedder controls the dispatch.";
  }

#if BUILDFLAG(IS_WIN)
  if (authenticator->GetType() == AuthenticatorType::kWinNative) {
    DCHECK(transport_availability_info_.has_win_native_api_authenticator);
    transport_availability_info_
        .win_native_ui_shows_resident_credential_notice =
        static_cast<WinWebAuthnApiAuthenticator*>(authenticator)
            ->ShowsResidentCredentialNotice();
  }
#endif  // BUILDFLAG(IS_WIN)
}

void FidoRequestHandlerBase::GetPlatformCredentialStatus(
    FidoAuthenticator* platform_authenticator) {
  transport_availability_callback_readiness_
      ->num_platform_credential_checks_pending--;
}

void FidoRequestHandlerBase::OnHavePlatformCredentialStatus(
    AuthenticatorType authenticator_type,
    std::optional<base::ElapsedTimer> timer,
    std::vector<DiscoverableCredentialMetadata> creds,
    RecognizedCredential has_credentials) {
  if (creds.size() > 0 && timer.has_value()) {
    MaybeRecordPlatformCredentialStatus(authenticator_type, timer->Elapsed());
  }

  if (authenticator_type == AuthenticatorType::kICloudKeychain) {
    // iCloud Keychain is the second platform authenticator on the system and
    // its status is reported via a different field.
    DCHECK_EQ(transport_availability_info_.has_icloud_keychain_credential,
              RecognizedCredential::kNoRecognizedCredential);
    transport_availability_info_.has_icloud_keychain_credential =
        has_credentials;
  } else {
    DCHECK_EQ(
        transport_availability_info_.has_platform_authenticator_credential,
        RecognizedCredential::kNoRecognizedCredential);
    transport_availability_info_.has_platform_authenticator_credential =
        has_credentials;
    if (has_credentials == RecognizedCredential::kNoRecognizedCredential) {
      transport_availability_info_.available_transports.erase(
          FidoTransportProtocol::kInternal);
    }
  }

  auto& out_creds = transport_availability_info_.recognized_credentials;
  if (out_creds.empty()) {
    out_creds = std::move(creds);
  } else if (!creds.empty()) {
    out_creds.insert(out_creds.end(), creds.begin(), creds.end());
  }

  transport_availability_callback_readiness_
      ->num_platform_credential_checks_pending--;
  MaybeSignalTransportsEnumerated();
}

bool FidoRequestHandlerBase::HasAuthenticator(
    const std::string& authenticator_id) const {
  return base::Contains(active_authenticators_, authenticator_id);
}

void FidoRequestHandlerBase::MaybeSignalTransportsEnumerated() {
  if (!observer_ ||
      !transport_availability_callback_readiness_->CanMakeCallback()) {
    return;
  }

  transport_availability_callback_readiness_->callback_made = true;
  FIDO_LOG(DEBUG) << "TransportsEnumerated: " << transport_availability_info_;
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

void FidoRequestHandlerBase::OnWinIsUvpaa(bool is_uvpaa) {
  transport_availability_info_.win_is_uvpaa = is_uvpaa;
  transport_availability_callback_readiness_->win_is_uvpaa_check_pending =
      false;
  MaybeSignalTransportsEnumerated();
}

void FidoRequestHandlerBase::StopDiscoveries() {
  for (const auto& discovery : discoveries_) {
    discovery->Stop();
  }
}

constexpr base::TimeDelta
    FidoRequestHandlerBase::kMinExpectedAuthenticatorResponseTime;

}  // namespace device

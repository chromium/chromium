// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"

namespace device {

class BleAdapterManager;
class FidoAuthenticator;
class FidoDiscoveryFactory;
class DiscoverableCredentialMetadata;
struct TransportAvailabilityCallbackReadiness;

// Base class that handles authenticator discovery/removal. Its lifetime is
// equivalent to that of a single WebAuthn request. For each authenticator, the
// per-device work is carried out by one FidoAuthenticator instance, which is
// constructed in a FidoDiscoveryBase and passed to the request handler via its
// Observer interface.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoRequestHandlerBase
    : public FidoDiscoveryBase::Observer {
 public:
  using RequestCallback = base::RepeatingCallback<void(const std::string&)>;

  using AuthenticatorMap = std::map<std::string,
                                    raw_ptr<FidoAuthenticator, CtnExperimental>,
                                    std::less<>>;

  // BLE adapter status.
  enum class BleStatus {
    // The adapter is turned on.
    kOn,

    // The adapter is turned off.
    kOff,

    // The user has denied Chrome the permission to use bluetooth.
    kPermissionDenied,

    // Chrome has not yet requested permission to use bluetooth.
    kPendingPermissionRequest,
  };

  using BlePermissionCallback = base::OnceCallback<void(BleStatus)>;

  enum class RecognizedCredential {
    kUnknown,
    kHasRecognizedCredential,
    kNoRecognizedCredential
  };

  // Encapsulates data required to initiate WebAuthN UX dialog. Once all
  // components of TransportAvailabilityInfo is set,
  // AuthenticatorRequestClientDelegate should be notified.
  struct COMPONENT_EXPORT(DEVICE_FIDO) TransportAvailabilityInfo {
    enum class ConditionalUITreatment {
      kDefault = 0,
      // kDontShowEmptyConditionalUI requests that, if there are no matching
      // credentials for conditional UI, that the option to use a passkey from
      // another device not be offered. This is for measurement and
      // experimentation.
      kDontShowEmptyConditionalUI,
      // kNeverOfferPasskeyFromAnotherDevice requests that the option to use a
      // passkey from another device is never offered in conditional UI. This is
      // for measurement and experimentation.
      kNeverOfferPasskeyFromAnotherDevice,
    };

    TransportAvailabilityInfo();
    TransportAvailabilityInfo(const TransportAvailabilityInfo& other);
    TransportAvailabilityInfo& operator=(
        const TransportAvailabilityInfo& other);
    ~TransportAvailabilityInfo();

    FidoRequestType request_type = FidoRequestType::kMakeCredential;

    // Indicates whether this is a GetAssertion request with an empty allow
    // list.
    bool has_empty_allow_list = false;

    // is_only_hybrid_or_internal is true if credentials in the allow-list only
    // contain the hybrid or internal transports.
    bool is_only_hybrid_or_internal = false;

    // True this process has iCloud Keychain support. Only meaningful on macOS.
    bool has_icloud_keychain = false;

    // The intersection of transports supported by the client and allowed by the
    // relying party.
    base::flat_set<FidoTransportProtocol> available_transports;

    // Whether the platform authenticator has a matching credential for the
    // request. This is only set for a GetAssertion request.
    RecognizedCredential has_platform_authenticator_credential =
        RecognizedCredential::kNoRecognizedCredential;

    // This field mirrors the previous one but is specific to iCloud
    // Keychain. They are separate because a macOS system can have both the
    // Chromium platform authenticator and iCloud Keychain as platform
    // authenticators.
    RecognizedCredential has_icloud_keychain_credential =
        RecognizedCredential::kNoRecognizedCredential;

    // The set of recognized credential user entities that can fulfill a
    // GetAssertion request. Not all authenticators report this, so the set
    // might be empty even if |has_platform_authenticator_credential| is
    // |kHasRecognizedCredential|.
    std::vector<DiscoverableCredentialMetadata> recognized_credentials;

    BleStatus ble_status = BleStatus::kOff;

    bool can_power_on_ble_adapter = false;

    // Indicates whether the native Windows WebAuthn API is available.
    // Dispatching to it should be controlled by the embedder.
    //
    // The embedder:
    //  - may choose not to dispatch immediately if caBLE is available
    //  - should dispatch immediately if no other transport is available
    bool has_win_native_api_authenticator = false;

    // Indicates whether the Windows native UI will include a privacy notice
    // when creating a resident credential.
    bool win_native_ui_shows_resident_credential_notice = false;

    // Whether the native Windows API reports that a user verifying platform
    // authenticator is available.
    bool win_is_uvpaa = false;

    // Whether the platform can check biometrics and has biometrics configured.
    bool platform_has_biometrics = false;

    // Indicates whether the request is occurring in an off-the-record
    // BrowserContext (e.g. Chrome Incognito mode).
    bool is_off_the_record_context = false;

    // Indicates the ResidentKeyRequirement of the current request. Only valid
    // if |request_type| is |RequestType::kMakeCredential|. Requests with a
    // value of |ResidentKeyRequirement::kPreferred| or
    // |ResidentKeyRequirement::kRequired| can create a resident credential,
    // which could be discovered by someone with physical access to the
    // authenticator and thus have privacy implications.
    ResidentKeyRequirement resident_key_requirement =
        ResidentKeyRequirement::kDiscouraged;

    // Indicates the UserVerificationRequirement of the current request.
    UserVerificationRequirement user_verification_requirement =
        UserVerificationRequirement::kDiscouraged;

    // The attestation preference. Present if, and only if, |request_type| is
    // |kMakeCredential|.
    std::optional<AttestationConveyancePreference>
        attestation_conveyance_preference;

    // transport_list_did_include_internal is set to true during a getAssertion
    // request if at least one element of the allowList included the "internal"
    // transport, or didn't have any transports.
    //
    // An embedder may use this to show a more precise UI when no transports
    // are available. If the lack of transports is because the allowList only
    // contained NFC-based credentials, and there's no NFC support, then that
    // might be meaningfully different from the case where the allowList
    // contained credentials that could have been on the local device but
    // weren't.
    bool transport_list_did_include_internal = false;

    // transport_list_did_include_hybrid is set to true during a getAssertion
    // request if at least one element of the allowList included the "hybrid"
    // transport, or didn't have any transports.
    bool transport_list_did_include_hybrid = false;

    // transport_list_did_include_security_key is set to true during a
    // getAssertion request if at least one element of the allowList included
    // the "usb", "nfc", or "ble" transport, or didn't have any transports.
    bool transport_list_did_include_security_key = false;

    // request_is_internal_only indicates that this request can only be serviced
    // by internal authenticators (e.g. due to the attachment setting).
    // See also `make_credential_attachment`.
    bool request_is_internal_only = false;

    // make_credential_attachment contains the attachment preference for
    // makeCredential requests. See also `request_is_internal_only`, which isn't
    // specific to makeCredential requests.
    std::optional<AuthenticatorAttachment> make_credential_attachment;

    // conditional_ui_treatment_ controls how conditional UI will be handled for
    // this request.
    ConditionalUITreatment conditional_ui_treatment =
        ConditionalUITreatment::kDefault;

    COMPONENT_EXPORT(DEVICE_FIDO)
    friend std::ostream& operator<<(std::ostream& os,
                                    const TransportAvailabilityInfo& t);
  };

  class COMPONENT_EXPORT(DEVICE_FIDO) Observer {
   public:
    struct COMPONENT_EXPORT(DEVICE_FIDO) CollectPINOptions {
      // Why this PIN is being collected.
      pin::PINEntryReason reason;

      // The error for which we are prompting for a PIN.
      pin::PINEntryError error = pin::PINEntryError::kNoError;

      // The minimum PIN length the authenticator will accept for the PIN.
      uint32_t min_pin_length = device::kMinPinLength;

      // The number of attempts remaining before a hard lock. Should be ignored
      // unless |mode| is kChallenge.
      int attempts = 0;
    };

    virtual ~Observer();

    // This method will not be invoked until the observer is set.
    virtual void OnTransportAvailabilityEnumerated(
        TransportAvailabilityInfo data) = 0;

    // If true, the request handler will defer dispatch of its request onto the
    // given authenticator to the embedder. The embedder needs to call
    // |StartAuthenticatorRequest| when it wants to initiate request dispatch.
    //
    // This method is invoked before |FidoAuthenticatorAdded|, and may be
    // invoked multiple times for the same authenticator. Depending on the
    // result, the request handler might decide not to make the authenticator
    // available, in which case it never gets passed to
    // |FidoAuthenticatorAdded|.
    virtual bool EmbedderControlsAuthenticatorDispatch(
        const FidoAuthenticator& authenticator) = 0;

    virtual void BluetoothAdapterStatusChanged(BleStatus ble_status) = 0;
    virtual void FidoAuthenticatorAdded(
        const FidoAuthenticator& authenticator) = 0;
    virtual void FidoAuthenticatorRemoved(std::string_view device_id) = 0;

    // SupportsPIN returns true if this observer supports collecting a PIN from
    // the user. If this function returns false, |CollectPIN| and
    // |FinishCollectPIN| will not be called.
    virtual bool SupportsPIN() const = 0;

    // CollectPIN is called when a PIN is needed to complete a request. The
    // |attempts| parameter is either |nullopt| to indicate that the user needs
    // to set a PIN, or contains the number of PIN attempts remaining before a
    // hard lock.
    virtual void CollectPIN(
        CollectPINOptions options,
        base::OnceCallback<void(std::u16string)> provide_pin_cb) = 0;

    virtual void FinishCollectToken() = 0;

    // Called when a biometric enrollment may be completed as part of the
    // request and the user should be notified to collect samples.
    // |next_callback| must be executed asynchronously at any time to move on to
    // the next step of the request.
    virtual void StartBioEnrollment(base::OnceClosure next_callback) = 0;

    // Called when a biometric enrollment sample has been collected.
    // |bio_samples_remaining| is the number of samples needed to finish the
    // enrollment.
    virtual void OnSampleCollected(int bio_samples_remaining) = 0;

    // Called when an authenticator reports internal user verification has
    // failed (e.g. not recognising the user's fingerprints) and the user should
    // try again. Receives the number of |attempts| before the device locks
    // internal user verification.
    virtual void OnRetryUserVerification(int attempts) = 0;
  };

  // ScopedAlwaysAllowBLECalls allows BLE API calls to always be made, even if
  // they would be disabled on macOS because Chromium was not launched with
  // self-responsibility.
  class COMPONENT_EXPORT(DEVICE_FIDO) ScopedAlwaysAllowBLECalls {
   public:
    ScopedAlwaysAllowBLECalls();
    ~ScopedAlwaysAllowBLECalls();
  };

  FidoRequestHandlerBase();

  // The |available_transports| should be the intersection of transports
  // supported by the client and allowed by the relying party.
  FidoRequestHandlerBase(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& available_transports);

  FidoRequestHandlerBase(
      FidoDiscoveryFactory* fido_discovery_factory,
      std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
      const base::flat_set<FidoTransportProtocol>& available_transports);

  FidoRequestHandlerBase(const FidoRequestHandlerBase&) = delete;
  FidoRequestHandlerBase& operator=(const FidoRequestHandlerBase&) = delete;

  ~FidoRequestHandlerBase() override;

  // Triggers DispatchRequest() if |active_authenticators_| hold
  // FidoAuthenticator with given |authenticator_id|.
  void StartAuthenticatorRequest(const std::string& authenticator_id);

  // Invokes |FidoAuthenticator::Cancel| on all authenticators, except if
  // matching |exclude_id|, if one is provided. Cancelled authenticators are
  // immediately removed from |active_authenticators_|.
  //
  // This function is invoked either when: (a) the entire WebAuthn API request
  // is canceled or, (b) a successful response or "invalid state error" is
  // received from the any one of the connected authenticators, in which case
  // all other authenticators are cancelled.
  // https://w3c.github.io/webauthn/#iface-pkcredential
  void CancelActiveAuthenticators(std::string_view exclude_id = "");
  virtual void OnBluetoothAdapterEnumerated(bool is_present,
                                            BleStatus ble_status,
                                            bool can_power_on,
                                            bool is_peripheral_role_supported);
  void OnBluetoothAdapterStatusChanged(BleStatus ble_status);
  void PowerOnBluetoothAdapter();

  // Queries the OS for the status of the Bluetooth adapter. This is useful on
  // macOS when TransportAvailabilityInfo::ble_status reports
  // kPendingPermissionRequest, in which case the OS will display a blocking
  // permissions prompt. Once the user allows or denies the prompt, |callback|
  // will be executed with the result.
  void RequestBluetoothPermission(BlePermissionCallback callback);

  base::WeakPtr<FidoRequestHandlerBase> GetWeakPtr();

  void set_observer(Observer* observer);

  // Returns whether FidoAuthenticator with id equal to |authenticator_id|
  // exists. Fake FidoRequestHandler objects used in testing overrides this
  // function to simulate scenarios where authenticator with |authenticator_id|
  // is known to the system.
  virtual bool HasAuthenticator(const std::string& authentiator_id) const;

  TransportAvailabilityInfo& transport_availability_info() {
    return transport_availability_info_;
  }

  const AuthenticatorMap& AuthenticatorsForTesting() {
    return active_authenticators_;
  }

  std::unique_ptr<BleAdapterManager>&
  get_bluetooth_adapter_manager_for_testing() {
    return bluetooth_adapter_manager_;
  }

  void StopDiscoveries();

 protected:
  // Authenticators that return a response in less than this time are likely to
  // have done so without interaction from the user.
  static constexpr base::TimeDelta kMinExpectedAuthenticatorResponseTime =
      base::Milliseconds(300);

  // Subclasses implement this method to dispatch their request onto the given
  // FidoAuthenticator. The FidoAuthenticator is owned by this
  // FidoRequestHandler and stored in active_authenticators().
  virtual void DispatchRequest(FidoAuthenticator*) = 0;

  void InitDiscoveries(
      FidoDiscoveryFactory* fido_discovery_factory,
      std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
      base::flat_set<FidoTransportProtocol> available_transports,
      bool consider_enclave);

  void Start();

  AuthenticatorMap& active_authenticators() { return active_authenticators_; }
  std::vector<std::unique_ptr<FidoDiscoveryBase>>& discoveries() {
    return discoveries_;
  }
  Observer* observer() const { return observer_; }

  // FidoDiscoveryBase::Observer
  void DiscoveryStarted(
      FidoDiscoveryBase* discovery,
      bool success,
      std::vector<FidoAuthenticator*> authenticators) override;
  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  // GetPlatformCredentialStatus is called to learn whether a platform
  // authenticator has credentials responsive to the current request. If this
  // method is overridden in a subclass then either:
  //  · The method in this base class must be called immediately, or
  //  · |OnHavePlatformCredentialStatus| must eventually called.
  //
  // This method runs only after the platform discovery has started
  // successfully. (The Windows API doesn't count as a platform authenticator
  // for the purposes of this call.)
  virtual void GetPlatformCredentialStatus(
      FidoAuthenticator* platform_authenticator);

  // OnHavePlatformCredentialStatus is called by subclasses (after
  // `GetPlatformCredentialStatus` has been called) to report on whether the
  // platform authenticator whether it has responsive discoverable credentials
  // and whether it has responsive credentials at all.
  // `timer` allows recording metrics with the wait time for this callback.
  void OnHavePlatformCredentialStatus(
      AuthenticatorType authenticator_type,
      std::optional<base::ElapsedTimer> timer,
      std::vector<DiscoverableCredentialMetadata> user_entities,
      RecognizedCredential has_credentials);

 private:
  friend class FidoRequestHandlerTest;

  void MaybeSignalTransportsEnumerated();

  // Invokes FidoAuthenticator::InitializeAuthenticator(), followed by
  // DispatchRequest(). InitializeAuthenticator() sends a GetInfo command
  // to FidoDeviceAuthenticator instances in order to determine their protocol
  // versions before a request can be dispatched.
  void InitializeAuthenticatorAndDispatchRequest(
      const std::string& authenticator_id);
  void ConstructBleAdapterPowerManager();
  void OnWinIsUvpaa(bool is_uvpaa);

  AuthenticatorMap active_authenticators_;
  std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries_;
  raw_ptr<Observer> observer_ = nullptr;
  TransportAvailabilityInfo transport_availability_info_;
  std::unique_ptr<BleAdapterManager> bluetooth_adapter_manager_;

  // transport_availability_callback_readiness_ keeps track of state which
  // determines whether this object is ready to call
  // |OnTransportAvailabilityEnumerated| on |observer_|.
  std::unique_ptr<TransportAvailabilityCallbackReadiness>
      transport_availability_callback_readiness_;

  base::WeakPtrFactory<FidoRequestHandlerBase> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_

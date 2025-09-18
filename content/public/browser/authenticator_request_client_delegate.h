// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "url/gurl.h"

namespace device {
class FidoAuthenticator;
class FidoDiscoveryFactory;
class PublicKeyCredentialDescriptor;
class PublicKeyCredentialUserEntity;
}  // namespace device

namespace url {
class Origin;
}

namespace content {

// LINT.IfChange
// Reasons why a WebAuthn get() request with `mediation: "immediate"` was
// rejected by the browser before showing any UI.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ImmediateMediationRejectionReason {
  // The request was in an incognito/off-the-record profile.
  kIncognito = 0,
  // The request was rate-limited for the origin.
  kRateLimited = 1,
  // No credentials were found for the request.
  kNoCredentials = 2,
  // The request timed out before the UI could be shown.
  kTimeout = 3,
  kMaxValue = kTimeout,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

// AuthenticatorRequestClientDelegate is an interface that lets embedders
// customize the lifetime of a single WebAuthn API request in the //content
// layer. In particular, the Authenticator mojo service uses
// AuthenticatorRequestClientDelegate to show WebAuthn request UI.
class CONTENT_EXPORT AuthenticatorRequestClientDelegate
    : public device::FidoRequestHandlerBase::Observer {
 public:
  using AccountPreselectedCallback =
      base::RepeatingCallback<void(device::DiscoverableCredentialMetadata)>;
  using PasswordSelectedCallback =
      base::RepeatingCallback<void(password_manager::CredentialInfo)>;

  // Failure reasons that might be of interest to the user, so the embedder may
  // decide to inform the user.
  enum class InterestingFailureReason {
    kTimeout,
    kKeyNotRegistered,
    kKeyAlreadyRegistered,
    kSoftPINBlock,
    kHardPINBlock,
    kAuthenticatorRemovedDuringPINEntry,
    kAuthenticatorMissingResidentKeys,
    kAuthenticatorMissingUserVerification,
    kAuthenticatorMissingLargeBlob,
    kNoCommonAlgorithms,
    // kStorageFull indicates that a resident credential could not be created
    // because the authenticator has insufficient storage.
    kStorageFull,
    kUserConsentDenied,
    // kWinUserCancelled means that the user clicked "Cancel" in the native
    // Windows UI.
    kWinUserCancelled,
    kHybridTransportError,
    kNoPasskeys,
    // kEnclaveError means that there was some error communicating with a
    // passkeys enclave. This is a fatal (like `kHybridTransportError` but
    // unlike security keys) because, like hybrid, the user has taken some
    // action to send the request to the enclave.
    kEnclaveError,
    // kEnclaveCancel means that the user canceled an enclave transaction.
    // At the time of writing the only way to trigger this is to cancel the
    // Windows Hello user verification dialog.
    kEnclaveCancel,
    // The request included a challenge URL but fetching the challenge failed.
    kChallengeUrlFailure,
  };

  // RequestSource enumerates the source of a request, which is either the Web
  // Authentication API (https://www.w3.org/TR/webauthn-2/), the Secure Payment
  // Authentication API (https://www.w3.org/TR/secure-payment-confirmation), or
  // a browser-internal use (which applies whenever
  // `AuthenticatorCommon::Create` is used).
  enum class RequestSource {
    kWebAuthentication,
    kSecurePaymentConfirmation,
    kInternal,
  };

  // Distinguishes different types of UI that can be shown for a WebAuthn
  // request.
  enum class UIPresentation {
    // The default tab-modal dialog shown for .get() and .create() request.
    kModal,
    // Tab modal for .get() requests with mediation = "immediate".
    kModalImmediate,
    // Passkey autofill UI for .get() requests with `mediation = "conditional"`.
    kAutofill,
    // Passkey upgrade request, i.e. .create() requests with `mediation =
    // "conditional"`.
    kPasskeyUpgrade,
    // No WebAuthn UI shown. This is used for some internal requests that
    // originate outside of WebAuthn (e.g. payments) and provide their own
    // request UI.
    kDisabled,
  };

  AuthenticatorRequestClientDelegate();

  AuthenticatorRequestClientDelegate(
      const AuthenticatorRequestClientDelegate&) = delete;
  AuthenticatorRequestClientDelegate& operator=(
      const AuthenticatorRequestClientDelegate&) = delete;

  ~AuthenticatorRequestClientDelegate() override;

  // SetRelyingPartyId sets the RP ID for this request. This is called after
  // |WebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride| is given the
  // opportunity to affect this value. For typical origins, the RP ID is just a
  // domain name, but
  // |WebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride| may return
  // other forms of strings.
  virtual void SetRelyingPartyId(const std::string& rp_id);

  // Configures the type of UI to be shown for this request.
  virtual void SetUIPresentation(UIPresentation ui_presentation);

  // Called when the request fails for the given |reason|.
  //
  // Embedders may return true if they want AuthenticatorImpl to hold off from
  // resolving the WebAuthn request with an error, e.g. because they want the
  // user to dismiss an error dialog first. In this case, embedders *must*
  // eventually invoke the FidoRequestHandlerBase::CancelCallback in order to
  // resolve the request. Returning false causes AuthenticatorImpl to resolve
  // the request with the error right away.
  virtual bool DoesBlockRequestOnFailure(InterestingFailureReason reason);

  // TransactionSuccessful is called when any WebAuthn get() or create() call
  // completes successfully.
  virtual void OnTransactionSuccessful(
      RequestSource request_source,
      device::FidoRequestType request_type,
      device::AuthenticatorType authenticator_type);

  // Supplies callbacks that the embedder can invoke to initiate certain
  // actions, namely: cancel the request, report no immediate mechanisms, start
  // the request over, preselect an account, dispatch request to connected
  // authenticators, power on the bluetooth adapter, and request permission to
  // use the bluetooth adapter.
  virtual void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::OnceClosure immediate_not_found_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      PasswordSelectedCallback password_selected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::OnceClosure cancel_ui_timeout_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      base::RepeatingCallback<
          void(device::FidoRequestHandlerBase::BlePermissionCallback)>
          request_ble_permission_callback);

  // ConfigureDiscoveries optionally configures |fido_discovery_factory|.
  //
  // |origin| is the origin of the calling site, |rp_id| is the relying party
  // identifier of the request, |request_type| is the type of the request and
  // |resident_key_requirement| (which is only set when provided, i.e. for
  // makeCredential calls) reflects the value requested by the site.
  //
  // For a create() request, |user_name| contains the contents of the
  // |user.name| field, which is set by the site.
  //
  // caBLE (also called the "hybrid" transport) must be configured in order to
  // be functional and |pairings_from_extension| contains any caBLEv1 pairings
  // that have been provided in an extension to the WebAuthn get() call.
  //
  // When `is_enclave_authenticator_available` is true, the embedder will
  // provide a cloud enclave authenticator option.
  //
  // Other FidoDiscoveryFactory fields (e.g. the `LAContextDropbox`) can also be
  // configured by this function.
  virtual void ConfigureDiscoveries(
      const url::Origin& origin,
      const std::string& rp_id,
      RequestSource request_source,
      device::FidoRequestType request_type,
      std::optional<device::ResidentKeyRequirement> resident_key_requirement,
      device::UserVerificationRequirement user_verification_requirement,
      std::optional<std::string_view> user_name,
      base::span<const device::CableDiscoveryData> pairings_from_extension,
      bool is_enclave_authenticator_available,
      device::FidoDiscoveryFactory* fido_discovery_factory);

  // Hints reflects the "hints" parameter that can be set on a request. See
  // https://w3c.github.io/webauthn/#enumdef-publickeycredentialhints
  struct Hints {
    // The site's preferred transport for this operation.
    std::optional<device::FidoTransportProtocol> transport;
  };

  // SetHints communicates the "hints" that were set in the request. See
  // https://w3c.github.io/webauthn/#enumdef-publickeycredentialhints
  virtual void SetHints(const Hints& hints);

  // SelectAccount is called to allow the embedder to select between one or more
  // accounts. This is triggered when the web page requests an unspecified
  // credential (by passing an empty allow-list). In this case, any accounts
  // will come from the authenticator's storage and the user should confirm the
  // use of any specific account before it is returned. The callback takes the
  // selected account, or else |cancel_callback| can be called.
  //
  // This is only called if |WebAuthenticationDelegate::SupportsResidentKeys|
  // returns true.
  virtual void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback);

  // Configures whether a virtual authenticator environment is enabled. The
  // embedder might choose to e.g. automate account selection under a virtual
  // environment.
  void SetVirtualEnvironment(bool virtual_environment);

  bool IsVirtualEnvironmentEnabled();

  // Set the credential types that are expected by the delegate. The types can
  // be used by the Ambient UI or modal requests. Credential types are defined
  // in `credential_type_flags.mojom`.
  virtual void SetCredentialTypes(int credential_type_flags);

  // Sets a credential filter for conditional mediation requests, which will
  // only allow passkeys with matching credential IDs to be displayed to the
  // user.
  virtual void SetCredentialIdFilter(
      std::vector<device::PublicKeyCredentialDescriptor> credential_list);

  // Optionally configures the user entity passed for a makeCredential request.
  virtual void SetUserEntityForMakeCredentialRequest(
      const device::PublicKeyCredentialUserEntity& user_entity);

  // Returns a list of `FidoDiscoveryBase` instances that can instantiate an
  // embedder-specific platform authenticator for handling WebAuthn requests.
  // The discoveries' `transport()` must be `FidoTransportProtocol::kInternal`.
  virtual std::vector<std::unique_ptr<device::FidoDiscoveryBase>>
  CreatePlatformDiscoveries();

  // Provides a URL from which the challenge for an assertion request may
  // be retrieved. The callback is invoked once the challenge is received or
  // an error is encountered. In the case of an error it passes nullopt.
  virtual void ProvideChallengeUrl(
      const GURL& url,
      base::OnceCallback<void(std::optional<base::span<const uint8_t>>)>
          callback);

  // device::FidoRequestHandlerBase::Observer:
  void StartObserving(device::FidoRequestHandlerBase* request_handler) override;
  void StopObserving(device::FidoRequestHandlerBase* request_handler) override;
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override;
  void BluetoothAdapterStatusChanged(
      device::FidoRequestHandlerBase::BleStatus ble_status) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(std::string_view device_id) override;
  bool SupportsPIN() const override;
  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override;
  void StartBioEnrollment(base::OnceClosure next_callback) override;
  void OnSampleCollected(int bio_samples_remaining) override;
  void FinishCollectToken() override;
  void OnRetryUserVerification(int attempts) override;

 private:
  bool virtual_environment_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

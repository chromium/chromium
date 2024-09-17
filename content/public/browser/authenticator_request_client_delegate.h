// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif

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

class BrowserContext;
class RenderFrameHost;
class WebContents;

// WebAuthenticationDelegate is an interface that lets the //content layer
// provide embedder specific configuration for handling Web Authentication API
// (https://www.w3.org/TR/webauthn/) requests.
//
// Instances can be obtained via
// ContentBrowserClient::GetWebAuthenticationDelegate().
class CONTENT_EXPORT WebAuthenticationDelegate {
 public:
  WebAuthenticationDelegate();
  virtual ~WebAuthenticationDelegate();

  // Returns true if `caller_origin` should be able to claim the given Relying
  // Party ID outside of regular processing. Otherwise, standard WebAuthn RP ID
  // security checks are performed by `WebAuthRequestSecurityChecker`.
  // (https://www.w3.org/TR/2021/REC-webauthn-2-20210408/#relying-party-identifier).
  //
  // This is an access-control decision: RP IDs are used to control access to
  // credentials so thought is required before allowing an origin to assert an
  // RP ID.
  virtual bool OverrideCallerOriginAndRelyingPartyIdValidation(
      BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id);

  // Returns whether |caller_origin| is permitted to use the
  // RemoteDesktopClientOverride extension.
  //
  // This is an access control decision: RP IDs are used to control access to
  // credentials. If this method returns true, the respective origin is able to
  // claim any RP ID.
  virtual bool OriginMayUseRemoteDesktopClientOverride(
      BrowserContext* browser_context,
      const url::Origin& caller_origin);

  // Permits the embedder to override the Relying Party ID for a WebAuthn call,
  // given the claimed relying party ID and the origin of the caller.
  //
  // This is an access-control decision: RP IDs are used to control access to
  // credentials so thought is required before allowing an origin to assert an
  // RP ID. RP ID strings may be stored on authenticators and may later appear
  // in management UI.
  virtual std::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_relying_party_id,
      const url::Origin& caller_origin);

  // Returns true if the given relying party ID is permitted to receive
  // individual attestation certificates. This:
  //  a) triggers a signal to the security key that returning individual
  //     attestation certificates is permitted, and
  //  b) skips any permission prompt for attestation.
  virtual bool ShouldPermitIndividualAttestation(
      BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id);

  // SupportsResidentKeys returns true if this implementation of
  // |AuthenticatorRequestClientDelegate| supports resident keys for WebAuthn
  // requests originating from |render_frame_host|. If false then requests to
  // create or get assertions will be immediately rejected.
  virtual bool SupportsResidentKeys(RenderFrameHost* render_frame_host);

  // SupportsPasskeyMetadataSyncing returns true if the embedder supports
  // syncing passkey metadata from external authenticators.
  virtual bool SupportsPasskeyMetadataSyncing();

  // Returns whether |web_contents| is the active tab in the focused window. We
  // do not want to allow authenticatorMakeCredential operations to be triggered
  // by background tabs.
  //
  // Note that the default implementation of this function, and the
  // implementation in ChromeContentBrowserClient for Android, return |true| so
  // that testing is possible.
  virtual bool IsFocused(WebContents* web_contents);

  // Determines if the isUserVerifyingPlatformAuthenticator API call originating
  // from |render_frame_host| should be overridden with a value. The callback is
  // invoked with the override value, or with std::nullopt if it should not be
  // overridden. The callback can be invoked synchronously or asynchronously.
  virtual void IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      RenderFrameHost* render_frame_host,
      base::OnceCallback<void(std::optional<bool>)> callback);

  // Returns the active WebAuthenticationRequestProxy for WebAuthn requests
  // originating from `caller_origin` in `browser_context`.
  //
  // If this method returns a proxy, the caller is expected to hand off WebAuthn
  // request handling to this proxy instance.
  virtual WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      BrowserContext* browser_context,
      const url::Origin& caller_origin);

  // DeletePasskey removes a passkey from the credential storage provider using
  // the provided credential ID and relying party ID.
  virtual void DeletePasskey(content::WebContents* web_contents,
                             const std::vector<uint8_t>& passkey_credential_id,
                             const std::string& relying_party_id);

  // DeleteUnacceptedPasskeys removes any non-appearing credential in the
  // all_accepted_credentials_ids list from the credential storage provider for
  // the given relying party ID and user ID.
  virtual void DeleteUnacceptedPasskeys(
      content::WebContents* web_contents,
      const std::string& relying_party_id,
      const std::vector<uint8_t>& user_id,
      const std::vector<std::vector<uint8_t>>& all_accepted_credentials_ids);

  // UpdateUserPasskeys updates the name and display name of a passkey for the
  // given relying party ID and user ID.
  virtual void UpdateUserPasskeys(content::WebContents* web_contents,
                                  const url::Origin& origin,
                                  const std::string& relying_party_id,
                                  std::vector<uint8_t>& user_id,
                                  const std::string& name,
                                  const std::string& display_name);

  // Invokes the callback with true when passkeys provided by browser sync
  // are available for use, and false otherwise. The callback can be invoked
  // synchronously or asynchronously.
  virtual void BrowserProvidedPasskeysAvailable(
      BrowserContext* browser_context,
      base::OnceCallback<void(bool)> callback);

#if BUILDFLAG(IS_MAC)
  using TouchIdAuthenticatorConfig = device::fido::mac::AuthenticatorConfig;

  // Returns configuration data for the built-in Touch ID platform
  // authenticator. May return nullopt if the authenticator is not available in
  // the current context, in which case the Touch ID authenticator will be
  // unavailable.
  virtual std::optional<TouchIdAuthenticatorConfig>
  GetTouchIdAuthenticatorConfig(BrowserContext* browser_context);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // Callback that should generate and return a unique request id.
  using ChromeOSGenerateRequestIdCallback =
      base::RepeatingCallback<std::string()>;

  // Returns a callback to generate a request id for a WebAuthn request
  // originating from |RenderFrameHost|. The request id has two purposes: 1.
  // ChromeOS UI will use the request id to find the source window and show a
  // dialog accordingly; 2. The authenticator will include the request id when
  // asking ChromeOS platform to cancel the request.
  virtual ChromeOSGenerateRequestIdCallback GetGenerateRequestIdCallback(
      RenderFrameHost* render_frame_host);
#endif  // BUILDFLAG(IS_CHROMEOS)
};

// AuthenticatorRequestClientDelegate is an interface that lets embedders
// customize the lifetime of a single WebAuthn API request in the //content
// layer. In particular, the Authenticator mojo service uses
// AuthenticatorRequestClientDelegate to show WebAuthn request UI.
class CONTENT_EXPORT AuthenticatorRequestClientDelegate
    : public device::FidoRequestHandlerBase::Observer {
 public:
  using AccountPreselectedCallback =
      base::RepeatingCallback<void(device::DiscoverableCredentialMetadata)>;

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
  // actions, namely: cancel the request, start the request over, preselect an
  // account, dispatch request to connected authenticators, power on the
  // bluetooth adapter, and request permission to use the bluetooth adapter.
  virtual void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
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

  // Disables the WebAuthn request modal dialog UI.
  virtual void DisableUI();

  virtual bool IsWebAuthnUIEnabled();

  // Configures whether a virtual authenticator environment is enabled. The
  // embedder might choose to e.g. automate account selection under a virtual
  // environment.
  void SetVirtualEnvironment(bool virtual_environment);

  bool IsVirtualEnvironmentEnabled();

  // Set to true to enable a mode where a priori discovered credentials are
  // shown alongside autofilled passwords, instead of the modal flow.
  virtual void SetConditionalRequest(bool is_conditional);

  // Set the credential types that are expected by the Ambient UI.
  // Credential types are defined in `credential_types.mojom`.
  virtual void SetAmbientCredentialTypes(int credential_type_flags);

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

  // device::FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo data) override;
  // If true, the request handler will defer dispatch of its request onto the
  // given authenticator to the embedder. The embedder needs to call
  // |StartAuthenticatorRequest| when it wants to initiate request dispatch.
  //
  // This method is invoked before |FidoAuthenticatorAdded|, and may be
  // invoked multiple times for the same authenticator. Depending on the
  // result, the request handler might decide not to make the authenticator
  // available, in which case it never gets passed to
  // |FidoAuthenticatorAdded|.
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

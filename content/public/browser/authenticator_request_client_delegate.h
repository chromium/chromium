// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

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

  // Returns true if the tab security level is acceptable to allow WebAuthn
  // requests, false otherwise.
  virtual bool IsSecurityLevelAcceptableForWebAuthn(
      content::RenderFrameHost* rfh,
      const url::Origin& caller_origin);

#if !BUILDFLAG(IS_ANDROID)
  // Permits the embedder to override the Relying Party ID for a WebAuthn call,
  // given the claimed relying party ID and the origin of the caller.
  //
  // This is an access-control decision: RP IDs are used to control access to
  // credentials so thought is required before allowing an origin to assert an
  // RP ID. RP ID strings may be stored on authenticators and may later appear
  // in management UI.
  virtual absl::optional<std::string> MaybeGetRelyingPartyIdOverride(
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

  // Returns whether |web_contents| is the active tab in the focused window. We
  // do not want to allow authenticatorMakeCredential operations to be triggered
  // by background tabs.
  //
  // Note that the default implementation of this function, and the
  // implementation in ChromeContentBrowserClient for Android, return |true| so
  // that testing is possible.
  virtual bool IsFocused(WebContents* web_contents);

  // Returns a bool if the result of the isUserVerifyingPlatformAuthenticator
  // API call originating from |render_frame_host| should be overridden with
  // that value, or absl::nullopt otherwise.
  virtual absl::optional<bool>
  IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      RenderFrameHost* render_frame_host);

  // Returns the WebAuthenticationRequestProxy for the |browser_context|, if
  // any.
  virtual WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      BrowserContext* browser_context);
#endif  // !IS_ANDROID

#if BUILDFLAG(IS_WIN)
  // OperationSucceeded is called when a registration or assertion operation
  // succeeded. It communicates whether the Windows API was used or not. The
  // implementation may wish to use this information to guide the UI for future
  // operations towards the types of security keys that the user tends to use.
  virtual void OperationSucceeded(BrowserContext* browser_context,
                                  bool used_win_api);
#endif

#if BUILDFLAG(IS_MAC)
  using TouchIdAuthenticatorConfig = device::fido::mac::AuthenticatorConfig;

  // Returns configuration data for the built-in Touch ID platform
  // authenticator. May return nullopt if the authenticator is not available in
  // the current context, in which case the Touch ID authenticator will be
  // unavailable.
  virtual absl::optional<TouchIdAuthenticatorConfig>
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

#if BUILDFLAG(IS_ANDROID)
  // GetIntentSender returns a Java object that implements
  // `WebAuthenticationDelegate.IntentSender` from
  // WebAuthenticationDelegate.java. See the comments in that file for details.
  virtual base::android::ScopedJavaLocalRef<jobject> GetIntentSender(
      WebContents* web_contents);

  // GetSupportLevel returns one of:
  //   0 -> No WebAuthn support for this `WebContents`.
  //   1 -> WebAuthn should be implemented like an app.
  //   2 -> WebAuthn should be implemented like a browser.
  //
  // The difference between app and browser is meaningful on Android because
  // there is a different, privileged interface for browsers.
  //
  // The return value is an `int` rather than an enum because it's bounced
  // access JNI boundaries multiple times and so it's only converted to an
  // enum at the very end.
  virtual int GetSupportLevel(WebContents* web_contents);
#endif
};

// AuthenticatorRequestClientDelegate is an interface that lets embedders
// customize the lifetime of a single WebAuthn API request in the //content
// layer. In particular, the Authenticator mojo service uses
// AuthenticatorRequestClientDelegate to show WebAuthn request UI.
class CONTENT_EXPORT AuthenticatorRequestClientDelegate
    : public device::FidoRequestHandlerBase::Observer {
 public:
  using AccountPreselectedCallback =
      base::RepeatingCallback<void(std::vector<uint8_t> credential_id)>;

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

  // Supplies callbacks that the embedder can invoke to initiate certain
  // actions, namely: cancel the request, start the request over, preselect an
  // account, dispatch request to connected authenticators, and power on the
  // bluetooth adapter.
  virtual void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback);

  // Invokes |callback| with |true| if the given relying party ID is permitted
  // to receive attestation certificates from the provided FidoAuthenticator.
  // Otherwise invokes |callback| with |false|.
  //
  // If |is_enterprise_attestation| is true then that authenticator has asserted
  // that |relying_party_id| is known to it and the attesation has no
  // expectations of privacy.
  //
  // Since these certificates may uniquely identify the authenticator, the
  // embedder may choose to show a permissions prompt to the user, and only
  // invoke |callback| afterwards. This may hairpin |callback|.
  virtual void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback);

  // ConfigureCable optionally configures Cloud-assisted Bluetooth Low Energy
  // transports. |origin| is the origin of the calling site and
  // |pairings_from_extension| are caBLEv1 pairings that have been provided in
  // an extension to the WebAuthn get() call. |resident_key_requirement| is only
  // set when provided (i.e. for makeCredential calls) and reflects the value
  // requested by the site. If the embedder wishes, it may use this to configure
  // caBLE on the |FidoDiscoveryFactory| for use in this request.
  virtual void ConfigureCable(
      const url::Origin& origin,
      device::CableRequestType request_type,
      absl::optional<device::ResidentKeyRequirement> resident_key_requirement,
      base::span<const device::CableDiscoveryData> pairings_from_extension,
      device::FidoDiscoveryFactory* fido_discovery_factory);

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

  // Sets a credential filter for conditional mediation requests, which will
  // only allow passkeys with matching credential IDs to be displayed to the
  // user.
  virtual void SetCredentialIdFilter(
      std::vector<device::PublicKeyCredentialDescriptor> credential_list);

  // Optionally configures the user entity passed for a makeCredential request.
  virtual void SetUserEntityForMakeCredentialRequest(
      const device::PublicKeyCredentialUserEntity& user_entity);

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
  void BluetoothAdapterPowerChanged(bool is_powered_on) override;
  void FidoAuthenticatorAdded(
      const device::FidoAuthenticator& authenticator) override;
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override;
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

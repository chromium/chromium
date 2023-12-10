// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/authenticator_common.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/make_credential_request_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace device {

class FidoDiscoveryFactory;

enum class FidoReturnCode : uint8_t;

enum class GetAssertionStatus;
enum class MakeCredentialStatus;

}  // namespace device

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class RenderFrameHost;
class WebAuthRequestSecurityChecker;

enum class RequestExtension;
enum class AttestationErasureOption;

// Common code for any WebAuthn Authenticator interfaces.
class CONTENT_EXPORT AuthenticatorCommonImpl : public AuthenticatorCommon {
 public:
  // ServingRequestsFor enumerates the sources of WebAuthn requests.
  enum class ServingRequestsFor {
    // kInternalUses is for synthesized requests that don't originate from
    // any Javascript call.
    kInternalUses,
    // kWebContents is for typical cases where Javascript is making a
    // `navigator.credentials` call.
    kWebContents,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class GetAssertionResult {
    kTimeout = 0,
    kUserCancelled = 1,

    kWinNativeSuccess = 2,
    kWinNativeError = 3,

    kTouchIDSuccess = 4,
    kTouchIDError = 5,

    kChromeOSSuccess = 6,
    kChromeOSError = 7,

    kPhoneSuccess = 8,
    kPhoneError = 9,

    kICloudKeychainSuccess = 10,
    kICloudKeychainError = 11,

    kEnclaveSuccess = 12,
    kEnclaveError = 13,

    kOtherSuccess = 14,
    kOtherError = 15,

    kMaxValue = kOtherError,
  };

  // Creates a new AuthenticatorCommonImpl. Callers must ensure that this
  // instance outlives the RenderFrameHost.
  explicit AuthenticatorCommonImpl(RenderFrameHost* render_frame_host,
                                   ServingRequestsFor serving_requests_for);

  AuthenticatorCommonImpl(const AuthenticatorCommonImpl&) = delete;
  AuthenticatorCommonImpl& operator=(const AuthenticatorCommonImpl&) = delete;

  ~AuthenticatorCommonImpl() override;

  // AuthenticatorCommon:
  void MakeCredential(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback) override;
  void GetAssertion(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::PaymentOptionsPtr payment,
      blink::mojom::Authenticator::GetAssertionCallback callback) override;
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      url::Origin caller_origin,
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback)
      override;
  void IsConditionalMediationAvailable(
      url::Origin caller_origin,
      blink::mojom::Authenticator::IsConditionalMediationAvailableCallback
          callback) override;
  void Cancel() override;
  void Cleanup() override;
  void DisableUI() override;
  void DisableTLSCheck() override;
  RenderFrameHost* GetRenderFrameHost() const override;
  void EnableRequestProxyExtensionsAPISupport() override;

 protected:
  // MaybeCreateRequestDelegate returns the embedder-provided implementation of
  // AuthenticatorRequestClientDelegate, which encapsulates per-request state
  // relevant to the embedder, e.g. because it is used to display browser UI.
  //
  // Chrome may return nullptr here in order to ensure that at most one request
  // per WebContents is ongoing at once.
  virtual std::unique_ptr<AuthenticatorRequestClientDelegate>
  MaybeCreateRequestDelegate();

 private:
  friend class AuthenticatorImplTest;
  struct RequestState;

  // Enumerates whether or not to check that the WebContents has focus.
  enum class Focus {
    kDoCheck,
    kDontCheck,
  };

  void ContinueMakeCredentialAfterRpIdCheck(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      bool is_cross_origin_iframe,
      blink::mojom::AuthenticatorStatus rp_id_validation_result);

  void ContinueGetAssertionAfterRpIdCheck(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
      blink::mojom::PaymentOptionsPtr payment_options,
      bool is_cross_origin_iframe,
      blink::mojom::AuthenticatorStatus rp_id_validation_result);

  // Replaces the current |request_handler_| with a
  // |MakeCredentialRequestHandler|, effectively restarting the request.
  void StartMakeCredentialRequest(bool allow_skipping_pin_touch);

  // Replaces the current |request_handler_| with a
  // |GetAssertionRequestHandler|, effectively restarting the request.
  void StartGetAssertionRequest(bool allow_skipping_pin_touch);

  bool IsFocused() const;

  void DispatchGetAssertionRequest(
      const std::string& authenticator_id,
      absl::optional<std::vector<uint8_t>> credential_id);

  // Callback to handle the async response from a U2fDevice.
  void OnRegisterResponse(
      device::MakeCredentialStatus status_code,
      absl::optional<device::AuthenticatorMakeCredentialResponse> response_data,
      const device::FidoAuthenticator* authenticator);

  // Callback to complete the registration process once a decision about
  // whether or not to return attestation data has been made.
  void OnRegisterResponseAttestationDecided(
      AttestationErasureOption attestation_erasure,
      device::AuthenticatorMakeCredentialResponse response_data,
      bool attestation_permitted);

  // Callback to handle the async response from a U2fDevice.
  void OnSignResponse(
      device::GetAssertionStatus status_code,
      absl::optional<std::vector<device::AuthenticatorGetAssertionResponse>>
          response_data,
      device::FidoAuthenticator* authenticator);

  // Begins a timeout at the beginning of a request.
  void BeginRequestTimeout(absl::optional<base::TimeDelta> timeout);

  // Runs when timer expires and cancels all issued requests to a U2fDevice.
  void OnTimeout();

  // Cancels the currently pending request (if any) with the supplied status.
  void CancelWithStatus(blink::mojom::AuthenticatorStatus status);

  // Runs when the user cancels WebAuthN request via UI dialog.
  void OnCancelFromUI();

  // Called when a GetAssertion has completed, either because an allow_list was
  // used and so an answer is returned directly, or because the user selected an
  // account from the options.
  void OnAccountSelected(device::AuthenticatorGetAssertionResponse response);

  // Signals to the request delegate that the request has failed for |reason|.
  // The request delegate decides whether to present the user with a visual
  // error before the request is finally resolved with |status|.
  void SignalFailureToRequestDelegate(
      AuthenticatorRequestClientDelegate::InterestingFailureReason reason,
      blink::mojom::AuthenticatorStatus status);

  // Creates a make credential response
  blink::mojom::MakeCredentialAuthenticatorResponsePtr
  CreateMakeCredentialResponse(
      device::AuthenticatorMakeCredentialResponse response_data,
      AttestationErasureOption attestation_erasure);

  // Runs |make_credential_response_callback_| and then Cleanup().
  void CompleteMakeCredentialRequest(
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::MakeCredentialAuthenticatorResponsePtr response = nullptr,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details =
          nullptr,
      Focus focus_check = Focus::kDontCheck);

  // Creates a get assertion response.
  blink::mojom::GetAssertionAuthenticatorResponsePtr CreateGetAssertionResponse(
      device::AuthenticatorGetAssertionResponse response_data);

  // Runs |get_assertion_response_callback_| and then Cleanup().
  void CompleteGetAssertionRequest(
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response = nullptr,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details =
          nullptr);

  AuthenticatorRequestClientDelegate::RequestSource RequestSource() const;
  BrowserContext* GetBrowserContext() const;

  // Returns the FidoDiscoveryFactory for the current request. This may be a
  // real instance, or one injected by the Virtual Authenticator environment, or
  // a unit testing fake. InitDiscoveryFactory() must be called before this
  // accessor. It gets reset at the end of each request by Cleanup().
  device::FidoDiscoveryFactory* discovery_factory();
  void InitDiscoveryFactory();

  WebAuthenticationRequestProxy* GetWebAuthnRequestProxyIfActive(
      const url::Origin& caller_origin);

  void OnMakeCredentialProxyResponse(
      WebAuthenticationRequestProxy::RequestId request_id,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr error,
      blink::mojom::MakeCredentialAuthenticatorResponsePtr response);

  void OnGetAssertionProxyResponse(
      WebAuthenticationRequestProxy::RequestId request_id,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr error,
      blink::mojom::GetAssertionAuthenticatorResponsePtr response);

  const GlobalRenderFrameHostId render_frame_host_id_;
  const ServingRequestsFor serving_requests_for_;
  const scoped_refptr<WebAuthRequestSecurityChecker> security_checker_;

  // These members hold state that spans different requests. All
  // request-specific state should go in `RequestState` to ensure that it's
  // cleared between requests.
  bool disable_tls_check_ = false;
  bool disable_ui_ = false;
  bool enable_request_proxy_api_ = false;

  // req_state_ contains all state specific to a single WebAuthn call. It
  // only contains a value when a request is being processed.
  std::unique_ptr<RequestState> req_state_;

  base::WeakPtrFactory<AuthenticatorCommonImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_IMPL_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_IMPL_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/authenticator_common.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/make_credential_request_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

namespace base {
class OneShotTimer;
}

namespace device {

class FidoRequestHandlerBase;
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
  // Creates a new AuthenticatorCommonImpl. Callers must ensure that this
  // instance outlives the RenderFrameHost.
  explicit AuthenticatorCommonImpl(RenderFrameHost* render_frame_host);

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

  std::unique_ptr<AuthenticatorRequestClientDelegate> request_delegate_;

 private:
  friend class AuthenticatorImplTest;

  // Enumerates whether or not to check that the WebContents has focus.
  enum class Focus {
    kDoCheck,
    kDontCheck,
  };

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
      const bool has_device_public_key_output,
      const bool device_public_key_included_attestation,
      device::AuthenticatorMakeCredentialResponse response_data,
      bool attestation_permitted);

  // Callback to handle the async response from a U2fDevice.
  void OnSignResponse(
      device::GetAssertionStatus status_code,
      absl::optional<std::vector<device::AuthenticatorGetAssertionResponse>>
          response_data);

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
  bool has_pending_request_ = false;
  std::unique_ptr<device::FidoRequestHandlerBase> request_handler_;
  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory_;
  // This dangling raw_ptr occurred in:
  // interactive_ui_tests:
  // WebAuthnDevtoolsAutofillIntegrationTest.SelectAccountWithAllowCredentials
  // https://ci.chromium.org/ui/p/chromium/builders/try/mac-rel/1357012/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Ainteractive_ui_tests%2FWebAuthnDevtoolsAutofillIntegrationTest.SelectAccountWithAllowCredentials+VHash%3A81d118f1ad0b63a6
  raw_ptr<device::FidoDiscoveryFactory, FlakyDanglingUntriaged>
      discovery_factory_testing_override_ = nullptr;
  blink::mojom::Authenticator::MakeCredentialCallback
      make_credential_response_callback_;
  blink::mojom::Authenticator::GetAssertionCallback
      get_assertion_response_callback_;
  std::string client_data_json_;
  bool disable_ui_ = false;
  bool disable_tls_check_ = false;
  url::Origin caller_origin_;
  std::string relying_party_id_;
  scoped_refptr<WebAuthRequestSecurityChecker> security_checker_;
  std::unique_ptr<base::OneShotTimer> timer_ =
      std::make_unique<base::OneShotTimer>();
  absl::optional<std::string> app_id_;
  absl::optional<device::CtapMakeCredentialRequest>
      ctap_make_credential_request_;
  absl::optional<device::MakeCredentialOptions> make_credential_options_;
  absl::optional<device::CtapGetAssertionRequest> ctap_get_assertion_request_;
  absl::optional<device::CtapGetAssertionOptions> ctap_get_assertion_options_;
  // device_public_key_attestation_requested_ is true if any form of DPK
  // attestation was requested, even if it was mapped to "none" in
  // |ctap_*_request_|.
  bool device_public_key_attestation_requested_ = false;
  // awaiting_attestation_response_ is true if the embedder has been queried
  // about an attestsation decision and the response is still pending.
  bool awaiting_attestation_response_ = false;
  blink::mojom::AuthenticatorStatus error_awaiting_user_acknowledgement_ =
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
  bool enable_request_proxy_api_ = false;
  bool discoverable_credential_request_ = false;

  base::flat_set<RequestExtension> requested_extensions_;

  // The request ID of a pending proxied MakeCredential or GetAssertion request.
  absl::optional<WebAuthenticationRequestProxy::RequestId>
      pending_proxied_request_id_;

  base::WeakPtrFactory<AuthenticatorCommonImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_IMPL_H_

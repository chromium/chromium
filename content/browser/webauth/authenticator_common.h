// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/make_credential_request_handler.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
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

// Common code for any WebAuthn Authenticator interfaces.
class CONTENT_EXPORT AuthenticatorCommon {
 public:
  // Creates a new AuthenticatorCommon. Callers must ensure that this instance
  // outlives the RenderFrameHost.
  explicit AuthenticatorCommon(RenderFrameHost* render_frame_host);

  AuthenticatorCommon(const AuthenticatorCommon&) = delete;
  AuthenticatorCommon& operator=(const AuthenticatorCommon&) = delete;

  virtual ~AuthenticatorCommon();

  // This is not-quite an implementation of blink::mojom::Authenticator. The
  // first two functions take the caller's origin explicitly. This allows the
  // caller origin to be overridden if needed. `GetAssertion()` also takes the
  // optional `payment` to add to "clientDataJson" after the browser displays
  // the payment confirmation dialog to the user.
  void MakeCredential(
      url::Origin caller_origin,
      blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
      blink::mojom::Authenticator::MakeCredentialCallback callback);
  void GetAssertion(url::Origin caller_origin,
                    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
                    blink::mojom::PaymentOptionsPtr payment,
                    blink::mojom::Authenticator::GetAssertionCallback callback);
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      blink::mojom::Authenticator::
          IsUserVerifyingPlatformAuthenticatorAvailableCallback callback);
  void IsConditionalMediationAvailable(
      blink::mojom::Authenticator::IsConditionalMediationAvailableCallback
          callback);
  void Cancel();

  void Cleanup();

  void DisableUI();

  // GetRenderFrameHost returns a pointer to the RenderFrameHost that was given
  // to the constructor. Use this rather than keeping a copy of the
  // RenderFrameHost* that was passed in.
  //
  // This object assumes that the RenderFrameHost overlives it but, in case it
  // doesn't, this avoids holding a raw pointer and creating a use-after-free.
  // If the RenderFrameHost has been destroyed then this function will return
  // nullptr and the process will crash when it tries to use it.
  RenderFrameHost* GetRenderFrameHost() const;

  // Enables support for the webAuthenticationRequestProxy extensions API.  If
  // called, remote desktop Chrome extensions may choose to act as a request
  // proxy for all requests sent to this instance.
  void EnableRequestProxyExtensionsAPISupport();

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

  enum class AttestationErasureOption {
    kIncludeAttestation,
    kEraseAttestationButIncludeAaguid,
    kEraseAttestationAndAaguid,
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

  // Callback to handle the large blob being compressed before attempting to
  // start a request.
  void OnLargeBlobCompressed(
      uint64_t original_size,
      base::expected<mojo_base::BigBuffer, std::string> result);

  // Callback to handle the large blob being uncompressed before completing a
  // request.
  void OnLargeBlobUncompressed(
      device::AuthenticatorGetAssertionResponse response,
      base::expected<mojo_base::BigBuffer, std::string> result);

  // Callback to handle the async response from a U2fDevice.
  void OnRegisterResponse(
      device::MakeCredentialStatus status_code,
      absl::optional<device::AuthenticatorMakeCredentialResponse> response_data,
      const device::FidoAuthenticator* authenticator);

  // Callback to complete the registration process once a decision about
  // whether or not to return attestation data has been made.
  void OnRegisterResponseAttestationDecided(
      device::AuthenticatorMakeCredentialResponse response_data,
      bool attestation_permitted);

  // Callback to handle the async response from a U2fDevice.
  void OnSignResponse(
      device::GetAssertionStatus status_code,
      absl::optional<std::vector<device::AuthenticatorGetAssertionResponse>>
          response_data,
      const device::FidoAuthenticator* authenticator);

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
      const device::FidoAuthenticator* authenticator,
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
      device::AuthenticatorGetAssertionResponse response_data,
      absl::optional<std::vector<uint8_t>> large_blob = absl::nullopt);

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
  void InitDiscoveryFactory(bool is_u2f_api_request);

  WebAuthenticationRequestProxy* GetWebAuthnRequestProxyIfActive();

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
  raw_ptr<device::FidoDiscoveryFactory> discovery_factory_testing_override_ =
      nullptr;
  blink::mojom::Authenticator::MakeCredentialCallback
      make_credential_response_callback_;
  blink::mojom::Authenticator::GetAssertionCallback
      get_assertion_response_callback_;
  std::string client_data_json_;
  bool disable_ui_ = false;
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
  // awaiting_attestation_response_ is true if the embedder has been queried
  // about an attestsation decision and the response is still pending.
  bool awaiting_attestation_response_ = false;
  blink::mojom::AuthenticatorStatus error_awaiting_user_acknowledgement_ =
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
  data_decoder::DataDecoder data_decoder_;
  bool enable_request_proxy_api_ = false;
  bool discoverable_credential_request_ = false;

  base::flat_set<RequestExtension> requested_extensions_;

  // The request ID of a pending proxied MakeCredential or GetAssertion request.
  absl::optional<WebAuthenticationRequestProxy::RequestId>
      pending_proxied_request_id_;

  base::WeakPtrFactory<AuthenticatorCommon> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_COMMON_H_

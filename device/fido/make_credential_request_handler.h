// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/fido/auth_token_requester.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/bio/enroller.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"

namespace base {
class ElapsedTimer;
}

namespace device {

class FidoDiscoveryFactory;

namespace pin {
class TokenResponse;
}  // namespace pin

class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialRequestHandler
    : public FidoRequestHandlerBase,
      public AuthTokenRequester::Delegate,
      public BioEnroller::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(
      MakeCredentialStatus,
      std::optional<AuthenticatorMakeCredentialResponse>,
      const FidoAuthenticator*)>;

  MakeCredentialRequestHandler(
      FidoDiscoveryFactory* fido_discovery_factory,
      std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapMakeCredentialRequest request_parameter,
      const MakeCredentialOptions& options,
      CompletionCallback completion_callback);

  MakeCredentialRequestHandler(const MakeCredentialRequestHandler&) = delete;
  MakeCredentialRequestHandler& operator=(const MakeCredentialRequestHandler&) =
      delete;

  ~MakeCredentialRequestHandler() override;

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForToken,
    kBioEnrollment,
    kBioEnrollmentDone,
    kWaitingForResponseWithToken,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  // AuthTokenRequester::Delegate:
  bool AuthenticatorSelectedForPINUVAuthToken(
      FidoAuthenticator* authenticator) override;
  void CollectPIN(pin::PINEntryReason reason,
                  pin::PINEntryError error,
                  uint32_t min_pin_length,
                  int attempts,
                  ProvidePINCallback provide_pin_cb) override;
  void PromptForInternalUVRetry(int attempts) override;
  void HavePINUVAuthTokenResultForAuthenticator(
      FidoAuthenticator* authenticator,
      AuthTokenRequester::Result result,
      std::optional<pin::TokenResponse> response) override;

  // BioEnroller::Delegate:
  void OnSampleCollected(BioEnrollmentSampleStatus status,
                         int samples_remaining) override;
  void OnEnrollmentDone(
      std::optional<std::vector<uint8_t>> template_id) override;
  void OnEnrollmentError(CtapDeviceResponseCode status) override;

  void ObtainPINUVAuthToken(FidoAuthenticator* authenticator,
                            bool skip_pin_touch,
                            bool internal_uv_locked);

  void DispatchRequestAfterAppIdExclude(
      std::unique_ptr<CtapMakeCredentialRequest> request,
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode status,
      std::optional<bool> unused);
  void HandleResponse(
      FidoAuthenticator* authenticator,
      std::unique_ptr<CtapMakeCredentialRequest> request,
      base::ElapsedTimer request_timer,
      MakeCredentialStatus response_code,
      std::optional<AuthenticatorMakeCredentialResponse> response);
  void HandleExcludedAuthenticator(FidoAuthenticator* authenticator);
  void HandleInapplicableAuthenticator(FidoAuthenticator* authenticator,
                                       MakeCredentialStatus status);
  void OnEnrollmentComplete(std::unique_ptr<CtapMakeCredentialRequest> request);
  void OnEnrollmentDismissed();
  void DispatchRequestWithToken(
      FidoAuthenticator* authenticator,
      std::unique_ptr<CtapMakeCredentialRequest> request,
      pin::TokenResponse token);

  void SpecializeRequestForAuthenticator(
      CtapMakeCredentialRequest* request,
      const FidoAuthenticator* authenticator);

  CompletionCallback completion_callback_;
  State state_ = State::kWaitingForTouch;
  bool suppress_attestation_ = false;
  CtapMakeCredentialRequest request_;
  std::optional<base::RepeatingClosure> bio_enrollment_complete_barrier_;
  const MakeCredentialOptions options_;

  std::map<FidoAuthenticator*, std::unique_ptr<AuthTokenRequester>>
      auth_token_requester_map_;

  // selected_authenticator_for_pin_uv_auth_token_ points to the authenticator
  // that was tapped by the user while requesting a pinUvAuthToken from
  // connected authenticators. The object is owned by the underlying discovery
  // object and this pointer is cleared if it's removed during processing.
  raw_ptr<FidoAuthenticator> selected_authenticator_for_pin_uv_auth_token_ =
      nullptr;
  std::optional<pin::TokenResponse> token_;
  std::unique_ptr<BioEnroller> bio_enroller_;

  // On ChromeOS, non-U2F cross-platform requests may be dispatched to the
  // platform authenticator if the request is user-presence-only and the
  // authenticator has been configured for user-presence-only mode via an
  // enterprise policy. For such requests, this field will be true.
  bool allow_platform_authenticator_for_cross_platform_request_ = false;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<MakeCredentialRequestHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

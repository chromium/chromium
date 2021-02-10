// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "device/fido/auth_token_requester.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/bio/enroller.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/pin.h"

namespace base {
class ElapsedTimer;
}

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

namespace pin {
class TokenResponse;
}  // namespace pin

enum class MakeCredentialStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kUserConsentButCredentialExcluded,
  kUserConsentDenied,
  kAuthenticatorRemovedDuringPINEntry,
  kSoftPINBlock,
  kHardPINBlock,
  kAuthenticatorMissingResidentKeys,
  // TODO(agl): kAuthenticatorMissingUserVerification can
  // also be returned when the authenticator supports UV, but
  // there's no UI support for collecting a PIN. This could
  // be clearer.
  kAuthenticatorMissingUserVerification,
  kAuthenticatorMissingLargeBlob,
  kNoCommonAlgorithms,
  kStorageFull,
  kWinInvalidStateError,
  kWinNotAllowedError,
};

class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialRequestHandler
    : public FidoRequestHandlerBase,
      public AuthTokenRequester::Delegate,
      public BioEnroller::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(
      MakeCredentialStatus,
      base::Optional<AuthenticatorMakeCredentialResponse>,
      const FidoAuthenticator*)>;

  // Options contains higher-level request parameters that aren't part of the
  // makeCredential request itself, or that need to be combined with knowledge
  // of the specific authenticator, thus don't live in
  // |CtapMakeCredentialRequest|.
  struct COMPONENT_EXPORT(DEVICE_FIDO) Options {
    Options();
    explicit Options(
        const AuthenticatorSelectionCriteria& authenticator_selection_criteria);
    ~Options();
    Options(const Options&);
    Options(Options&&);
    Options& operator=(const Options&);
    Options& operator=(Options&&);

    // authenticator_attachment is a constraint on the type of authenticator
    // that a credential should be created on.
    AuthenticatorAttachment authenticator_attachment =
        AuthenticatorAttachment::kAny;

    // resident_key indicates whether the request should result in the creation
    // of a client-side discoverable credential (aka resident key).
    ResidentKeyRequirement resident_key = ResidentKeyRequirement::kDiscouraged;

    // user_verification indicates whether the authenticator should (or must)
    // perform user verficiation before creating the credential.
    UserVerificationRequirement user_verification =
        UserVerificationRequirement::kPreferred;

    // cred_protect_request extends |CredProtect| to include information that
    // applies at request-routing time. The second element is true if the
    // indicated protection level must be provided by the target authenticator
    // for the MakeCredential request to be sent.
    base::Optional<std::pair<CredProtectRequest, bool>> cred_protect_request;

    // allow_skipping_pin_touch causes the handler to forego the first
    // "touch-only" step to collect a PIN if exactly one authenticator is
    // discovered.
    bool allow_skipping_pin_touch = false;

    // large_blob_support indicates whether the request should select for
    // authenticators supporting the largeBlobs extension (kRequired), merely
    // indicate support on the response (kPreferred), or ignore it
    // (kNotRequested).
    // Values other than kNotRequested will attempt to initialize the large blob
    // on the authenticator.
    LargeBlobSupport large_blob_support = LargeBlobSupport::kNotRequested;
  };

  MakeCredentialRequestHandler(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapMakeCredentialRequest request_parameter,
      const Options& options,
      CompletionCallback completion_callback);
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
  void AuthenticatorSelectedForPINUVAuthToken(
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
      base::Optional<pin::TokenResponse> response) override;

  // BioEnroller::Delegate:
  void OnSampleCollected(BioEnrollmentSampleStatus status,
                         int samples_remaining) override;
  void OnEnrollmentDone(
      base::Optional<std::vector<uint8_t>> template_id) override;
  void OnEnrollmentError(CtapDeviceResponseCode status) override;

  void ObtainPINUVAuthToken(FidoAuthenticator* authenticator,
                            bool skip_pin_touch,
                            bool internal_uv_locked);

  void HandleResponse(
      FidoAuthenticator* authenticator,
      std::unique_ptr<CtapMakeCredentialRequest> request,
      base::ElapsedTimer request_timer,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response);
  void HandleInapplicableAuthenticator(
      FidoAuthenticator* authenticator,
      std::unique_ptr<CtapMakeCredentialRequest> request);
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
  base::Optional<base::RepeatingClosure> bio_enrollment_complete_barrier_;
  const Options options_;

  std::map<FidoAuthenticator*, std::unique_ptr<AuthTokenRequester>>
      auth_token_requester_map_;

  // selected_authenticator_for_pin_uv_auth_token_ points to the authenticator
  // that was tapped by the user while requesting a pinUvAuthToken from
  // connected authenticators. The object is owned by the underlying discovery
  // object and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* selected_authenticator_for_pin_uv_auth_token_ = nullptr;
  base::Optional<pin::TokenResponse> token_;
  std::unique_ptr<BioEnroller> bio_enroller_;

  // On ChromeOS, non-U2F cross-platform requests may be dispatched to the
  // platform authenticator if the request is user-presence-only and the
  // authenticator has been configured for user-presence-only mode via an
  // enterprise policy. For such requests, this field will be true.
  bool allow_platform_authenticator_for_cross_platform_request_ = false;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<MakeCredentialRequestHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

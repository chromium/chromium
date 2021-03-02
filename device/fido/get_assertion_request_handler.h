// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_
#define DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/auth_token_requester.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace base {
class ElapsedTimer;
}

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

namespace pin {
class TokenResponse;
}  // namespace pin

enum class GetAssertionStatus {
  kSuccess,
  kAuthenticatorResponseInvalid,
  kUserConsentButCredentialNotRecognized,
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
  kWinNotAllowedError,
};

class COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionRequestHandler
    : public FidoRequestHandlerBase,
      public AuthTokenRequester::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(
      GetAssertionStatus,
      base::Optional<std::vector<AuthenticatorGetAssertionResponse>>,
      const FidoAuthenticator*)>;

  GetAssertionRequestHandler(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapGetAssertionRequest request_parameter,
      CtapGetAssertionOptions request_options,
      bool allow_skipping_pin_touch,
      CompletionCallback completion_callback);
  ~GetAssertionRequestHandler() override;

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForToken,
    kWaitingForResponseWithToken,
    kReadingMultipleResponses,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void OnBluetoothAdapterEnumerated(bool is_present,
                                    bool is_powered_on,
                                    bool can_power_on,
                                    bool is_peripheral_role_supported) override;
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;
  void FillHasRecognizedPlatformCredential(
      base::OnceCallback<void()> done_callback) override;

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

  void ObtainPINUVAuthToken(FidoAuthenticator* authenticator,
                            std::set<pin::Permissions> permissions,
                            bool skip_pin_touch,
                            bool internal_uv_locked);
  void HandleResponse(
      FidoAuthenticator* authenticator,
      CtapGetAssertionRequest request,
      base::ElapsedTimer request_timer,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response);
  void HandleNextResponse(
      FidoAuthenticator* authenticator,
      CtapGetAssertionRequest request,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response);
  void TerminateUnsatisfiableRequestPostTouch(FidoAuthenticator* authenticator);
  void DispatchRequestWithToken(pin::TokenResponse token);
  void OnGetAssertionSuccess(FidoAuthenticator* authenticator,
                             CtapGetAssertionRequest request);
  void OnReadLargeBlobs(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode status,
      base::Optional<std::vector<std::pair<LargeBlobKey, std::vector<uint8_t>>>>
          blobs);
  void OnWriteLargeBlob(FidoAuthenticator* authenticator,
                        CtapDeviceResponseCode status);
  void OnHasPlatformCredential(base::OnceCallback<void()> done_callback,
                               bool has_credential);

  CompletionCallback completion_callback_;
  State state_ = State::kWaitingForTouch;
  CtapGetAssertionRequest request_;
  CtapGetAssertionOptions options_;
  base::Optional<pin::TokenResponse> pin_token_;

  // If true, and if at the time the request is dispatched to the first
  // authenticator no other authenticators are available, the request handler
  // will skip the initial touch that is usually required to select a PIN
  // protected authenticator.
  bool allow_skipping_pin_touch_;

  // selected_authenticator_for_pin_uv_auth_token_ points to the authenticator
  // that was tapped by the user while requesting a pinUvAuthToken from
  // connected authenticators. The object is owned by the underlying discovery
  // object and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* selected_authenticator_for_pin_uv_auth_token_ = nullptr;

  // responses_ holds the set of responses while they are incrementally read
  // from the device. Only used when more than one response is returned.
  std::vector<AuthenticatorGetAssertionResponse> responses_;

  // remaining_responses_ contains the number of responses that remain to be
  // read when multiple responses are returned.
  size_t remaining_responses_ = 0;

  // auth_token_requester_map_ holds active AuthTokenRequesters for
  // authenticators that need a pinUvAuthToken to service the request.
  std::map<FidoAuthenticator*, std::unique_ptr<AuthTokenRequester>>
      auth_token_requester_map_;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<GetAssertionRequestHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetAssertionRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_
#define DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
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
struct RetriesResponse;
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
    : public FidoRequestHandlerBase {
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
    kWaitingForSecondTouch,
    kGettingRetries,
    kWaitingForPIN,
    kRequestWithPIN,
    kReadingMultipleResponses,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void OnBluetoothAdapterEnumerated(bool is_present,
                                    bool is_powered_on,
                                    bool can_power_on,
                                    bool is_peripheral_role_supported) override;
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void HandleResponse(
      FidoAuthenticator* authenticator,
      base::ElapsedTimer request_timer,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response);
  void HandleNextResponse(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorGetAssertionResponse> response);
  void CollectPINThenSendRequest(FidoAuthenticator* authenticator);
  void StartPINFallbackForInternalUv(FidoAuthenticator* authenticator);
  void TerminateUnsatisfiableRequestPostTouch(FidoAuthenticator* authenticator);
  void OnPinRetriesResponse(CtapDeviceResponseCode status,
                            base::Optional<pin::RetriesResponse> response);
  void OnHavePIN(std::string pin);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);
  void OnStartUvTokenOrFallback(FidoAuthenticator* authenticator,
                                CtapDeviceResponseCode status,
                                base::Optional<pin::RetriesResponse> response);
  void OnUvRetriesResponse(CtapDeviceResponseCode status,
                           base::Optional<pin::RetriesResponse> response);
  void OnHaveUvToken(FidoAuthenticator* authenticator,
                     CtapDeviceResponseCode status,
                     base::Optional<pin::TokenResponse> response);
  void DispatchRequestWithToken(pin::TokenResponse token);

  CompletionCallback completion_callback_;
  State state_ = State::kWaitingForTouch;
  CtapGetAssertionRequest request_;
  CtapGetAssertionOptions options_;
  base::Optional<AndroidClientDataExtensionInput> android_client_data_ext_;
  // If true, and if at the time the request is dispatched to the first
  // authenticator no other authenticators are available, the request handler
  // will skip the initial touch that is usually required to select a PIN
  // protected authenticator.
  bool allow_skipping_pin_touch_;
  // authenticator_ points to the authenticator that will be used for this
  // operation. It's only set after the user touches an authenticator to select
  // it, after which point that authenticator will be used exclusively through
  // requesting PIN etc. The object is owned by the underlying discovery object
  // and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* authenticator_ = nullptr;
  // responses_ holds the set of responses while they are incrementally read
  // from the device. Only used when more than one response is returned.
  std::vector<AuthenticatorGetAssertionResponse> responses_;
  // remaining_responses_ contains the number of responses that remain to be
  // read when multiple responses are returned.
  size_t remaining_responses_ = 0;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<GetAssertionRequestHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetAssertionRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

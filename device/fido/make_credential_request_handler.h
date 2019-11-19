// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_
#define DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace device {

class FidoAuthenticator;
class FidoDiscoveryFactory;

namespace pin {
struct EmptyResponse;
struct KeyAgreementResponse;
struct RetriesResponse;
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
  kStorageFull,
  kWinInvalidStateError,
  kWinNotAllowedError,
};

class COMPONENT_EXPORT(DEVICE_FIDO) MakeCredentialRequestHandler
    : public FidoRequestHandlerBase {
 public:
  using CompletionCallback = base::OnceCallback<void(
      MakeCredentialStatus,
      base::Optional<AuthenticatorMakeCredentialResponse>,
      const FidoAuthenticator*)>;

  MakeCredentialRequestHandler(
      service_manager::Connector* connector,
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapMakeCredentialRequest request_parameter,
      AuthenticatorSelectionCriteria authenticator_criteria,
      bool allow_skipping_pin_touch,
      CompletionCallback completion_callback);
  ~MakeCredentialRequestHandler() override;

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForSecondTouch,
    kGettingRetries,
    kWaitingForPIN,
    kWaitingForNewPIN,
    kGetEphemeralKey,
    kGetEphemeralKeyForNewPIN,
    kSettingPIN,
    kRequestWithPIN,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

  void HandleResponse(
      FidoAuthenticator* authenticator,
      CtapDeviceResponseCode response_code,
      base::Optional<AuthenticatorMakeCredentialResponse> response);
  void HandleTouch(FidoAuthenticator* authenticator);
  void HandleInapplicableAuthenticator(FidoAuthenticator* authenticator);
  void OnHavePIN(std::string pin);
  void OnRetriesResponse(CtapDeviceResponseCode status,
                         base::Optional<pin::RetriesResponse> response);
  void OnHaveEphemeralKey(std::string pin,
                          CtapDeviceResponseCode status,
                          base::Optional<pin::KeyAgreementResponse> response);
  void OnHaveSetPIN(std::string pin,
                    pin::KeyAgreementResponse key_agreement,
                    CtapDeviceResponseCode status,
                    base::Optional<pin::EmptyResponse> response);
  void OnHavePINToken(CtapDeviceResponseCode status,
                      base::Optional<pin::TokenResponse> response);

  CompletionCallback completion_callback_;
  State state_ = State::kWaitingForTouch;
  CtapMakeCredentialRequest request_;
  AuthenticatorSelectionCriteria authenticator_selection_criteria_;
  // If true, the request handler may skip the first touch to select a device
  // that will require a PIN.
  bool allow_skipping_pin_touch_;
  // authenticator_ points to the authenticator that will be used for this
  // operation. It's only set after the user touches an authenticator to select
  // it, after which point that authenticator will be used exclusively through
  // requesting PIN etc. The object is owned by the underlying discovery object
  // and this pointer is cleared if it's removed during processing.
  FidoAuthenticator* authenticator_ = nullptr;
  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<MakeCredentialRequestHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MakeCredentialRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_MAKE_CREDENTIAL_REQUEST_HANDLER_H_

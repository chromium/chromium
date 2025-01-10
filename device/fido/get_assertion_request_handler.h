// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_
#define DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/auth_token_requester.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_descriptor.h"

namespace base {
class ElapsedTimer;
}

namespace device {

class FidoDiscoveryFactory;

namespace pin {
class TokenResponse;
}  // namespace pin

class COMPONENT_EXPORT(DEVICE_FIDO) GetAssertionRequestHandler
    : public FidoRequestHandlerBase,
      public AuthTokenRequester::Delegate {
 public:
  using CompletionCallback = base::OnceCallback<void(
      GetAssertionStatus,
      std::optional<std::vector<AuthenticatorGetAssertionResponse>>,
      FidoAuthenticator* authenticator)>;

  GetAssertionRequestHandler(
      FidoDiscoveryFactory* fido_discovery_factory,
      std::vector<std::unique_ptr<FidoDiscoveryBase>> additional_discoveries,
      const base::flat_set<FidoTransportProtocol>& supported_transports,
      CtapGetAssertionRequest request_parameter,
      CtapGetAssertionOptions request_options,
      bool allow_skipping_pin_touch,
      CompletionCallback completion_callback);

  GetAssertionRequestHandler(const GetAssertionRequestHandler&) = delete;
  GetAssertionRequestHandler& operator=(const GetAssertionRequestHandler&) =
      delete;

  ~GetAssertionRequestHandler() override;

  // Filters the allow list of the get assertion request to the given
  // |credential|.
  void PreselectAccount(DiscoverableCredentialMetadata credential);

  base::WeakPtr<GetAssertionRequestHandler> GetWeakPtr();

 private:
  enum class State {
    kWaitingForTouch,
    kWaitingForToken,
    kWaitingForResponseWithToken,
    kFinished,
  };

  // FidoRequestHandlerBase:
  void OnBluetoothAdapterEnumerated(bool is_present,
                                    BleStatus ble_status,
                                    bool can_power_on,
                                    bool is_peripheral_role_supported) override;
  void DispatchRequest(FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;
  void GetPlatformCredentialStatus(
      FidoAuthenticator* platform_authenticator) override;

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

  void ObtainPINUVAuthToken(FidoAuthenticator* authenticator,
                            std::set<pin::Permissions> permissions,
                            bool skip_pin_touch,
                            bool internal_uv_locked);
  void HandleResponse(FidoAuthenticator* authenticator,
                      CtapGetAssertionRequest request,
                      base::ElapsedTimer request_timer,
                      GetAssertionStatus response_code,
                      std::vector<AuthenticatorGetAssertionResponse> response);
  void TerminateUnsatisfiableRequestPostTouch(FidoAuthenticator* authenticator);
  void DispatchRequestWithToken(pin::TokenResponse token);

  CompletionCallback completion_callback_;
  State state_ = State::kWaitingForTouch;
  const CtapGetAssertionRequest request_;
  CtapGetAssertionOptions options_;

  // If true, and if at the time the request is dispatched to the first
  // authenticator no other authenticators are available, the request handler
  // will skip the initial touch that is usually required to select a PIN
  // protected authenticator.
  bool allow_skipping_pin_touch_;

  // selected_authenticator_for_pin_uv_auth_token_ points to the authenticator
  // that was tapped by the user while requesting a pinUvAuthToken from
  // connected authenticators. The object is owned by the underlying discovery
  // object and this pointer is cleared if it's removed during processing.
  raw_ptr<FidoAuthenticator> selected_authenticator_for_pin_uv_auth_token_ =
      nullptr;

  // auth_token_requester_map_ holds active AuthTokenRequesters for
  // authenticators that need a pinUvAuthToken to service the request.
  std::map<FidoAuthenticator*, std::unique_ptr<AuthTokenRequester>>
      auth_token_requester_map_;

  // preselected_credential_ is set when the UI invokes `PreselectAccount()`. It
  // contains the credential chosen by the user during a request prior to
  // dispatching to the authenticator.
  std::optional<DiscoverableCredentialMetadata> preselected_credential_;

  SEQUENCE_CHECKER(my_sequence_checker_);
  base::WeakPtrFactory<GetAssertionRequestHandler> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_GET_ASSERTION_REQUEST_HANDLER_H_

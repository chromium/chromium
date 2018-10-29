// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_

#include "device/fido/fido_request_handler_base.h"

#include <utility>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

// Handles receiving response form potentially multiple connected authenticators
// and relaying response to the relying party.
template <class Response>
class FidoRequestHandler : public FidoRequestHandlerBase {
 public:
  using CompletionCallback =
      base::OnceCallback<void(FidoReturnCode status_code,
                              base::Optional<Response> response_data,
                              FidoTransportProtocol transport_used)>;

  // The |available_transports| should be the intersection of transports
  // supported by the client and allowed by the relying party.
  FidoRequestHandler(
      service_manager::Connector* connector,
      const base::flat_set<FidoTransportProtocol>& available_transports,
      CompletionCallback completion_callback)
      : FidoRequestHandlerBase(connector, available_transports),
        completion_callback_(std::move(completion_callback)) {}

  ~FidoRequestHandler() override {
    if (!is_complete())
      CancelActiveAuthenticators();
  }

  bool is_complete() const { return completion_callback_.is_null(); }

 protected:
  // Converts authenticator response code received from CTAP1/CTAP2 device into
  // FidoReturnCode and passes response data to webauth::mojom::Authenticator.
  void OnAuthenticatorResponse(FidoAuthenticator* authenticator,
                               CtapDeviceResponseCode device_response_code,
                               base::Optional<Response> response_data) {
    if (is_complete()) {
      DVLOG(2)
          << "Response from authenticator received after request is complete.";
      return;
    }

    base::Optional<FidoReturnCode> return_code =
        ConvertDeviceResponseCodeToFidoReturnCode(device_response_code,
                                                  response_data.has_value());

    // Any authenticator response codes that do not result from user consent
    // imply that the authenticator should be dropped and that other on-going
    // requests should continue until timeout is reached.
    if (!return_code) {
      active_authenticators().erase(authenticator->GetId());
      return;
    }

    // Once response has been passed to the relying party, cancel all other on
    // going requests.
    CancelActiveAuthenticators(authenticator->GetId());
    std::move(completion_callback_)
        .Run(*return_code, std::move(response_data),
             authenticator->AuthenticatorTransport());
  }

 private:
  static base::Optional<FidoReturnCode>
  ConvertDeviceResponseCodeToFidoReturnCode(
      CtapDeviceResponseCode device_response_code,
      bool response_has_value) {
    switch (device_response_code) {
      case CtapDeviceResponseCode::kSuccess:
        return response_has_value
                   ? FidoReturnCode::kSuccess
                   : FidoReturnCode::kAuthenticatorResponseInvalid;

      // These errors are only returned after the user interacted with the
      // authenticator.
      case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
        return FidoReturnCode::kUserConsentButCredentialExcluded;
      case CtapDeviceResponseCode::kCtap2ErrNoCredentials:
        return FidoReturnCode::kUserConsentButCredentialNotRecognized;

      // The user explicitly denied the operation. Touch ID returns this error
      // when the user cancels the macOS prompt. External authenticators may
      // return it e.g. after the user fails fingerprint verification.
      case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
        return FidoReturnCode::kUserConsentDenied;

      // This error is returned by some authenticators (e.g. the "Yubico FIDO
      // 2" CTAP2 USB keys) during GetAssertion **before the user interacted
      // with the device**. The authenticator does this to avoid blinking (and
      // possibly asking the user for their PIN) for requests it knows
      // beforehand it cannot handle.
      //
      // Ignore this error to avoid canceling the request without user
      // interaction.
      case CtapDeviceResponseCode::kCtap2ErrInvalidCredential:
        return base::nullopt;

      // For all other errors, the authenticator will be dropped, and other
      // authenticators may continue.
      default:
        return base::nullopt;
    }
  }

  CompletionCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_H_

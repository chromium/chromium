// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/enclave/enclave_websocket_client.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_types.h"
#include "device/fido/network_context_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace device::enclave {

struct CredentialRequest;

class COMPONENT_EXPORT(DEVICE_FIDO) EnclaveAuthenticator
    : public FidoAuthenticator {
 public:
  EnclaveAuthenticator(
      std::unique_ptr<CredentialRequest> ui_request,
      NetworkContextFactory network_context_factory);
  ~EnclaveAuthenticator() override;

  EnclaveAuthenticator(const EnclaveAuthenticator&) = delete;
  EnclaveAuthenticator& operator=(const EnclaveAuthenticator&) = delete;

  void SetOauthToken(std::optional<std::string_view> token);

  // FidoAuthenticator:
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void Cancel() override;
  AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  std::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

 private:
  struct PendingGetAssertionRequest {
    PendingGetAssertionRequest(CtapGetAssertionRequest,
                               CtapGetAssertionOptions,
                               GetAssertionCallback);
    ~PendingGetAssertionRequest();
    PendingGetAssertionRequest(const PendingGetAssertionRequest&) = delete;
    PendingGetAssertionRequest& operator=(const PendingGetAssertionRequest&) =
        delete;

    CtapGetAssertionRequest request;
    CtapGetAssertionOptions options;
    GetAssertionCallback callback;
  };

  struct PendingMakeCredentialRequest {
    PendingMakeCredentialRequest(CtapMakeCredentialRequest,
                                 MakeCredentialOptions,
                                 MakeCredentialCallback);
    ~PendingMakeCredentialRequest();
    PendingMakeCredentialRequest(const PendingMakeCredentialRequest&) = delete;
    PendingMakeCredentialRequest& operator=(
        const PendingMakeCredentialRequest&) = delete;

    CtapMakeCredentialRequest request;
    MakeCredentialOptions options;
    MakeCredentialCallback callback;
  };

  void DispatchMakeCredentialWithNewUVKey(
      base::span<const uint8_t> uv_public_key);
  void DispatchGetAssertionWithNewUVKey(
      base::span<const uint8_t> uv_public_key);
  void ProcessMakeCredentialResponse(std::optional<cbor::Value> response);
  void ProcessGetAssertionResponse(std::optional<cbor::Value> response);
  void ProcessErrorResponse(const ErrorResponse& error);

  // `Complete*` methods invoke callbacks that can result in `this` being
  // destroyed, and so should only be called immediately before a return.
  void CompleteRequestWithError(
      absl::variant<GetAssertionStatus, MakeCredentialStatus> error);
  void CompleteMakeCredentialRequest(
      MakeCredentialStatus status,
      std::optional<AuthenticatorMakeCredentialResponse> response);
  void CompleteGetAssertionRequest(
      GetAssertionStatus status,
      std::vector<AuthenticatorGetAssertionResponse> responses);

  const std::array<uint8_t, 8> id_;
  const NetworkContextFactory network_context_factory_;
  const std::unique_ptr<CredentialRequest> ui_request_;

  // Caches the request while waiting for the connection to be established.
  // At most one of these can be non-null at any given time.
  std::unique_ptr<PendingGetAssertionRequest> pending_get_assertion_request_;
  std::unique_ptr<PendingMakeCredentialRequest>
      pending_make_credential_request_;

  // Set to true when the request included a deferred UV key creation.
  bool includes_new_uv_key_ = false;

  base::WeakPtrFactory<EnclaveAuthenticator> weak_factory_{this};
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_

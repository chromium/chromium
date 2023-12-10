// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_

#include <memory>
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
#include "device/fido/fido_types.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace device {

namespace cablev2 {
class Crypter;
class HandshakeInitiator;
}  // namespace cablev2

namespace enclave {

// TODO(kenrb): Remove the export directive when it is no longer used by the
// client stand-alone app.
class COMPONENT_EXPORT(DEVICE_FIDO) EnclaveAuthenticator
    : public FidoAuthenticator {
 public:
  EnclaveAuthenticator(
      const GURL& service_url,
      base::span<const uint8_t, device::kP256X962Length> peer_identity,
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys,
      base::RepeatingCallback<void(sync_pb::WebauthnCredentialSpecifics)>
          save_passkey_callback,
      std::vector<uint8_t> device_id,
      const std::string& username,
      raw_ptr<network::mojom::NetworkContext> network_context,
      EnclaveRequestSigningCallback request_signing_callback);
  ~EnclaveAuthenticator() override;

  EnclaveAuthenticator(const EnclaveAuthenticator&) = delete;
  EnclaveAuthenticator& operator=(const EnclaveAuthenticator&) = delete;

  // TODO(kenrb): Make these private when no longer embedded in test app.
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;

 private:
  enum class State {
    kInitialized,
    kWaitingForHandshakeResponse,
    kConnected,
    kError,
  };

  struct PendingGetAssertionRequest {
    PendingGetAssertionRequest(const CtapGetAssertionRequest&,
                               const CtapGetAssertionOptions&,
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
    PendingMakeCredentialRequest(const CtapMakeCredentialRequest&,
                                 const MakeCredentialOptions&,
                                 MakeCredentialCallback);
    ~PendingMakeCredentialRequest();
    PendingMakeCredentialRequest(const PendingMakeCredentialRequest&) = delete;
    PendingMakeCredentialRequest& operator=(
        const PendingMakeCredentialRequest&) = delete;

    CtapMakeCredentialRequest request;
    MakeCredentialOptions options;
    MakeCredentialCallback callback;
  };

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void Cancel() override;
  AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  absl::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  void StartRequest();
  void OnResponseReceived(EnclaveWebSocketClient::SocketStatus status,
                          absl::optional<std::vector<uint8_t>> data);
  void BuildCommand();
  void SendCommand(std::vector<uint8_t> command_body);
  void CompleteRequestWithError(CtapDeviceResponseCode error);
  void CompleteMakeCredentialRequest(
      CtapDeviceResponseCode status,
      absl::optional<AuthenticatorMakeCredentialResponse> response);
  void CompleteGetAssertionRequest(
      CtapDeviceResponseCode status,
      std::vector<AuthenticatorGetAssertionResponse> responses);

  State state_ = State::kInitialized;

  std::unique_ptr<EnclaveWebSocketClient> websocket_client_;

  // The peer's public key.
  const std::array<uint8_t, device::kP256X962Length> peer_identity_;

  // Synced passkeys available for this account. Calls to |GetAssertion| must
  // identify one from this list in the request's allowCredentials.
  std::vector<sync_pb::WebauthnCredentialSpecifics> available_passkeys_;

  // Callback for storing a newly-created passkey.
  base::RepeatingCallback<void(sync_pb::WebauthnCredentialSpecifics)>
      save_passkey_callback_;

  // Identifier for this device, previously registered to the enclave.
  std::vector<uint8_t> device_id_;

  // Callback for signing requests with the device-bound key.
  EnclaveRequestSigningCallback request_signing_callback_;

  // Fields for establishing and using the encrypted channel.
  std::unique_ptr<cablev2::HandshakeInitiator> handshake_;
  absl::optional<std::array<uint8_t, 32>> handshake_hash_;
  std::unique_ptr<cablev2::Crypter> crypter_;

  // Caches the request while waiting for the connection to be established.
  // At most one of these can be non-null at any given time.
  std::unique_ptr<PendingGetAssertionRequest> pending_get_assertion_request_;
  std::unique_ptr<PendingMakeCredentialRequest>
      pending_make_credential_request_;

  base::WeakPtrFactory<EnclaveAuthenticator> weak_factory_{this};
};

}  // namespace enclave

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_

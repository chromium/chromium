// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace device {

namespace cablev2 {
class Crypter;
class HandshakeInitiator;
}  // namespace cablev2

namespace enclave {

class EnclaveHttpClient;

// TODO(kenrb): Remove the export directive when it is no longer used by the
// client stand-alone app.
class COMPONENT_EXPORT(DEVICE_FIDO) EnclaveAuthenticator
    : public FidoAuthenticator {
 public:
  EnclaveAuthenticator(
      const GURL& service_url,
      base::span<const uint8_t, device::kP256X962Length> peer_identity,
      std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys);
  ~EnclaveAuthenticator() override;

  EnclaveAuthenticator(const EnclaveAuthenticator&) = delete;
  EnclaveAuthenticator& operator=(const EnclaveAuthenticator&) = delete;

  // TODO(kenrb): Make this private when no longer embedded in test app.
  void GetAssertion(CtapGetAssertionRequest request,
                    CtapGetAssertionOptions options,
                    GetAssertionCallback callback) override;

 private:
  enum class State {
    kInitialized,
    kWaitingForHandshakeResponse,
    kConnected,
    kError,
  };

  // FidoAuthenticator:
  void InitializeAuthenticator(base::OnceClosure callback) override;
  void MakeCredential(CtapMakeCredentialRequest request,
                      MakeCredentialOptions options,
                      MakeCredentialCallback callback) override;
  void Cancel() override;
  AuthenticatorType GetType() const override;
  std::string GetId() const override;
  const AuthenticatorSupportedOptions& Options() const override;
  absl::optional<FidoTransportProtocol> AuthenticatorTransport() const override;
  base::WeakPtr<FidoAuthenticator> GetWeakPtr() override;

  void OnResponseReceived(int status,
                          absl::optional<std::vector<uint8_t>> data);
  void SendCommand();

  State state_ = State::kInitialized;

  std::unique_ptr<EnclaveHttpClient> http_client_;

  // The peer's public key.
  const std::array<uint8_t, device::kP256X962Length> peer_identity_;

  std::unique_ptr<cablev2::HandshakeInitiator> handshake_;
  absl::optional<std::array<uint8_t, 32>> handshake_hash_;
  std::unique_ptr<cablev2::Crypter> crypter_;

  // GetAssertion arguments while waiting for the connection to be established.
  std::string pending_request_body_;
  GetAssertionCallback pending_get_assertion_callback_;

  std::vector<sync_pb::WebauthnCredentialSpecifics> available_passkeys_;

  base::WeakPtrFactory<EnclaveAuthenticator> weak_factory_{this};
};

}  // namespace enclave

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_AUTHENTICATOR_H_

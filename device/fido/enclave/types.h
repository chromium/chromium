// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_TYPES_H_
#define DEVICE_FIDO_ENCLAVE_TYPES_H_

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "crypto/sha2.h"
#include "device/fido/fido_constants.h"
#include "url/gurl.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace device::enclave {

// EnclaveIdentity contains addressing and identity information needed to
// connect to an enclave.
struct COMPONENT_EXPORT(DEVICE_FIDO) EnclaveIdentity {
  EnclaveIdentity();
  ~EnclaveIdentity();
  EnclaveIdentity(const EnclaveIdentity&);

  GURL url;
  std::array<uint8_t, kP256X962Length> public_key;
};

// ClientKeyType enumerates the types of identity keys that a client might
// register with an enclave.
enum class ClientKeyType {
  // kHardware ("hw") keys are hardware-bound, but can be used silently.
  kHardware,
  // kUserVerified ("uv") keys are hardware-bound, but can only be used for
  // signing after the user has performed some explicit action such as providing
  // a local biometric or PIN.
  kUserVerified,
};

// A ClientSignature is the result of signing an enclave request with a
// client-side identity key.
struct COMPONENT_EXPORT(DEVICE_FIDO) ClientSignature {
  ClientSignature();
  ~ClientSignature();
  ClientSignature(const ClientSignature&);
  ClientSignature(ClientSignature&&);

  std::vector<uint8_t> device_id;
  std::vector<uint8_t> signature;
  ClientKeyType key_type;
};

// Message format that can be signed by SignedCallback.
using SignedMessage = std::array<uint8_t, 2 * crypto::kSHA256Length>;

// A SigningCallback is used to sign an encoded array of enclave requests.
using SigningCallback = base::OnceCallback<void(
    SignedMessage,
    base::OnceCallback<void(std::optional<ClientSignature>)>)>;

// A CredentialRequest contains the values that, in addition to a CTAP request,
// are needed for building a fully-formed enclave request.
struct COMPONENT_EXPORT(DEVICE_FIDO) CredentialRequest {
  CredentialRequest();
  ~CredentialRequest();
  CredentialRequest(CredentialRequest&&);

  SigningCallback signing_callback;
  // access_token contains an OAuth2 token to authenticate access to the enclave
  // at the account level.
  std::string access_token;
  // wrapped_secrets contains one or more security domain secrets, wrapped by
  // the enclave. These wrapped secrets are sent to the enclave so that it can
  // unwrap them and perform the requested operation.
  std::vector<std::vector<uint8_t>> wrapped_secrets;
  // Required for create() requests: the version/epoch of the single wrapped
  // secret in `wrapped_secrets`.
  std::optional<int32_t> wrapped_secret_version;
  // entity optionally contains a passkey Sync entity. This may be omitted for
  // create() requests.
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_TYPES_H_

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

// A PIN entered by the user, after hashing and encoding.
struct COMPONENT_EXPORT(DEVICE_FIDO) ClaimedPIN {
  explicit ClaimedPIN(std::vector<uint8_t> pin_claim,
                      std::vector<uint8_t> wrapped_pin);
  ~ClaimedPIN();
  ClaimedPIN(ClaimedPIN&) = delete;
  ClaimedPIN(ClaimedPIN&&) = delete;

  // The hashed PIN, encrypted to the claim key.
  std::vector<uint8_t> pin_claim;
  // The true PIN hash, encrypted to the security domain secret.
  std::vector<uint8_t> wrapped_pin;
};

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
  // wrapped_secret contains the wrapped security domain secret, wrapped by
  // the enclave. This wrapped secret is sent to the enclave so that it can
  // unwrap it and perform the requested operation.
  std::optional<std::vector<uint8_t>> wrapped_secret;
  // secret contains the security domain secret itself. The enclave can use
  // this instead of a wrapped secret and possession of the actual secret
  // is sufficient to show user verification.
  std::optional<std::vector<uint8_t>> secret;
  // Required for create() requests: the version/epoch of `wrapped_secret`.
  std::optional<int32_t> key_version;
  // entity optionally contains a passkey Sync entity. This may be omitted for
  // create() requests.
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  // The PIN entered by the user (wrapped for the enclave), and the correct PIN
  // (encrypted to the security domain secret). Optional, may be nullptr.
  std::unique_ptr<ClaimedPIN> claimed_pin;
};

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_TYPES_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/enclave/types.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace cbor {
class Value;
}

namespace device {

class JSONRequest;

namespace enclave {

// Parses a decrypted assertion command response from the enclave.
std::pair<std::optional<AuthenticatorGetAssertionResponse>, std::string>
    COMPONENT_EXPORT(DEVICE_FIDO)
        ParseGetAssertionResponse(cbor::Value response_value,
                                  base::span<const uint8_t> credential_id);

// Parses a decrypted registration command response from the enclave.
std::tuple<std::optional<AuthenticatorMakeCredentialResponse>,
           std::optional<sync_pb::WebauthnCredentialSpecifics>,
           std::string>
    COMPONENT_EXPORT(DEVICE_FIDO)
        ParseMakeCredentialResponse(cbor::Value response,
                                    const CtapMakeCredentialRequest& request,
                                    int32_t wrapped_secret_version);

// Returns a CBOR value with the provided GetAssertion request and associated
// passkey. The return value can be serialized into a Command request according
// to the enclave protocol.
cbor::Value COMPONENT_EXPORT(DEVICE_FIDO) BuildGetAssertionCommand(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request,
    std::string client_data_hash,
    std::vector<std::vector<uint8_t>> wrapped_secrets);

// Returns a CBOR value with the provided MakeCredential request. The return
// value can be serialized into a Command request according to the enclave
// protocol.
cbor::Value COMPONENT_EXPORT(DEVICE_FIDO)
    BuildMakeCredentialCommand(scoped_refptr<JSONRequest> request,
                               std::vector<uint8_t> wrapped_secret);

// Builds a CBOR serialization of the command to be sent to the enclave
// service which can then be encrypted and sent over HTTPS.
//
// |command| is either an array (in which case it is used directly) or another
//     type of object (in which case it will be wrapped in a 1-element array).
// |signing_callback| is used to generate the signature over the encoded
//     command using the protected private key. It can be null if the command
//     does not need to be authenticated.
// |handshake_hash| is the 32-byte hash from the Noise handshake.
// |complete_callback| is invoked with the finished serialized command.
void COMPONENT_EXPORT(DEVICE_FIDO) BuildCommandRequestBody(
    cbor::Value command,
    SigningCallback signing_callback,
    base::span<const uint8_t, crypto::kSHA256Length> handshake_hash,
    base::OnceCallback<void(std::vector<uint8_t>)> complete_callback);

}  // namespace enclave

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

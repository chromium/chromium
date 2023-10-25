// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace device {

class JSONRequest;

namespace enclave {

const char kInitPath[] = "v1/init";
const char kCommandPath[] = "v1/cmd";

// Keys for the RPC param names to the HTTP front end:
const char kInitSessionRequestData[] = "request";
const char kInitSessionResponseData[] = "response";
const char kSessionId[] = "session_name";
const char kSendCommandRequestData[] = "request";
const char kSendCommandResponseData[] = "response";

// The first argument is the handshake_hash, the second is the data that will
// be signed. This is invoked asynchronously on a different thread, so there
// must not be any thread-local dependencies in the callback.
using EnclaveRequestSigningCallback =
    base::RepeatingCallback<std::vector<uint8_t>(base::span<const uint8_t>,
                                                 base::span<const uint8_t>)>;

// Parses a decrypted command response from the enclave.
std::pair<absl::optional<AuthenticatorGetAssertionResponse>, std::string>
ParseGetAssertionResponse(const std::vector<uint8_t>& response_cbor,
                          base::span<uint8_t> credential_id);

// Returns a CBOR value with the provided GetAssertion request and associated
// passkey. The return value can be serialized into a Command request according
// to the enclave protocol.
cbor::Value BuildGetAssertionCommand(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request,
    std::string client_data_hash,
    std::string rp_id);

// Builds a CBOR serialization of the command to be sent to the enclave
// service which can then be encrypted and sent over HTTPS.
// |command_callback| is used to generate the encoded MakeCredential or
//     GetAssertion command.
// |signing_callback| is used to generate the signature over the encoded
//     command using the protected private key.
// |device_id| is the unique identifier for this device which the server uses
//     to look up the previously-registered public key.
// |complete_callback| is invoked with the finished serialized command.
void BuildCommandRequestBody(
    base::OnceCallback<cbor::Value()> command_callback,
    EnclaveRequestSigningCallback signing_callback,
    base::span<uint8_t> handshake_hash,
    const std::vector<uint8_t>& device_id,
    base::OnceCallback<void(std::vector<uint8_t>)> complete_callback);

// For testing only. (Also this is obsolete, the test service code needs to
// be updated).
std::string COMPONENT_EXPORT(DEVICE_FIDO)
    AuthenticatorGetAssertionResponseToJson(
        const AuthenticatorGetAssertionResponse& response);

// For testing only. (Also this is obsolete, the test service code needs to
// be updated).
bool COMPONENT_EXPORT(DEVICE_FIDO) ParseGetAssertionRequestBody(
    const std::string& request_body,
    sync_pb::WebauthnCredentialSpecifics* out_passkey,
    base::Value* out_request);

}  // namespace enclave

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

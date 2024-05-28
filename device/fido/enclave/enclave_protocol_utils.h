// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/enclave/types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}

namespace cbor {
class Value;
}

namespace device {

class JSONRequest;

namespace enclave {

// Represents an error encountered while parsing a response from the enclave
// service. This can be parsing errors or specified errors returned from the
// service.
// If the service returned an error response, `index` indicates which response
// in the array contained the error. `index` is -1 if there was an error
// parsing the response.
// `error_code` when present represents a code received in an error response
// from the enclave. `error_string` when present represents a string received in
// an error response from the enclave, or a description of the parsing error
// encountered.
struct COMPONENT_EXPORT(DEVICE_FIDO) ErrorResponse {
  explicit ErrorResponse(std::string error);
  ErrorResponse(int index, int error_code);
  ErrorResponse(int index, std::string error);
  ~ErrorResponse();
  ErrorResponse(ErrorResponse&);
  ErrorResponse(ErrorResponse&&);

  int index = -1;
  std::optional<int> error_code;
  std::optional<std::string> error_string;
};

// Parses a decrypted assertion command response from the enclave.
// If there are multiple request responses in the array, it assumes the last
// one is for the GetAssertion.
// Returns one of: A successful response, or a struct containing details of
//                 the error.
absl::variant<AuthenticatorGetAssertionResponse, ErrorResponse>
    COMPONENT_EXPORT(DEVICE_FIDO)
        ParseGetAssertionResponse(cbor::Value response_value,
                                  base::span<const uint8_t> credential_id);

// Parses a decrypted registration command response from the enclave.
// If there are multiple request responses in the array, it assumes the last
// one is for the MakeCredential.
// Returns one of: A pair containing the response and the new passkey entity,
//                 a struct containing details of the error.
absl::variant<std::pair<AuthenticatorMakeCredentialResponse,
                        sync_pb::WebauthnCredentialSpecifics>,
              ErrorResponse>
    COMPONENT_EXPORT(DEVICE_FIDO)
        ParseMakeCredentialResponse(cbor::Value response,
                                    const CtapMakeCredentialRequest& request,
                                    int32_t wrapped_secret_version,
                                    bool user_verified);

// Returns a CBOR value with the provided GetAssertion request and associated
// passkey. The return value can be serialized into a Command request according
// to the enclave protocol.
cbor::Value COMPONENT_EXPORT(DEVICE_FIDO) BuildGetAssertionCommand(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request,
    std::string client_data_hash,
    std::unique_ptr<ClaimedPIN> claimed_pin,
    std::optional<std::vector<uint8_t>> wrapped_secret,
    std::optional<std::vector<uint8_t>> secret);

// Returns a CBOR value with the provided MakeCredential request. The return
// value can be serialized into a Command request according to the enclave
// protocol.
cbor::Value COMPONENT_EXPORT(DEVICE_FIDO) BuildMakeCredentialCommand(
    scoped_refptr<JSONRequest> request,
    std::unique_ptr<ClaimedPIN> claimed_pin,
    std::optional<std::vector<uint8_t>> wrapped_secret,
    std::optional<std::vector<uint8_t>> secret);

// Returns a CBOR value with the provided AddUVKey command to the enclave.
// It must precede a credential registration or assertion request in the
// command array.
cbor::Value COMPONENT_EXPORT(DEVICE_FIDO)
    BuildAddUVKeyCommand(base::span<const uint8_t> uv_public_key);

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
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
        complete_callback);

}  // namespace enclave

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

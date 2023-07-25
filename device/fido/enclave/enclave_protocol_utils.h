// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

#include <string>
#include <utility>

#include "base/component_export.h"
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

// For testing only.
std::pair<absl::optional<CtapGetAssertionRequest>, std::string>
    COMPONENT_EXPORT(DEVICE_FIDO)
        CtapGetAssertionRequestFromJson(const std::string& json);

// For testing only.
std::string COMPONENT_EXPORT(DEVICE_FIDO)
    AuthenticatorGetAssertionResponseToJson(
        const AuthenticatorGetAssertionResponse& response);

std::pair<absl::optional<AuthenticatorGetAssertionResponse>, std::string>
AuthenticatorGetAssertionResponseFromJson(const std::string& json);

void BuildGetAssertionRequestBody(
    const sync_pb::WebauthnCredentialSpecifics& passkey,
    scoped_refptr<JSONRequest> request,
    std::string* out_request_body);

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_PROTOCOL_UTILS_H_

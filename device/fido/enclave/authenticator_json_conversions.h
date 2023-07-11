// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_AUTHENTICATOR_JSON_CONVERSIONS_H_
#define DEVICE_FIDO_ENCLAVE_AUTHENTICATOR_JSON_CONVERSIONS_H_

#include <string>
#include <utility>

#include "base/values.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

std::string CtapGetAssertionRequestToJson(
    const CtapGetAssertionRequest& request);

// For testing only.
std::pair<absl::optional<CtapGetAssertionRequest>, std::string>
CtapGetAssertionRequestFromJson(const std::string& json);

// For testing only.
std::string AuthenticatorGetAssertionResponseToJson(
    const AuthenticatorGetAssertionResponse& response);

std::pair<absl::optional<AuthenticatorGetAssertionResponse>, std::string>
AuthenticatorGetAssertionRequestFromJson(const std::string& json);

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_AUTHENTICATOR_JSON_CONVERSIONS_H_

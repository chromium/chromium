// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_get_assertion_request.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

CtapGetAssertionRequest::CtapGetAssertionRequest(
    std::string in_rp_id,
    std::string in_client_data_json)
    : rp_id(std::move(in_rp_id)),
      client_data_json(std::move(in_client_data_json)),
      client_data_hash(fido_parsing_utils::CreateSHA256Hash(client_data_json)) {
}

CtapGetAssertionRequest::CtapGetAssertionRequest(
    const CtapGetAssertionRequest& that) = default;

CtapGetAssertionRequest::CtapGetAssertionRequest(
    CtapGetAssertionRequest&& that) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    const CtapGetAssertionRequest& other) = default;

CtapGetAssertionRequest& CtapGetAssertionRequest::operator=(
    CtapGetAssertionRequest&& other) = default;

CtapGetAssertionRequest::~CtapGetAssertionRequest() = default;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetAssertionRequest& request) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value(1)] = cbor::Value(request.rp_id);
  cbor_map[cbor::Value(2)] = cbor::Value(request.client_data_hash);

  if (!request.allow_list.empty()) {
    cbor::Value::ArrayValue allow_list_array;
    for (const auto& descriptor : request.allow_list) {
      allow_list_array.push_back(AsCBOR(descriptor));
    }
    cbor_map[cbor::Value(3)] = cbor::Value(std::move(allow_list_array));
  }

  if (request.pin_auth) {
    cbor_map[cbor::Value(6)] = cbor::Value(*request.pin_auth);
  }

  if (request.pin_protocol) {
    cbor_map[cbor::Value(7)] = cbor::Value(*request.pin_protocol);
  }

  cbor::Value::MapValue option_map;

  // User presence is required by default.
  if (!request.user_presence_required) {
    option_map[cbor::Value(kUserPresenceMapKey)] =
        cbor::Value(request.user_presence_required);
  }

  // User verification is not required by default.
  if (request.user_verification == UserVerificationRequirement::kRequired) {
    option_map[cbor::Value(kUserVerificationMapKey)] = cbor::Value(true);
  }

  if (!option_map.empty()) {
    cbor_map[cbor::Value(5)] = cbor::Value(std::move(option_map));
  }

  return std::make_pair(CtapRequestCommand::kAuthenticatorGetAssertion,
                        cbor::Value(std::move(cbor_map)));
}

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetNextAssertionRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorGetNextAssertion,
                        base::nullopt);
}

}  // namespace device

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_id_token_decoder.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"

namespace {

// The name of the service flag that defines the account is Unicorn.
const char kChildAccountServiceFlag[] = "uca";

// The name of the service flag that defines the account is in advanced
// protection program.
const char kAdvancedProtectionAccountServiceFlag[] = "tia";

// The key indexing service flags in the ID token JSON.
const char kServicesKey[] = "services";

// Decodes the JWT ID token to a dictionary. Returns whether the decoding was
// successful.
std::optional<base::Value::Dict> DecodeIdToken(std::string_view id_token) {
  const std::vector<std::string_view> token_pieces = base::SplitStringPiece(
      id_token, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (token_pieces.size() != 3) {
    VLOG(1) << "Invalid id_token: not in JWT format";
    return std::nullopt;
  }
  // Only the payload is used. The header is ignored, and signature
  // verification is not needed since the token was obtained directly from LSO.
  std::string payload;
  if (!base::Base64UrlDecode(token_pieces[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &payload)) {
    VLOG(1) << "Invalid id_token: not in Base64Url encoding";
    return std::nullopt;
  }
  std::optional<base::Value> decoded_payload = base::JSONReader::Read(payload);
  if (!decoded_payload.has_value() ||
      decoded_payload->type() != base::Value::Type::DICT) {
    VLOG(1) << "Invalid id_token: paylod is not a well-formed JSON";
    return std::nullopt;
  }
  return std::move(decoded_payload->GetDict());
}

// Obtains a vector of service flags from the encoded JWT ID token. Returns
// whether decoding the ID token and obtaining the list of service flags from it
// was successful.
bool GetServiceFlags(std::string_view id_token,
                     std::vector<std::string>* out_service_flags) {
  DCHECK(out_service_flags->empty());

  std::optional<base::Value::Dict> decoded_payload = DecodeIdToken(id_token);
  if (!decoded_payload.has_value()) {
    VLOG(1) << "Failed to decode the id_token";
    return false;
  }
  const base::Value::List* service_flags_value_raw =
      decoded_payload->FindList(kServicesKey);
  if (service_flags_value_raw == nullptr) {
    VLOG(1) << "Missing service flags in the id_token";
    return false;
  }
  for (const auto& flag_value : *service_flags_value_raw) {
    const std::string& flag = flag_value.GetString();
    if (flag.size())
      out_service_flags->push_back(flag);
  }
  return true;
}

}  // namespace

namespace gaia {

TokenServiceFlags ParseServiceFlags(const std::string& id_token) {
  TokenServiceFlags token_service_flags;
  std::vector<std::string> service_flags;
  if (!GetServiceFlags(id_token, &service_flags)) {
    // If service flags can’t be obtained, then assume these service flags
    // are not set.
    VLOG(1) << "Assuming the account doesn't have any service flag set "
            << "due to decoding failure";
    return token_service_flags;
  }

  token_service_flags.is_child_account =
      base::Contains(service_flags, kChildAccountServiceFlag);
  token_service_flags.is_under_advanced_protection =
      base::Contains(service_flags, kAdvancedProtectionAccountServiceFlag);
  return token_service_flags;
}

}  // namespace gaia

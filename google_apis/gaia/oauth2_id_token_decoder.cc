// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth2_id_token_decoder.h"

#include <memory>

#include "base/base64url.h"
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
std::unique_ptr<base::Value> DecodeIdToken(const std::string id_token) {
  const std::vector<base::StringPiece> token_pieces =
      base::SplitStringPiece(base::StringPiece(id_token), ".",
                             base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (token_pieces.size() != 3) {
    VLOG(1) << "Invalid id_token: not in JWT format";
    return nullptr;
  }
  // Only the payload is used. The header is ignored, and signature
  // verification is not needed since the token was obtained directly from LSO.
  std::string payload;
  if (!base::Base64UrlDecode(token_pieces[1],
                             base::Base64UrlDecodePolicy::IGNORE_PADDING,
                             &payload)) {
    VLOG(1) << "Invalid id_token: not in Base64Url encoding";
    return nullptr;
  }
  std::unique_ptr<base::Value> decoded_payload =
      base::JSONReader::ReadDeprecated(payload);
  if (!decoded_payload.get() ||
      decoded_payload->type() != base::Value::Type::DICTIONARY) {
    VLOG(1) << "Invalid id_token: paylod is not a well-formed JSON";
    return nullptr;
  }
  return decoded_payload;
}

// Obtains a vector of service flags from the encoded JWT ID token. Returns
// whether decoding the ID token and obtaining the list of service flags from it
// was successful.
bool GetServiceFlags(const std::string id_token,
                     std::vector<std::string>* out_service_flags) {
  DCHECK(out_service_flags->empty());

  std::unique_ptr<base::Value> decoded_payload = DecodeIdToken(id_token);
  if (decoded_payload == nullptr) {
    VLOG(1) << "Failed to decode the id_token";
    return false;
  }
  const base::Value* service_flags_value_raw =
      decoded_payload->FindKeyOfType(kServicesKey, base::Value::Type::LIST);
  if (service_flags_value_raw == nullptr) {
    VLOG(1) << "Missing service flags in the id_token";
    return false;
  }
  for (const auto& flag_value : service_flags_value_raw->GetList()) {
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
      std::find(service_flags.begin(), service_flags.end(),
                kChildAccountServiceFlag) != service_flags.end();
  token_service_flags.is_under_advanced_protection =
      std::find(service_flags.begin(), service_flags.end(),
                kAdvancedProtectionAccountServiceFlag) != service_flags.end();
  return token_service_flags;
}

}  // namespace gaia

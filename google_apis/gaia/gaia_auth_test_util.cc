// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_test_util.h"

#include "google_apis/gaia/oauth2_mint_token_consent_result.pb.h"

namespace gaia {

std::string GenerateOAuth2MintTokenConsentResult(
    std::optional<bool> approved,
    const std::optional<std::string>& encrypted_approval_data,
    const std::optional<std::string>& obfuscated_id,
    base::Base64UrlEncodePolicy encode_policy) {
  OAuth2MintTokenConsentResult consent_result;
  if (approved.has_value())
    consent_result.set_approved(approved.value());
  if (encrypted_approval_data.has_value())
    consent_result.set_encrypted_approval_data(encrypted_approval_data.value());
  if (obfuscated_id.has_value())
    consent_result.set_obfuscated_id(obfuscated_id.value());
  std::string serialized_consent = consent_result.SerializeAsString();
  std::string encoded_consent;
  base::Base64UrlEncode(serialized_consent, encode_policy, &encoded_consent);
  return encoded_consent;
}

}  // namespace gaia

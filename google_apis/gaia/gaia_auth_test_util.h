// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_

#include <optional>
#include <string>

#include "base/base64url.h"
#include "google_apis/gaia/gaia_id.h"

namespace gaia {

// Parameters for the fake ListAccounts response.
struct CookieParams {
  std::string email;
  GaiaId gaia_id;
  bool valid;
  bool signed_out;
  bool verified;
};

std::string GenerateOAuth2MintTokenConsentResult(
    std::optional<bool> approved,
    const std::optional<std::string>& encrypted_approval_data,
    const std::optional<GaiaId>& obfuscated_id,
    base::Base64UrlEncodePolicy encode_policy =
        base::Base64UrlEncodePolicy::OMIT_PADDING);

// Creates the content for listing accounts in binary format.
std::string CreateListAccountsResponseInBinaryFormat(
    const std::vector<CookieParams>& params);

// Creates the content for listing accounts.
std::string CreateListAccountsResponseInLegacyFormat(
    const std::vector<CookieParams>& params);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_

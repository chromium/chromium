// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_

#include <optional>
#include <string>

#include "base/base64url.h"

namespace gaia {

std::string GenerateOAuth2MintTokenConsentResult(
    std::optional<bool> approved,
    const std::optional<std::string>& encrypted_approval_data,
    const std::optional<std::string>& obfuscated_id,
    base::Base64UrlEncodePolicy encode_policy =
        base::Base64UrlEncodePolicy::OMIT_PADDING);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_

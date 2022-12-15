// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_

#include "base/base64url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include <string>

namespace gaia {

std::string GenerateOAuth2MintTokenConsentResult(
    absl::optional<bool> approved,
    const absl::optional<std::string>& encrypted_approval_data,
    const absl::optional<std::string>& obfuscated_id,
    base::Base64UrlEncodePolicy encode_policy =
        base::Base64UrlEncodePolicy::OMIT_PADDING);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_TEST_UTIL_H_

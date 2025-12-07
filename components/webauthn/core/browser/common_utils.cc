// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/common_utils.h"

#include <string>

#include "base/base64url.h"
#include "base/containers/span.h"

namespace webauthn {

std::string Base64UrlEncodeOmitPadding(
    const base::span<const uint8_t> challenge) {
  std::string ret;
  base::Base64UrlEncode(challenge, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &ret);
  return ret;
}

}  // namespace webauthn

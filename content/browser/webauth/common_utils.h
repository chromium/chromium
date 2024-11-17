// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_COMMON_UTILS_H_
#define CONTENT_BROWSER_WEBAUTH_COMMON_UTILS_H_

#include <string>

#include "base/containers/span.h"

namespace content {

std::string Base64UrlEncodeChallenge(const base::span<const uint8_t> challenge);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_COMMON_UTILS_H_

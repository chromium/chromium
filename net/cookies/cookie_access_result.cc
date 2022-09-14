// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_access_result.h"

namespace net {

CookieAccessResult::CookieAccessResult() = default;

CookieAccessResult::CookieAccessResult(
    CookieEffectiveSameSite effective_same_site,
    CookieInclusionStatus status,
    CookieAccessSemantics access_semantics,
    bool is_allowed_to_access_secure_cookies)
    : status(status),
      effective_same_site(effective_same_site),
      access_semantics(access_semantics),
      is_allowed_to_access_secure_cookies(is_allowed_to_access_secure_cookies) {
}

CookieAccessResult::CookieAccessResult(CookieInclusionStatus status)
    : status(status) {}

CookieAccessResult::CookieAccessResult(const CookieAccessResult&) = default;

CookieAccessResult& CookieAccessResult::operator=(
    const CookieAccessResult& cookie_access_result) = default;

CookieAccessResult::CookieAccessResult(CookieAccessResult&&) = default;

CookieAccessResult::~CookieAccessResult() = default;

}  // namespace net

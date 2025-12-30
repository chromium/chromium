// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/cookie_craving_display.h"

namespace net::device_bound_sessions {

CookieCravingDisplay::CookieCravingDisplay() = default;

CookieCravingDisplay::CookieCravingDisplay(const std::string& name,
                                           const std::string& domain,
                                           const std::string& path,
                                           bool secure,
                                           bool http_only,
                                           net::CookieSameSite same_site)
    : name(name),
      domain(domain),
      path(path),
      secure(secure),
      http_only(http_only),
      same_site(same_site) {}

CookieCravingDisplay::~CookieCravingDisplay() = default;

CookieCravingDisplay::CookieCravingDisplay(const CookieCravingDisplay&) =
    default;
CookieCravingDisplay& CookieCravingDisplay::operator=(
    const CookieCravingDisplay&) = default;
CookieCravingDisplay::CookieCravingDisplay(CookieCravingDisplay&&) noexcept =
    default;
CookieCravingDisplay& CookieCravingDisplay::operator=(
    CookieCravingDisplay&&) noexcept = default;

}  // namespace net::device_bound_sessions

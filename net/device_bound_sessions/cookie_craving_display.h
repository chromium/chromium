// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_COOKIE_CRAVING_DISPLAY_H_
#define NET_DEVICE_BOUND_SESSIONS_COOKIE_CRAVING_DISPLAY_H_

#include <string>

#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"

namespace net::device_bound_sessions {

// This class represents a display-friendly version of a CookieCraving. Used for
// DevTools.
struct NET_EXPORT CookieCravingDisplay {
  CookieCravingDisplay();
  CookieCravingDisplay(const std::string& name,
                       const std::string& domain,
                       const std::string& path,
                       bool secure,
                       bool http_only,
                       net::CookieSameSite same_site);
  ~CookieCravingDisplay();

  CookieCravingDisplay(const CookieCravingDisplay&);
  CookieCravingDisplay& operator=(const CookieCravingDisplay&);
  CookieCravingDisplay(CookieCravingDisplay&&) noexcept;
  CookieCravingDisplay& operator=(CookieCravingDisplay&&) noexcept;
  friend bool operator==(const CookieCravingDisplay&,
                         const CookieCravingDisplay&) = default;

  std::string name;
  std::string domain;
  std::string path;
  bool secure = false;
  bool http_only = false;
  net::CookieSameSite same_site = net::CookieSameSite::UNSPECIFIED;
};

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_COOKIE_CRAVING_DISPLAY_H_

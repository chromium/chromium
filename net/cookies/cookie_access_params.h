// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_ACCESS_PARAMS_H_
#define NET_COOKIES_COOKIE_ACCESS_PARAMS_H_

#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"

namespace net {

// Params used to determine whether a cookie can be accessed.
struct NET_EXPORT CookieAccessParams {
  CookieAccessParams() = delete;
  CookieAccessParams(CookieAccessSemantics access_semantics,
                     bool delegate_treats_url_as_trustworthy);

  // |access_semantics| is the access mode of the cookie access check.
  CookieAccessSemantics access_semantics = CookieAccessSemantics::UNKNOWN;

  // |delegate_treats_url_as_trustworthy| should be true iff the
  // CookieAccessDelegate has authorized access to secure cookies from URLs
  // which might not otherwise be able to do so.
  bool delegate_treats_url_as_trustworthy = false;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_ACCESS_PARAMS_H_

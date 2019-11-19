// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_CANONICAL_COOKIE_TEST_HELPERS_H_
#define NET_COOKIES_CANONICAL_COOKIE_TEST_HELPERS_H_

#include <string>

#include "net/cookies/canonical_cookie.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

MATCHER_P(MatchesCookieLine, cookie_line, "") {
  std::string argument_line = CanonicalCookie::BuildCookieLine(arg);
  if (argument_line == cookie_line)
    return true;

  *result_listener << argument_line;
  return false;
}

}  // namespace net

#endif  // NET_COOKIES_CANONICAL_COOKIE_TEST_HELPERS_H_

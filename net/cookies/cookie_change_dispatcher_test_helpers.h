// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_CHANGE_DISPATCHER_TEST_HELPERS_H_
#define NET_COOKIES_COOKIE_CHANGE_DISPATCHER_TEST_HELPERS_H_

#include <ostream>

#include "net/cookies/cookie_change_dispatcher.h"

namespace net {

// Google Test helper for printing CookieChangeCause values.
std::ostream& operator<<(std::ostream& os, const CookieChangeCause& cause);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_CHANGE_DISPATCHER_TEST_HELPERS_H_

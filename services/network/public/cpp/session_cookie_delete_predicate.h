// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SESSION_COOKIE_DELETE_PREDICATE_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SESSION_COOKIE_DELETE_PREDICATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "net/cookies/cookie_constants.h"

namespace network {
// A DeleteCookiePredicate callback function decides if the cookie associated
// with the domain and is_https status should be deleted on exit, and is used
// when creating a cookie storage policy. It has two parameters, the first one
// is the domain of a cookie and the second one is the scheme of the domain.
using DeleteCookiePredicate =
    base::RepeatingCallback<bool(const std::string&, net::CookieSourceScheme)>;

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SESSION_COOKIE_DELETE_PREDICATE_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_CANONICAL_COOKIE_PROTO_CONVERTER_H_
#define NET_COOKIES_CANONICAL_COOKIE_PROTO_CONVERTER_H_

#include <stdint.h>

#include <memory>

#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie.pb.h"

namespace canonical_cookie_proto {

std::unique_ptr<net::CanonicalCookie> Convert(
    const canonical_cookie_proto::CanonicalCookie& cookie);

}

#endif  // NET_COOKIES_CANONICAL_COOKIE_PROTO_CONVERTER_H_

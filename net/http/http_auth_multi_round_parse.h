// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_MULTI_ROUND_PARSE_H_
#define NET_HTTP_HTTP_AUTH_MULTI_ROUND_PARSE_H_

#include <string>

#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/http/http_auth.h"

namespace net {

class HttpAuthChallengeTokenizer;

NET_EXPORT_PRIVATE HttpAuth::AuthorizationResult ParseFirstRoundChallenge(
    HttpAuth::Scheme scheme,
    HttpAuthChallengeTokenizer* challenge);

NET_EXPORT_PRIVATE HttpAuth::AuthorizationResult ParseLaterRoundChallenge(
    HttpAuth::Scheme scheme,
    HttpAuthChallengeTokenizer* challenge,
    std::string* encoded_token,
    std::string* decoded_token);

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_MULTI_ROUND_PARSE_H_

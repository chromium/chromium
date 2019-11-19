// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_ntlm.h"

#include <utility>

#include "net/base/url_util.h"
#include "net/cert/x509_util.h"
#include "net/http/http_auth_scheme.h"
#include "net/ssl/ssl_info.h"

namespace net {

HttpAuthHandlerNTLM::Factory::Factory() = default;

HttpAuthHandlerNTLM::Factory::~Factory() = default;

bool HttpAuthHandlerNTLM::Init(HttpAuthChallengeTokenizer* tok,
                               const SSLInfo& ssl_info) {
  auth_scheme_ = HttpAuth::AUTH_SCHEME_NTLM;
  score_ = 3;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;

  if (ssl_info.is_valid())
    x509_util::GetTLSServerEndPointChannelBinding(*ssl_info.cert,
                                                  &channel_bindings_);

  return ParseChallenge(tok, true) == HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
}

HttpAuth::AuthorizationResult HttpAuthHandlerNTLM::HandleAnotherChallengeImpl(
    HttpAuthChallengeTokenizer* challenge) {
  return ParseChallenge(challenge, false);
}

// static
std::string HttpAuthHandlerNTLM::CreateSPN(const GURL& origin) {
  // The service principal name of the destination server.  See
  // http://msdn.microsoft.com/en-us/library/ms677949%28VS.85%29.aspx
  std::string target("HTTP/");
  target.append(GetHostAndOptionalPort(origin));
  return target;
}

}  // namespace net

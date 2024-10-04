// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_auth.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_values.h"

namespace net {

namespace {
const char* const kSchemeNames[] = {kBasicAuthScheme,     kDigestAuthScheme,
                                    kNtlmAuthScheme,      kNegotiateAuthScheme,
                                    kSpdyProxyAuthScheme, kMockAuthScheme};
}  // namespace

HttpAuth::Identity::Identity() = default;

// static
void HttpAuth::ChooseBestChallenge(
    HttpAuthHandlerFactory* http_auth_handler_factory,
    const HttpResponseHeaders& response_headers,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key,
    Target target,
    const url::SchemeHostPort& scheme_host_port,
    const std::set<Scheme>& disabled_schemes,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  DCHECK(http_auth_handler_factory);
  DCHECK(handler->get() == nullptr);

  // Choose the challenge whose authentication handler gives the maximum score.
  std::unique_ptr<HttpAuthHandler> best;
  const std::string header_name = GetChallengeHeaderName(target);
  std::string cur_challenge;
  size_t iter = 0;
  while (response_headers.EnumerateHeader(&iter, header_name, &cur_challenge)) {
    std::unique_ptr<HttpAuthHandler> cur;
    int rv = http_auth_handler_factory->CreateAuthHandlerFromString(
        cur_challenge, target, ssl_info, network_anonymization_key,
        scheme_host_port, net_log, host_resolver, &cur);
    if (rv != OK) {
      VLOG(1) << "Unable to create AuthHandler. Status: "
              << ErrorToString(rv) << " Challenge: " << cur_challenge;
      continue;
    }
    if (cur.get() && (!best.get() || best->score() < cur->score()) &&
        (disabled_schemes.find(cur->auth_scheme()) == disabled_schemes.end()))
      best.swap(cur);
  }
  handler->swap(best);
}

// static
HttpAuth::AuthorizationResult HttpAuth::HandleChallengeResponse(
    HttpAuthHandler* handler,
    const HttpResponseHeaders& response_headers,
    Target target,
    const std::set<Scheme>& disabled_schemes,
    std::string* challenge_used) {
  DCHECK(handler);
  DCHECK(challenge_used);

  challenge_used->clear();
  HttpAuth::Scheme current_scheme = handler->auth_scheme();
  if (disabled_schemes.find(current_scheme) != disabled_schemes.end())
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  const char* current_scheme_name = SchemeToString(current_scheme);
  const std::string header_name = GetChallengeHeaderName(target);
  size_t iter = 0;
  std::optional<std::string_view> challenge;
  HttpAuth::AuthorizationResult authorization_result =
      HttpAuth::AUTHORIZATION_RESULT_INVALID;
  while ((challenge = response_headers.EnumerateHeader(&iter, header_name))) {
    HttpAuthChallengeTokenizer challenge_tokens(*challenge);
    if (challenge_tokens.auth_scheme() != current_scheme_name)
      continue;
    authorization_result = handler->HandleAnotherChallenge(&challenge_tokens);
    if (authorization_result != HttpAuth::AUTHORIZATION_RESULT_INVALID) {
      *challenge_used = *challenge;
      return authorization_result;
    }
  }
  // Finding no matches is equivalent to rejection.
  return HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

// static
std::string HttpAuth::GetChallengeHeaderName(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "Proxy-Authenticate";
    case AUTH_SERVER:
      return "WWW-Authenticate";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// static
std::string HttpAuth::GetAuthorizationHeaderName(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return HttpRequestHeaders::kProxyAuthorization;
    case AUTH_SERVER:
      return HttpRequestHeaders::kAuthorization;
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// static
std::string HttpAuth::GetAuthTargetString(Target target) {
  switch (target) {
    case AUTH_PROXY:
      return "proxy";
    case AUTH_SERVER:
      return "server";
    default:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

// static
const char* HttpAuth::SchemeToString(Scheme scheme) {
  static_assert(std::size(kSchemeNames) == AUTH_SCHEME_MAX,
                "http auth scheme names incorrect size");
  if (scheme < AUTH_SCHEME_BASIC || scheme >= AUTH_SCHEME_MAX) {
    NOTREACHED_IN_MIGRATION();
    return "invalid_scheme";
  }
  return kSchemeNames[scheme];
}

// static
HttpAuth::Scheme HttpAuth::StringToScheme(const std::string& str) {
  for (uint8_t i = 0; i < std::size(kSchemeNames); i++) {
    if (str == kSchemeNames[i])
      return static_cast<Scheme>(i);
  }
  NOTREACHED_IN_MIGRATION();
  return AUTH_SCHEME_MAX;
}

// static
const char* HttpAuth::AuthorizationResultToString(
    AuthorizationResult authorization_result) {
  switch (authorization_result) {
    case AUTHORIZATION_RESULT_ACCEPT:
      return "accept";
    case AUTHORIZATION_RESULT_REJECT:
      return "reject";
    case AUTHORIZATION_RESULT_STALE:
      return "stale";
    case AUTHORIZATION_RESULT_INVALID:
      return "invalid";
    case AUTHORIZATION_RESULT_DIFFERENT_REALM:
      return "different_realm";
  }
  NOTREACHED_IN_MIGRATION();
  return "(invalid result)";
}

// static
base::Value::Dict HttpAuth::NetLogAuthorizationResultParams(
    const char* name,
    AuthorizationResult authorization_result) {
  return NetLogParamsWithString(
      name, AuthorizationResultToString(authorization_result));
}

}  // namespace net

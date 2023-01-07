// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth.h"
#include "net/http/http_auth_handler_ntlm.h"

#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_mechanism.h"
#include "url/scheme_host_port.h"

namespace net {

int HttpAuthHandlerNTLM::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::SchemeHostPort& scheme_host_port,
    CreateReason reason,
    int digest_nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  if (reason == CREATE_PREEMPTIVE)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  // NOTE: Default credentials are not supported for the portable implementation
  // of NTLM.
  auto tmp_handler =
      std::make_unique<HttpAuthHandlerNTLM>(http_auth_preferences());
  if (!tmp_handler->InitFromChallenge(challenge, target, ssl_info,
                                      network_anonymization_key,
                                      scheme_host_port, net_log)) {
    return ERR_INVALID_RESPONSE;
  }
  *handler = std::move(tmp_handler);
  return OK;
}

HttpAuthHandlerNTLM::HttpAuthHandlerNTLM(
    const HttpAuthPreferences* http_auth_preferences)
    : mechanism_(http_auth_preferences) {}

bool HttpAuthHandlerNTLM::NeedsIdentity() {
  return mechanism_.NeedsIdentity();
}

bool HttpAuthHandlerNTLM::AllowsDefaultCredentials() {
  // Default credentials are not supported in the portable implementation of
  // NTLM, but are supported in the SSPI implementation.
  return false;
}

int HttpAuthHandlerNTLM::GenerateAuthTokenImpl(
    const AuthCredentials* credentials,
    const HttpRequestInfo* request,
    CompletionOnceCallback callback,
    std::string* auth_token) {
  return mechanism_.GenerateAuthToken(credentials, CreateSPN(scheme_host_port_),
                                      channel_bindings_, auth_token, net_log(),
                                      std::move(callback));
}

HttpAuth::AuthorizationResult HttpAuthHandlerNTLM::ParseChallenge(
    HttpAuthChallengeTokenizer* tok) {
  return mechanism_.ParseChallenge(tok);
}

HttpAuthHandlerNTLM::~HttpAuthHandlerNTLM() = default;

}  // namespace net

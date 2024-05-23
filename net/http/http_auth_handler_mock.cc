// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/http/http_auth_handler_mock.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_request_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

void PrintTo(const HttpAuthHandlerMock::State& state, ::std::ostream* os) {
  switch (state) {
    case HttpAuthHandlerMock::State::WAIT_FOR_INIT:
      *os << "WAIT_FOR_INIT";
      break;
    case HttpAuthHandlerMock::State::WAIT_FOR_CHALLENGE:
      *os << "WAIT_FOR_CHALLENGE";
      break;
    case HttpAuthHandlerMock::State::WAIT_FOR_GENERATE_AUTH_TOKEN:
      *os << "WAIT_FOR_GENERATE_AUTH_TOKEN";
      break;
    case HttpAuthHandlerMock::State::TOKEN_PENDING:
      *os << "TOKEN_PENDING";
      break;
    case HttpAuthHandlerMock::State::DONE:
      *os << "DONE";
      break;
  }
}

HttpAuthHandlerMock::HttpAuthHandlerMock() = default;

HttpAuthHandlerMock::~HttpAuthHandlerMock() = default;

void HttpAuthHandlerMock::SetGenerateExpectation(bool async, int rv) {
  generate_async_ = async;
  generate_rv_ = rv;
}

bool HttpAuthHandlerMock::NeedsIdentity() {
  return first_round_;
}

bool HttpAuthHandlerMock::AllowsDefaultCredentials() {
  return allows_default_credentials_;
}

bool HttpAuthHandlerMock::AllowsExplicitCredentials() {
  return allows_explicit_credentials_;
}

bool HttpAuthHandlerMock::Init(
    HttpAuthChallengeTokenizer* challenge,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key) {
  EXPECT_EQ(State::WAIT_FOR_INIT, state_);
  state_ = State::WAIT_FOR_GENERATE_AUTH_TOKEN;
  auth_scheme_ = HttpAuth::AUTH_SCHEME_MOCK;
  score_ = 1;
  properties_ = connection_based_ ? IS_CONNECTION_BASED : 0;
  return true;
}

int HttpAuthHandlerMock::GenerateAuthTokenImpl(
    const AuthCredentials* credentials,
    const HttpRequestInfo* request,
    CompletionOnceCallback callback,
    std::string* auth_token) {
  EXPECT_EQ(State::WAIT_FOR_GENERATE_AUTH_TOKEN, state_);
  first_round_ = false;
  request_url_ = request->url;
  if (generate_async_) {
    EXPECT_TRUE(callback_.is_null());
    EXPECT_TRUE(auth_token_ == nullptr);
    callback_ = std::move(callback);
    auth_token_ = auth_token;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&HttpAuthHandlerMock::OnGenerateAuthToken,
                                  weak_factory_.GetWeakPtr()));
    state_ = State::TOKEN_PENDING;
    return ERR_IO_PENDING;
  } else {
    if (generate_rv_ == OK) {
      *auth_token = "auth_token";
      state_ = is_connection_based() ? State::WAIT_FOR_CHALLENGE
                                     : State::WAIT_FOR_GENERATE_AUTH_TOKEN;
    } else {
      state_ = State::DONE;
    }
    return generate_rv_;
  }
}

HttpAuth::AuthorizationResult HttpAuthHandlerMock::HandleAnotherChallengeImpl(
    HttpAuthChallengeTokenizer* challenge) {
  EXPECT_THAT(state_, ::testing::AnyOf(State::WAIT_FOR_CHALLENGE,
                                       State::WAIT_FOR_GENERATE_AUTH_TOKEN));
  // If we receive an empty challenge for a connection based scheme, or a second
  // challenge for a non connection based scheme, assume it's a rejection.
  if (!is_connection_based() || challenge->base64_param().empty()) {
    state_ = State::DONE;
    return HttpAuth::AUTHORIZATION_RESULT_REJECT;
  }

  if (challenge->auth_scheme() != "mock") {
    state_ = State::DONE;
    return HttpAuth::AUTHORIZATION_RESULT_INVALID;
  }

  state_ = State::WAIT_FOR_GENERATE_AUTH_TOKEN;
  return HttpAuth::AUTHORIZATION_RESULT_ACCEPT;
}

void HttpAuthHandlerMock::OnGenerateAuthToken() {
  EXPECT_TRUE(generate_async_);
  EXPECT_TRUE(!callback_.is_null());
  EXPECT_EQ(State::TOKEN_PENDING, state_);
  if (generate_rv_ == OK) {
    *auth_token_ = "auth_token";
    state_ = is_connection_based() ? State::WAIT_FOR_CHALLENGE
                                   : State::WAIT_FOR_GENERATE_AUTH_TOKEN;
  } else {
    state_ = State::DONE;
  }
  auth_token_ = nullptr;
  std::move(callback_).Run(generate_rv_);
}

HttpAuthHandlerMock::Factory::Factory() {
  // TODO(cbentzel): Default do_init_from_challenge_ to true.
}

HttpAuthHandlerMock::Factory::~Factory() = default;

void HttpAuthHandlerMock::Factory::AddMockHandler(
    std::unique_ptr<HttpAuthHandler> handler,
    HttpAuth::Target target) {
  handlers_[target].push_back(std::move(handler));
}

int HttpAuthHandlerMock::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const SSLInfo& ssl_info,
    const NetworkAnonymizationKey& network_anonymization_key,
    const url::SchemeHostPort& scheme_host_port,
    CreateReason reason,
    int nonce_count,
    const NetLogWithSource& net_log,
    HostResolver* host_resolver,
    std::unique_ptr<HttpAuthHandler>* handler) {
  if (handlers_[target].empty())
    return ERR_UNEXPECTED;
  std::unique_ptr<HttpAuthHandler> tmp_handler =
      std::move(handlers_[target][0]);
  std::vector<std::unique_ptr<HttpAuthHandler>>& handlers = handlers_[target];
  handlers.erase(handlers.begin());
  if (do_init_from_challenge_ &&
      !tmp_handler->InitFromChallenge(challenge, target, ssl_info,
                                      network_anonymization_key,
                                      scheme_host_port, net_log)) {
    return ERR_INVALID_RESPONSE;
  }
  handler->swap(tmp_handler);
  return OK;
}

}  // namespace net

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_MOCK_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_MOCK_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_auth_handler_factory.h"
#include "url/gurl.h"

namespace net {

// MockAuthHandler is used in tests to reliably trigger edge cases.
class HttpAuthHandlerMock : public HttpAuthHandler {
 public:
  enum class State {
    WAIT_FOR_INIT,
    WAIT_FOR_CHALLENGE,
    WAIT_FOR_GENERATE_AUTH_TOKEN,
    TOKEN_PENDING,
    DONE
  };

  // The Factory class returns handlers in the order they were added via
  // AddMockHandler.
  class Factory : public HttpAuthHandlerFactory {
   public:
    Factory();
    ~Factory() override;

    void AddMockHandler(HttpAuthHandler* handler, HttpAuth::Target target);

    void set_do_init_from_challenge(bool do_init_from_challenge) {
      do_init_from_challenge_ = do_init_from_challenge;
    }

    // HttpAuthHandlerFactory:
    int CreateAuthHandler(HttpAuthChallengeTokenizer* challenge,
                          HttpAuth::Target target,
                          const SSLInfo& ssl_info,
                          const GURL& origin,
                          CreateReason reason,
                          int nonce_count,
                          const NetLogWithSource& net_log,
                          HostResolver* host_resolver,
                          std::unique_ptr<HttpAuthHandler>* handler) override;

   private:
    std::vector<std::unique_ptr<HttpAuthHandler>>
        handlers_[HttpAuth::AUTH_NUM_TARGETS];
    bool do_init_from_challenge_;
  };

  HttpAuthHandlerMock();

  ~HttpAuthHandlerMock() override;


  void SetGenerateExpectation(bool async, int rv);

  void set_connection_based(bool connection_based) {
    connection_based_ = connection_based;
  }

  void set_allows_default_credentials(bool allows_default_credentials) {
    allows_default_credentials_ = allows_default_credentials;
  }

  void set_allows_explicit_credentials(bool allows_explicit_credentials) {
    allows_explicit_credentials_ = allows_explicit_credentials;
  }

  const GURL& request_url() const {
    return request_url_;
  }

  State state() const { return state_; }

 protected:
  // HttpAuthHandler
  bool NeedsIdentity() override;
  bool AllowsDefaultCredentials() override;
  bool AllowsExplicitCredentials() override;
  bool Init(HttpAuthChallengeTokenizer* challenge,
            const SSLInfo& ssl_info) override;
  int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                            const HttpRequestInfo* request,
                            CompletionOnceCallback callback,
                            std::string* auth_token) override;
  HttpAuth::AuthorizationResult HandleAnotherChallengeImpl(
      HttpAuthChallengeTokenizer* challenge) override;

 private:
  void OnGenerateAuthToken();

  State state_;
  CompletionOnceCallback callback_;
  bool generate_async_;
  int generate_rv_;
  std::string* auth_token_;
  bool first_round_;
  bool connection_based_;
  bool allows_default_credentials_;
  bool allows_explicit_credentials_;
  GURL request_url_;
  base::WeakPtrFactory<HttpAuthHandlerMock> weak_factory_{this};
};

void PrintTo(const HttpAuthHandlerMock::State& state, ::std::ostream* os);

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_MOCK_H_

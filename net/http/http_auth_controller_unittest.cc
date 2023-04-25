// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_controller.h"

#include <algorithm>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_mock.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

enum HandlerRunMode {
  RUN_HANDLER_SYNC,
  RUN_HANDLER_ASYNC
};

enum SchemeState {
  SCHEME_IS_DISABLED,
  SCHEME_IS_ENABLED
};

scoped_refptr<HttpResponseHeaders> HeadersFromString(const char* string) {
  return base::MakeRefCounted<HttpResponseHeaders>(
      HttpUtil::AssembleRawHeaders(string));
}

// Runs an HttpAuthController with a single round mock auth handler
// that returns |handler_rv| on token generation.  The handler runs in
// async if |run_mode| is RUN_HANDLER_ASYNC.  Upon completion, the
// return value of the controller is tested against
// |expected_controller_rv|.  |scheme_state| indicates whether the
// auth scheme used should be disabled after this run.
void RunSingleRoundAuthTest(
    HandlerRunMode run_mode,
    int handler_rv,
    int expected_controller_rv,
    SchemeState scheme_state,
    const NetLogWithSource& net_log = NetLogWithSource()) {
  HttpAuthCache dummy_auth_cache(
      false /* key_server_entries_by_network_anonymization_key */);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://example.com");

  scoped_refptr<HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 407\r\n"
      "Proxy-Authenticate: MOCK foo\r\n"
      "\r\n"));

  HttpAuthHandlerMock::Factory auth_handler_factory;
  auto auth_handler = std::make_unique<HttpAuthHandlerMock>();
  auth_handler->SetGenerateExpectation((run_mode == RUN_HANDLER_ASYNC),
                                       handler_rv);
  auth_handler_factory.AddMockHandler(std::move(auth_handler),
                                      HttpAuth::AUTH_PROXY);
  auth_handler_factory.set_do_init_from_challenge(true);
  auto host_resolver = std::make_unique<MockHostResolver>();

  scoped_refptr<HttpAuthController> controller(
      base::MakeRefCounted<HttpAuthController>(
          HttpAuth::AUTH_PROXY, GURL("http://example.com"),
          NetworkAnonymizationKey(), &dummy_auth_cache, &auth_handler_factory,
          host_resolver.get()));
  SSLInfo null_ssl_info;
  ASSERT_EQ(OK, controller->HandleAuthChallenge(headers, null_ssl_info, false,
                                                false, net_log));
  ASSERT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(AuthCredentials());
  EXPECT_TRUE(controller->HaveAuth());

  TestCompletionCallback callback;
  EXPECT_EQ(
      (run_mode == RUN_HANDLER_ASYNC) ? ERR_IO_PENDING : expected_controller_rv,
      controller->MaybeGenerateAuthToken(&request, callback.callback(),
                                         net_log));
  if (run_mode == RUN_HANDLER_ASYNC)
    EXPECT_EQ(expected_controller_rv, callback.WaitForResult());
  EXPECT_EQ((scheme_state == SCHEME_IS_DISABLED),
            controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_MOCK));
}

}  // namespace

// If an HttpAuthHandler returns an error code that indicates a
// permanent error, the HttpAuthController should disable the scheme
// used and retry the request.
TEST(HttpAuthControllerTest, PermanentErrors) {
  base::test::TaskEnvironment task_environment;

  // Run a synchronous handler that returns
  // ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS.  We expect a return value
  // of OK from the controller so we can retry the request.
  RunSingleRoundAuthTest(RUN_HANDLER_SYNC,
                         ERR_UNEXPECTED_SECURITY_LIBRARY_STATUS, OK,
                         SCHEME_IS_DISABLED);

  // Now try an async handler that returns
  // ERR_MISSING_AUTH_CREDENTIALS.  Async and sync handlers invoke
  // different code paths in HttpAuthController when generating
  // tokens. For this particular error the scheme state depends on
  // the AllowsExplicitCredentials of the handler (which equals true for
  // the mock handler). If it's true we expect the same behaviour as
  // for ERR_INVALID_AUTH_CREDENTIALS so we pass SCHEME_IS_ENABLED.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_MISSING_AUTH_CREDENTIALS, OK,
                         SCHEME_IS_ENABLED);

  // If a non-permanent error is returned by the handler, then the
  // controller should report it unchanged.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_UNEXPECTED, ERR_UNEXPECTED,
                         SCHEME_IS_ENABLED);

  // ERR_INVALID_AUTH_CREDENTIALS is special. It's a non-permanet error, but
  // the error isn't propagated, nor is the auth scheme disabled. This allows
  // the scheme to re-attempt the authentication attempt using a different set
  // of credentials.
  RunSingleRoundAuthTest(RUN_HANDLER_ASYNC, ERR_INVALID_AUTH_CREDENTIALS, OK,
                         SCHEME_IS_ENABLED);
}

// Verify that the controller logs appropriate lifetime events.
TEST(HttpAuthControllerTest, Logging) {
  base::test::TaskEnvironment task_environment;
  RecordingNetLogObserver net_log_observer;

  RunSingleRoundAuthTest(RUN_HANDLER_SYNC, OK, OK, SCHEME_IS_ENABLED,
                         NetLogWithSource::Make(NetLogSourceType::NONE));
  auto entries = net_log_observer.GetEntries();

  // There should be at least two events.
  ASSERT_GE(entries.size(), 2u);

  auto begin =
      base::ranges::find_if(entries, [](const NetLogEntry& e) {
        if (e.type != NetLogEventType::AUTH_CONTROLLER ||
            e.phase != NetLogEventPhase::BEGIN)
          return false;

        auto target = GetOptionalStringValueFromParams(e, "target");
        auto url = GetOptionalStringValueFromParams(e, "url");
        if (!target || !url)
          return false;

        EXPECT_EQ("proxy", *target);
        EXPECT_EQ("http://example.com/", *url);
        return true;
      });
  EXPECT_TRUE(begin != entries.end());
  EXPECT_TRUE(std::any_of(++begin, entries.end(), [](const NetLogEntry& e) {
    return e.type == NetLogEventType::AUTH_CONTROLLER &&
           e.phase == NetLogEventPhase::END;
  }));
}

// If an HttpAuthHandler indicates that it doesn't allow explicit
// credentials, don't prompt for credentials.
TEST(HttpAuthControllerTest, NoExplicitCredentialsAllowed) {
  // Modified mock HttpAuthHandler for this test.
  class MockHandler : public HttpAuthHandlerMock {
   public:
    MockHandler(int expected_rv, HttpAuth::Scheme scheme)
        : expected_scheme_(scheme) {
      SetGenerateExpectation(false, expected_rv);
    }

   protected:
    bool Init(
        HttpAuthChallengeTokenizer* challenge,
        const SSLInfo& ssl_info,
        const NetworkAnonymizationKey& network_anonymization_key) override {
      HttpAuthHandlerMock::Init(challenge, ssl_info, network_anonymization_key);
      set_allows_default_credentials(true);
      set_allows_explicit_credentials(false);
      set_connection_based(true);
      // Pretend to be SCHEME_BASIC so we can test failover logic.
      if (challenge->auth_scheme() == "basic") {
        auth_scheme_ = HttpAuth::AUTH_SCHEME_BASIC;
        --score_;  // Reduce score, so we rank below Mock.
        set_allows_explicit_credentials(true);
      }
      EXPECT_EQ(expected_scheme_, auth_scheme_);
      return true;
    }

    int GenerateAuthTokenImpl(const AuthCredentials* credentials,
                              const HttpRequestInfo* request,
                              CompletionOnceCallback callback,
                              std::string* auth_token) override {
      int result = HttpAuthHandlerMock::GenerateAuthTokenImpl(
          credentials, request, std::move(callback), auth_token);
      EXPECT_TRUE(result != OK ||
                  !AllowsExplicitCredentials() ||
                  !credentials->Empty());
      return result;
    }

   private:
    HttpAuth::Scheme expected_scheme_;
  };

  NetLogWithSource dummy_log;
  HttpAuthCache dummy_auth_cache(
      false /* key_server_entries_by_network_anonymization_key */);
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://example.com");

  HttpRequestHeaders request_headers;
  scoped_refptr<HttpResponseHeaders> headers(HeadersFromString(
      "HTTP/1.1 401\r\n"
      "WWW-Authenticate: Mock\r\n"
      "WWW-Authenticate: Basic\r\n"
      "\r\n"));

  HttpAuthHandlerMock::Factory auth_handler_factory;

  // Handlers for the first attempt at authentication.  AUTH_SCHEME_MOCK handler
  // accepts the default identity and successfully constructs a token.
  auth_handler_factory.AddMockHandler(
      std::make_unique<MockHandler>(OK, HttpAuth::AUTH_SCHEME_MOCK),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.AddMockHandler(
      std::make_unique<MockHandler>(ERR_UNEXPECTED,
                                    HttpAuth::AUTH_SCHEME_BASIC),
      HttpAuth::AUTH_SERVER);

  // Handlers for the second attempt.  Neither should be used to generate a
  // token.  Instead the controller should realize that there are no viable
  // identities to use with the AUTH_SCHEME_MOCK handler and fail.
  auth_handler_factory.AddMockHandler(
      std::make_unique<MockHandler>(ERR_UNEXPECTED, HttpAuth::AUTH_SCHEME_MOCK),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.AddMockHandler(
      std::make_unique<MockHandler>(ERR_UNEXPECTED,
                                    HttpAuth::AUTH_SCHEME_BASIC),
      HttpAuth::AUTH_SERVER);

  // Fallback handlers for the second attempt.  The AUTH_SCHEME_MOCK handler
  // should be discarded due to the disabled scheme, and the AUTH_SCHEME_BASIC
  // handler should successfully be used to generate a token.
  auth_handler_factory.AddMockHandler(
      std::make_unique<MockHandler>(ERR_UNEXPECTED, HttpAuth::AUTH_SCHEME_MOCK),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.AddMockHandler(
      std::make_unique<MockHandler>(OK, HttpAuth::AUTH_SCHEME_BASIC),
      HttpAuth::AUTH_SERVER);
  auth_handler_factory.set_do_init_from_challenge(true);

  auto host_resolver = std::make_unique<MockHostResolver>();

  scoped_refptr<HttpAuthController> controller(
      base::MakeRefCounted<HttpAuthController>(
          HttpAuth::AUTH_SERVER, GURL("http://example.com"),
          NetworkAnonymizationKey(), &dummy_auth_cache, &auth_handler_factory,
          host_resolver.get()));
  SSLInfo null_ssl_info;
  ASSERT_EQ(OK, controller->HandleAuthChallenge(headers, null_ssl_info, false,
                                                false, dummy_log));
  ASSERT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(AuthCredentials());
  EXPECT_TRUE(controller->HaveAuth());

  // Should only succeed if we are using the AUTH_SCHEME_MOCK MockHandler.
  EXPECT_EQ(OK, controller->MaybeGenerateAuthToken(
                    &request, CompletionOnceCallback(), dummy_log));
  controller->AddAuthorizationHeader(&request_headers);

  // Once a token is generated, simulate the receipt of a server response
  // indicating that the authentication attempt was rejected.
  ASSERT_EQ(OK, controller->HandleAuthChallenge(headers, null_ssl_info, false,
                                                false, dummy_log));
  ASSERT_TRUE(controller->HaveAuthHandler());
  controller->ResetAuth(AuthCredentials(u"Hello", std::u16string()));
  EXPECT_TRUE(controller->HaveAuth());
  EXPECT_TRUE(controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_MOCK));
  EXPECT_FALSE(controller->IsAuthSchemeDisabled(HttpAuth::AUTH_SCHEME_BASIC));

  // Should only succeed if we are using the AUTH_SCHEME_BASIC MockHandler.
  EXPECT_EQ(OK, controller->MaybeGenerateAuthToken(
                    &request, CompletionOnceCallback(), dummy_log));
}

}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/registration_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/device_bound_sessions/registration_request_param.h"
#include "net/device_bound_sessions/test_support.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/test_net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArg;

constexpr char kBasicValidJson[] =
    R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.example.com",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

constexpr char kSessionIdentifier[] = "session_id";
constexpr char kRedirectPath[] = "/redirect";
constexpr char kChallenge[] = "test_challenge";
const GURL kRegistrationUrl = GURL("https://www.example.test/startsession");
constexpr unexportable_keys::BackgroundTaskPriority kTaskPriority =
    unexportable_keys::BackgroundTaskPriority::kBestEffort;
std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

struct InvokeCallbackArgumentAction {};

class RegistrationTest : public TestWithTaskEnvironment {
 protected:
  RegistrationTest()
      : server_(test_server::EmbeddedTestServer::TYPE_HTTPS),
        context_(CreateTestURLRequestContextBuilder()->Build()),
        unexportable_key_service_(task_manager_) {}

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  RegistrationFetcherParam GetBasicParam(
      std::optional<GURL> url = std::nullopt) {
    if (!url) {
      url = server_.GetURL("/");
    }

    return RegistrationFetcherParam::CreateInstanceForTesting(
        *url, CreateAlgArray(), std::string(kChallenge),
        /*authorization=*/std::nullopt);
  }

  void CreateKeyAndRunCallback(
      base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                              unexportable_keys::UnexportableKeyId>)>
          callback) {
    unexportable_key_service_.GenerateSigningKeySlowlyAsync(
        CreateAlgArray(), kTaskPriority, std::move(callback));
  }

  test_server::EmbeddedTestServer server_;
  std::unique_ptr<URLRequestContext> context_;

  const url::Origin kOrigin = url::Origin::Create(GURL("https://origin/"));
  unexportable_keys::UnexportableKeyTaskManager task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
};

class TestRegistrationCallback {
 public:
  TestRegistrationCallback() = default;

  RegistrationFetcher::RegistrationCompleteCallback callback() {
    return base::BindOnce(&TestRegistrationCallback::OnRegistrationComplete,
                          base::Unretained(this));
  }

  void WaitForCall() {
    if (outcome_.has_value()) {
      return;
    }

    base::RunLoop run_loop;

    waiting_ = true;
    closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const base::expected<SessionParams, SessionError>& outcome() {
    EXPECT_TRUE(outcome_.has_value());
    return *outcome_;
  }

 private:
  void OnRegistrationComplete(
      base::expected<SessionParams, SessionError> params) {
    EXPECT_FALSE(outcome_.has_value());

    outcome_ = std::move(params);

    if (waiting_) {
      waiting_ = false;
      std::move(closure_).Run();
    }
  }

  std::optional<base::expected<SessionParams, SessionError>> outcome_ =
      std::nullopt;

  bool waiting_ = false;
  base::OnceClosure closure_;
};

std::unique_ptr<test_server::HttpResponse> ReturnResponse(
    HttpStatusCode code,
    std::string_view response_text,
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(code);
  response->set_content_type("application/json");
  response->set_content(response_text);
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnUnauthorized(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_UNAUTHORIZED);
  response->AddCustomHeader("Sec-Session-Challenge", R"("challenge")");
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnTextResponse(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_OK);
  response->set_content_type("text/plain");
  response->set_content("some content");
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnInvalidResponse(
    const test_server::HttpRequest& request) {
  return std::make_unique<test_server::RawHttpResponse>(
      "", "Not a valid HTTP response.");
}

class UnauthorizedThenSuccessResponseContainer {
 public:
  explicit UnauthorizedThenSuccessResponseContainer(
      int unauthorize_response_times)
      : run_times(0), error_respose_times(unauthorize_response_times) {}

  std::unique_ptr<test_server::HttpResponse> Return(
      const test_server::HttpRequest& request) {
    if (run_times++ < error_respose_times) {
      return ReturnUnauthorized(request);
    }
    return ReturnResponse(HTTP_OK, kBasicValidJson, request);
  }

 private:
  int run_times;
  int error_respose_times;
};

TEST_F(RegistrationTest, BasicSuccess) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request) {
        auto resp_iter = request.headers.find("Sec-Session-Response");
        EXPECT_TRUE(resp_iter != request.headers.end());
        if (resp_iter != request.headers.end()) {
          EXPECT_TRUE(VerifyEs256Jwt(resp_iter->second));
        }
        return ReturnResponse(HTTP_OK, kBasicValidJson, request);
      }));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, NoScopeJson) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_FALSE(session_params.scope.include_site);
  EXPECT_TRUE(session_params.scope.specifications.empty());
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, NoSessionIdJson) {
  constexpr char kTestingJson[] =
      R"({
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
}

TEST_F(RegistrationTest, SpecificationNotDictJson) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      "type", "domain", "path"
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_TRUE(session_params.scope.specifications.empty());
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, OneMissingPath) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "include",
        "domain": "trusted.example.com"
      },
      {
        "type": "exclude",
        "domain": "new.example.com",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "other_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);

  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kExclude,
                  "new.example.com", "/only_trusted_path")));

  EXPECT_THAT(session_params.credentials,
              ElementsAre(SessionParams::Credential(
                  "other_cookie",
                  "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, OneSpecTypeInvalid) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : [
      {
        "type": "invalid",
        "domain": "trusted.example.com",
        "path": "/only_trusted_path"
      },
      {
        "type": "exclude",
        "domain": "new.example.com",
        "path": "/only_trusted_path"
      }
    ]
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kExclude,
                  "new.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, InvalidTypeSpecList) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "scope": {
    "include_site": true,
    "scope_specification" : "missing"
  },
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_TRUE(session_params.scope.specifications.empty());
}

TEST_F(RegistrationTest, TypeIsNotCookie) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "credentials": [{
    "type": "sync auth",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  }]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
  EXPECT_EQ(out_params.error().type,
            SessionError::ErrorType::kInvalidCredentials);
}

TEST_F(RegistrationTest, TwoTypesCookie_NotCookie) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "credentials": [
    {
      "type": "cookie",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    },
    {
      "type": "sync auth",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    }
  ]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, TwoTypesNotCookie_Cookie) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "credentials": [
    {
      "type": "sync auth",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    },
    {
      "type": "cookie",
      "name": "auth_cookie",
      "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
    }
  ]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, CredEntryWithoutDict) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "credentials": [{
    "type": "cookie",
    "name": "auth_cookie",
    "attributes": "Domain=example.com; Path=/; Secure; SameSite=None"
  },
  "test"]
})";

  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, ReturnTextFile) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnTextResponse));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  ASSERT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kInvalidConfigJson);
}

TEST_F(RegistrationTest, ReturnInvalidJson) {
  std::string invalid_json = "*{}";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, invalid_json));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kInvalidConfigJson);
}

TEST_F(RegistrationTest, ReturnEmptyJson) {
  std::string empty_json = "{}";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, empty_json));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kInvalidSessionId);
}

TEST_F(RegistrationTest, NetworkErrorServerShutdown) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  ASSERT_TRUE(server_.Start());
  GURL url = server_.GetURL("/");
  ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam(url);
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kNetError);
}

TEST_F(RegistrationTest, NetworkErrorInvalidResponse) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnInvalidResponse));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kNetError);
}

TEST_F(RegistrationTest, ServerError407) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnResponse, HTTP_PROXY_AUTHENTICATION_REQUIRED, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kNetError);
}

TEST_F(RegistrationTest, ServerError400) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_BAD_REQUEST, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kPersistentHttpError);
}

TEST_F(RegistrationTest, ServerError500) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnResponse, HTTP_INTERNAL_SERVER_ERROR, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kTransientHttpError);
}

TEST_F(RegistrationTest, ServerErrorReturnOne401ThenSuccess) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  auto* container = new UnauthorizedThenSuccessResponseContainer(1);
  server_.RegisterRequestHandler(
      base::BindRepeating(&UnauthorizedThenSuccessResponseContainer::Return,
                          base::Owned(container)));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

std::unique_ptr<test_server::HttpResponse> ReturnRedirect(
    const std::string& location,
    const test_server::HttpRequest& request) {
  if (request.relative_url != "/") {
    return nullptr;
  }

  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_FOUND);
  response->AddCustomHeader("Location", location);
  response->set_content("Redirected");
  response->set_content_type("text/plain");
  return std::move(response);
}

std::unique_ptr<test_server::HttpResponse> CheckRedirect(
    bool* redirect_followed_out,
    const test_server::HttpRequest& request) {
  if (request.relative_url != kRedirectPath) {
    return nullptr;
  }

  *redirect_followed_out = true;
  return ReturnResponse(HTTP_OK, kBasicValidJson, request);
}

// Should be allowed: https://a.test -> https://a.test/redirect.
TEST_F(RegistrationTest, FollowHttpsToHttpsRedirect) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  bool followed = false;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, kRedirectPath));
  server_.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  // Required to add a certificate for a.test, which is used below.
  server_.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params =
      GetBasicParam(server_.GetURL("a.test", "/"));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(followed);
  EXPECT_TRUE(callback.outcome().has_value());
}

// Should not be allowed: https://a.test -> http://a.test/redirect.
TEST_F(RegistrationTest, DontFollowHttpsToHttpRedirect) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  bool followed = false;
  test_server::EmbeddedTestServer http_server;
  http_server.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  ASSERT_TRUE(http_server.Start());
  // Required to add a certificate for a.test, which is used below.
  server_.SetSSLConfig(EmbeddedTestServer::CERT_TEST_NAMES);
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnRedirect, http_server.GetURL("a.test", kRedirectPath).spec()));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params =
      GetBasicParam(server_.GetURL("a.test", "/"));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(followed);
  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kPersistentHttpError);
}

// Should be allowed: http://localhost -> http://localhost/redirect.
TEST_F(RegistrationTest, FollowLocalhostHttpToHttpRedirect) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  bool followed = false;
  test_server::EmbeddedTestServer http_server;
  http_server.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  http_server.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, kRedirectPath));
  ASSERT_TRUE(http_server.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam(http_server.GetURL("/"));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(followed);
  EXPECT_TRUE(callback.outcome().has_value());
}

// Should be allowed: https://localhost -> http://localhost/redirect.
TEST_F(RegistrationTest, FollowLocalhostHttpsToHttpRedirect) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  bool followed = false;
  test_server::EmbeddedTestServer http_server;
  http_server.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  ASSERT_TRUE(http_server.Start());

  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnRedirect, http_server.GetURL(kRedirectPath).spec()));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(followed);
  EXPECT_TRUE(callback.outcome().has_value());
}

TEST_F(RegistrationTest, FailOnSslErrorExpired) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());

  callback.WaitForCall();
  EXPECT_FALSE(callback.outcome().has_value());
  EXPECT_EQ(callback.outcome().error().type,
            SessionError::ErrorType::kNetError);
}

std::unique_ptr<test_server::HttpResponse> ReturnResponseForRefreshRequest(
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();

  auto resp_iter = request.headers.find("Sec-Session-Response");
  std::string session_response =
      resp_iter != request.headers.end() ? resp_iter->second : "";
  if (session_response.empty()) {
    const auto session_iter = request.headers.find("Sec-Session-Id");
    EXPECT_TRUE(session_iter != request.headers.end() &&
                !session_iter->second.empty());

    response->set_code(HTTP_UNAUTHORIZED);
    response->AddCustomHeader("Sec-Session-Challenge",
                              R"("test_challenge";id="session_id")");
    return response;
  }

  response->set_code(HTTP_OK);
  response->set_content_type("application/json");
  response->set_content(kBasicValidJson);
  return response;
}

std::unique_ptr<test_server::HttpResponse>
Return401ResponseWithInvalidChallenge(const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(HTTP_UNAUTHORIZED);
  response->AddCustomHeader("Sec-Session-Challenge", "");
  return response;
}

TEST_F(RegistrationTest, BasicSuccessForExistingKey) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, kChallenge);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(unexportable_key_service()), context_.get(),
      std::ref(isolation_info), /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));

  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, FetchRegistrationWithCachedChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponseForRefreshRequest));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, kChallenge);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(unexportable_key_service()), context_.get(),
      std::ref(isolation_info), /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));

  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, FetchRegistrationAndChallengeRequired) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponseForRefreshRequest));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, std::nullopt);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(unexportable_key_service()), context_.get(),
      std::ref(isolation_info), /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));

  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
  const SessionParams& session_params = *out_params;
  EXPECT_TRUE(session_params.scope.include_site);
  EXPECT_THAT(session_params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      session_params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest,
       FetchRegistrationAndChallengeRequired_InvalidChallengeParams) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&Return401ResponseWithInvalidChallenge));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), /*session_identifier=*/std::nullopt, kChallenge);
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(unexportable_key_service()), context_.get(),
      std::ref(isolation_info), /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));

  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
  EXPECT_EQ(out_params.error().type,
            SessionError::ErrorType::kInvalidChallenge);
}

TEST_F(RegistrationTest, ContinueFalse) {
  constexpr char kTestingJson[] =
      R"({
  "session_identifier": "session_id",
  "continue": false
})";
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
  const SessionError& error = out_params.error();
  EXPECT_EQ(error.session_id, "session_id");
  EXPECT_EQ(error.site, net::SchemefulSite(server_.base_url()));
}

TEST_F(RegistrationTest, RetriesOnKeyFailure) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_service;

  // We only want to mock the first call to SignSlowlyAsync, so proxy
  // other required calls to `unexportable_key_service()`.
  EXPECT_CALL(mock_service, GetAlgorithm(_))
      .WillRepeatedly(
          Invoke(&unexportable_key_service(),
                 &unexportable_keys::UnexportableKeyService::GetAlgorithm));
  EXPECT_CALL(mock_service, GetSubjectPublicKeyInfo(_))
      .WillRepeatedly(Invoke(
          &unexportable_key_service(),
          &unexportable_keys::UnexportableKeyService::GetSubjectPublicKeyInfo));
  EXPECT_CALL(mock_service, SignSlowlyAsync(_, _, _, _, _))
      .WillOnce(base::test::RunOnceCallback<4>(
          base::unexpected(unexportable_keys::ServiceError::kCryptoApiFailed)))
      .WillOnce(
          Invoke(&unexportable_key_service(),
                 &unexportable_keys::UnexportableKeyService::SignSlowlyAsync));

  TestRegistrationCallback callback;
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, kChallenge);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(mock_service), context_.get(), std::ref(isolation_info),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));
  callback.WaitForCall();
  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_TRUE(out_params.has_value());
}

TEST_F(RegistrationTest, TerminateSessionOnRepeatedFailure_Refresh) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_service;

  EXPECT_CALL(mock_service, GetAlgorithm(_))
      .WillRepeatedly(
          Invoke(&unexportable_key_service(),
                 &unexportable_keys::UnexportableKeyService::GetAlgorithm));
  EXPECT_CALL(mock_service, GetSubjectPublicKeyInfo(_))
      .WillRepeatedly(Invoke(
          &unexportable_key_service(),
          &unexportable_keys::UnexportableKeyService::GetSubjectPublicKeyInfo));
  EXPECT_CALL(mock_service, SignSlowlyAsync(_, _, _, _, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<4>(
          base::unexpected(unexportable_keys::ServiceError::kCryptoApiFailed)));

  TestRegistrationCallback callback;
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, kChallenge);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(mock_service), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));
  callback.WaitForCall();

  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
  EXPECT_EQ(out_params.error().type, SessionError::ErrorType::kSigningError);
}

TEST_F(RegistrationTest, TerminateSessionOnRepeatedFailure_Registration) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  unexportable_keys::MockUnexportableKeyService mock_service;

  EXPECT_CALL(mock_service, GetAlgorithm(_))
      .WillRepeatedly(
          Invoke(&unexportable_key_service(),
                 &unexportable_keys::UnexportableKeyService::GetAlgorithm));
  EXPECT_CALL(mock_service, GetSubjectPublicKeyInfo(_))
      .WillRepeatedly(Invoke(
          &unexportable_key_service(),
          &unexportable_keys::UnexportableKeyService::GetSubjectPublicKeyInfo));
  EXPECT_CALL(mock_service, SignSlowlyAsync(_, _, _, _, _))
      .WillRepeatedly(base::test::RunOnceCallbackRepeatedly<4>(
          base::unexpected(unexportable_keys::ServiceError::kCryptoApiFailed)));

  TestRegistrationCallback callback;
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), /*session_identifier=*/std::nullopt, kChallenge);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(mock_service), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));
  callback.WaitForCall();

  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
  EXPECT_EQ(out_params.error().type, SessionError::ErrorType::kSigningError);
}

TEST_F(RegistrationTest, NetLogRegistrationResultLogged) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(net_log_observer
                .GetEntriesWithType(NetLogEventType::DBSC_REGISTRATION_RESULT)
                .size(),
            1u);
}

TEST_F(RegistrationTest, NetLogRefreshResultLogged) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  RecordingNetLogObserver net_log_observer;
  TestRegistrationCallback callback;
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, kChallenge);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(unexportable_key_service()), context_.get(),
      std::ref(isolation_info), /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));
  callback.WaitForCall();

  EXPECT_EQ(
      net_log_observer.GetEntriesWithType(NetLogEventType::DBSC_REFRESH_RESULT)
          .size(),
      1u);
}

TEST_F(RegistrationTest, TerminateSessionOnRepeatedChallenge) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;

  auto* container = new UnauthorizedThenSuccessResponseContainer(100);
  server_.RegisterRequestHandler(
      base::BindRepeating(&UnauthorizedThenSuccessResponseContainer::Return,
                          base::Owned(container)));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  auto isolation_info = IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  auto request_param = RegistrationRequestParam::CreateForTesting(
      server_.base_url(), kSessionIdentifier, kChallenge);
  CreateKeyAndRunCallback(base::BindOnce(
      &RegistrationFetcher::StartFetchWithExistingKey, std::move(request_param),
      std::ref(unexportable_key_service()), context_.get(),
      std::ref(isolation_info), /*net_log_source=*/std::nullopt,
      /*original_request_initiator=*/std::nullopt, callback.callback()));
  callback.WaitForCall();

  const base::expected<SessionParams, SessionError>& out_params =
      callback.outcome();
  ASSERT_FALSE(out_params.has_value());
  const SessionError& session_error = out_params.error();
  EXPECT_EQ(session_error.type, SessionError::ErrorType::kTooManyChallenges);
  EXPECT_EQ(session_error.site, net::SchemefulSite(server_.base_url()));
  EXPECT_EQ(session_error.session_id, kSessionIdentifier);
}

class RegistrationTokenHelperTest : public testing::Test {
 public:
  RegistrationTokenHelperTest() : unexportable_key_service_(task_manager_) {}

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
  }

  void RunBackgroundTasks() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  unexportable_keys::UnexportableKeyTaskManager task_manager_{
      crypto::UnexportableKeyProvider::Config()};
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_;
};

TEST_F(RegistrationTokenHelperTest, CreateSuccess) {
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider;
  base::test::TestFuture<
      std::optional<RegistrationFetcher::RegistrationTokenResult>>
      future;
  RegistrationFetcher::CreateTokenAsyncForTesting(
      unexportable_key_service(), "test_challenge",
      GURL("https://accounts.example.test.com/Register"),
      /*authorization=*/std::nullopt, future.GetCallback());
  RunBackgroundTasks();
  ASSERT_TRUE(future.Get().has_value());
}

TEST_F(RegistrationTokenHelperTest, CreateFail) {
  crypto::ScopedNullUnexportableKeyProvider scoped_null_key_provider;
  base::test::TestFuture<
      std::optional<RegistrationFetcher::RegistrationTokenResult>>
      future;
  RegistrationFetcher::CreateTokenAsyncForTesting(
      unexportable_key_service(), "test_challenge",
      GURL("https://https://accounts.example.test/Register"),
      /*authorization=*/std::nullopt, future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace

}  // namespace net::device_bound_sessions

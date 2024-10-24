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
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/http/http_status_code.h"
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

using ::testing::ElementsAre;

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

constexpr char kRedirectPath[] = "/redirect";
constexpr char kChallenge[] = "test_challenge";
const GURL kRegistrationUrl = GURL("https://www.example.test/startsession");

std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

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
    if (called_) {
      return;
    }

    base::RunLoop run_loop;

    waiting_ = true;
    closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::optional<RegistrationFetcher::RegistrationCompleteParams> outcome() {
    EXPECT_TRUE(called_);
    return std::move(outcome_);
  }

 private:
  void OnRegistrationComplete(
      std::optional<RegistrationFetcher::RegistrationCompleteParams> params) {
    EXPECT_FALSE(called_);

    called_ = true;
    outcome_ = std::move(params);

    if (waiting_) {
      waiting_ = false;
      std::move(closure_).Run();
    }
  }

  bool called_ = false;
  std::optional<RegistrationFetcher::RegistrationCompleteParams> outcome_ =
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
  UnauthorizedThenSuccessResponseContainer(int unauthorize_response_times)
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
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_TRUE(out_params->params.scope.include_site);
  EXPECT_THAT(out_params->params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      out_params->params.credentials,
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
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_FALSE(out_params->params.scope.include_site);
  EXPECT_TRUE(out_params->params.scope.specifications.empty());
  EXPECT_THAT(
      out_params->params.credentials,
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
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_FALSE(out_params);
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_TRUE(out_params->params.scope.include_site);
  EXPECT_TRUE(out_params->params.scope.specifications.empty());
  EXPECT_THAT(
      out_params->params.credentials,
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_TRUE(out_params->params.scope.include_site);

  EXPECT_THAT(out_params->params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kExclude,
                  "new.example.com", "/only_trusted_path")));

  EXPECT_THAT(out_params->params.credentials,
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_TRUE(out_params->params.scope.include_site);

  EXPECT_THAT(out_params->params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kExclude,
                  "new.example.com", "/only_trusted_path")));

  EXPECT_THAT(
      out_params->params.credentials,
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_TRUE(out_params->params.scope.include_site);
  EXPECT_TRUE(out_params->params.scope.specifications.empty());
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  EXPECT_EQ(callback.outcome(), std::nullopt);
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_THAT(
      out_params->params.credentials,
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_THAT(
      out_params->params.credentials,
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

  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kTestingJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcher::StartCreateTokenAndFetch(
      GetBasicParam(), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_THAT(
      out_params->params.credentials,
      ElementsAre(SessionParams::Credential(
          "auth_cookie", "Domain=example.com; Path=/; Secure; SameSite=None")));
}

TEST_F(RegistrationTest, ReturnTextFile) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnTextResponse));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, ReturnInvalidJson) {
  std::string invalid_json = "*{}";
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, invalid_json));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, ReturnEmptyJson) {
  std::string empty_json = "{}";
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, empty_json));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, NetworkErrorServerShutdown) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  ASSERT_TRUE(server_.Start());
  GURL url = server_.GetURL("/");
  ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam(url);
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, NetworkErrorInvalidResponse) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnInvalidResponse));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, ServerError500) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(base::BindRepeating(
      &ReturnResponse, HTTP_INTERNAL_SERVER_ERROR, kBasicValidJson));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, ServerErrorReturnOne401ThenSuccess) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;

  auto* container = new UnauthorizedThenSuccessResponseContainer(1);
  server_.RegisterRequestHandler(
      base::BindRepeating(&UnauthorizedThenSuccessResponseContainer::Return,
                          base::Owned(container)));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  std::optional<RegistrationFetcher::RegistrationCompleteParams> out_params =
      callback.outcome();
  ASSERT_TRUE(out_params);
  EXPECT_TRUE(out_params->params.scope.include_site);
  EXPECT_THAT(out_params->params.scope.specifications,
              ElementsAre(SessionParams::Scope::Specification(
                  SessionParams::Scope::Specification::Type::kInclude,
                  "trusted.example.com", "/only_trusted_path")));
  EXPECT_THAT(
      out_params->params.credentials,
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

TEST_F(RegistrationTest, FollowHttpsRedirect) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  bool followed = false;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, kRedirectPath));
  server_.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_TRUE(followed);
  EXPECT_NE(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, DontFollowHttpRedirect) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  bool followed = false;
  test_server::EmbeddedTestServer http_server_;
  ASSERT_TRUE(http_server_.Start());
  const GURL target = http_server_.GetURL(kRedirectPath);

  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnRedirect, target.spec()));
  server_.RegisterRequestHandler(
      base::BindRepeating(&CheckRedirect, &followed));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(followed);
  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, FailOnSslErrorExpired) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_OK, kBasicValidJson));
  server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  RegistrationFetcherParam params = GetBasicParam();
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());

  callback.WaitForCall();
  EXPECT_EQ(callback.outcome(), std::nullopt);
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
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
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
  crypto::ScopedNullUnexportableKeyProvider scoped_null_key_provider_;
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

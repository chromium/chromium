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
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

class RegistrationTest : public TestWithTaskEnvironment {
 protected:
  RegistrationTest()
      : server_(test_server::EmbeddedTestServer::TYPE_HTTPS),
        context_(CreateTestURLRequestContextBuilder()->Build()),
        unexportable_key_service_(task_manager_) {}

  unexportable_keys::UnexportableKeyService& unexportable_key_service() {
    return unexportable_key_service_;
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

  std::optional<DeviceBoundSessionParams> outcome() const { return outcome_; }

 private:
  void OnRegistrationComplete(std::optional<DeviceBoundSessionParams> params) {
    EXPECT_FALSE(called_);

    called_ = true;
    outcome_ = params;

    if (waiting_) {
      waiting_ = false;
      std::move(closure_).Run();
    }
  }

  bool called_ = false;
  std::optional<DeviceBoundSessionParams> outcome_ = std::nullopt;

  bool waiting_ = false;
  base::OnceClosure closure_;
};

constexpr std::string_view kChallenge = "test_challenge";
const GURL kRegistrationUrl = GURL("https://www.example.test/startsession");

std::vector<crypto::SignatureVerifier::SignatureAlgorithm> CreateAlgArray() {
  return {crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
          crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
}

std::unique_ptr<test_server::HttpResponse> ReturnResponse(
    HttpStatusCode code,
    const test_server::HttpRequest& request) {
  auto response = std::make_unique<test_server::BasicHttpResponse>();
  response->set_code(code);
  response->set_content("some content");
  response->set_content_type("text/plain");
  return response;
}

std::unique_ptr<test_server::HttpResponse> ReturnInvalidResponse(
    const test_server::HttpRequest& request) {
  return std::make_unique<test_server::RawHttpResponse>(
      "", "Not a valid HTTP response.");
}

TEST_F(RegistrationTest, BasicSuccess) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          server_.GetURL("/"), CreateAlgArray(), std::string(kChallenge));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();
  EXPECT_NE(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, NetworkErrorServerShutdown) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  ASSERT_TRUE(server_.Start());
  GURL url = server_.GetURL("/");
  ASSERT_TRUE(server_.ShutdownAndWaitUntilComplete());

  TestRegistrationCallback callback;
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          url, CreateAlgArray(), std::string(kChallenge));
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
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          server_.GetURL("/"), CreateAlgArray(), std::string(kChallenge));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, ServerError500) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(
      base::BindRepeating(&ReturnResponse, HTTP_INTERNAL_SERVER_ERROR));
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          server_.GetURL("/"), CreateAlgArray(), std::string(kChallenge));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_EQ(callback.outcome(), std::nullopt);
}

const char kRedirectPath[] = "/redirect";

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
  return ReturnResponse(HTTP_OK, request);
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
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          server_.GetURL("/"), CreateAlgArray(), std::string(kChallenge));
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
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          server_.GetURL("/"), CreateAlgArray(), std::string(kChallenge));
  RegistrationFetcher::StartCreateTokenAndFetch(
      std::move(params), unexportable_key_service(), context_.get(),
      IsolationInfo::CreateTransient(), callback.callback());
  callback.WaitForCall();

  EXPECT_FALSE(followed);
  EXPECT_EQ(callback.outcome(), std::nullopt);
}

TEST_F(RegistrationTest, FailOnSslErrorExpired) {
  crypto::ScopedMockUnexportableKeyProvider scoped_mock_key_provider_;
  server_.RegisterRequestHandler(base::BindRepeating(&ReturnResponse, HTTP_OK));
  server_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(server_.Start());

  TestRegistrationCallback callback;
  DeviceBoundSessionRegistrationFetcherParam params =
      DeviceBoundSessionRegistrationFetcherParam::CreateInstanceForTesting(
          server_.GetURL("/"), CreateAlgArray(), std::string(kChallenge));
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
      GURL("https://accounts.example.test.com/Register"), future.GetCallback());
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
      future.GetCallback());
  RunBackgroundTasks();
  EXPECT_FALSE(future.Get().has_value());
}

}  // namespace
}  // namespace net

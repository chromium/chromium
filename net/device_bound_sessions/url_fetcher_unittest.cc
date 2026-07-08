// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/url_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "components/unexportable_keys/fake_unexportable_key_service.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/device_bound_sessions/session_service.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/device_bound_session_mode.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::test::RunOnceCallback;
using testing::_;

namespace net::device_bound_sessions {

namespace {

class URLFetcherTest : public TestWithTaskEnvironment {
 protected:
  URLFetcherTest() : context_(CreateTestURLRequestContextBuilder()->Build()) {
    URLRequestFailedJob::AddUrlHandler();
  }

  ~URLFetcherTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  URLRequestContext* context() { return context_.get(); }

 private:
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(URLFetcherTest, BasicSuccess) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        response->set_code(HTTP_OK);
        response->set_content("test data");
        return response;
      }));
  ASSERT_TRUE(server.Start());

  auto fetcher = std::make_unique<URLFetcher>(
      context(), server.GetURL("/"), std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "test data");
}

TEST_F(URLFetcherTest, AsyncErrorOnRead) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_ASYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(context(), url, std::nullopt,
                                              /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

TEST_F(URLFetcherTest, SyncErrorOnRead) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_SYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(context(), url, std::nullopt,
                                              /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

TEST_F(URLFetcherTest, Non2xxResponse) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        response->set_code(HTTP_NOT_FOUND);
        response->set_content("not found");
        return response;
      }));
  ASSERT_TRUE(server.Start());

  auto fetcher = std::make_unique<URLFetcher>(
      context(), server.GetURL("/"), std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "not found");
}

TEST_F(URLFetcherTest, FollowRedirect) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        if (request.relative_url == "/redirect") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_FOUND);
          response->AddCustomHeader("Location", "/target");
          return response;
        } else if (request.relative_url == "/target") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_OK);
          response->set_content("target data");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(server.Start());

  auto fetcher =
      std::make_unique<URLFetcher>(context(), server.GetURL("/redirect"),
                                   std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "target data");
}

TEST_F(URLFetcherTest, ImmediateErrorInOnResponseStarted) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_SYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(context(), url, std::nullopt,
                                              /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

class URLFetcherDeferralBypassTest : public base::test::WithFeatureOverride,
                                     public URLFetcherTest {
 public:
  URLFetcherDeferralBypassTest()
      : base::test::WithFeatureOverride(
            net::features::
                kDeviceBoundSessionsBypassDeferralsForRefreshRequests) {}
};

TEST_P(URLFetcherDeferralBypassTest, ModeIsCorrectForRefresh) {
  auto fetcher = std::make_unique<URLFetcher>(
      context(), GURL("http://example.com"), std::nullopt, /*is_refresh=*/true);
  net::DeviceBoundSessionMode expected_mode =
      IsParamFeatureEnabled() ? net::DeviceBoundSessionMode::kBypassDeferral
                              : net::DeviceBoundSessionMode::kAllowed;
  EXPECT_EQ(fetcher->request().device_bound_session_mode(), expected_mode);
}

TEST_P(URLFetcherDeferralBypassTest, ModeIsAllowedWhenNotRefresh) {
  auto fetcher =
      std::make_unique<URLFetcher>(context(), GURL("http://example.com"),
                                   std::nullopt, /*is_refresh=*/false);
  EXPECT_EQ(fetcher->request().device_bound_session_mode(),
            net::DeviceBoundSessionMode::kAllowed);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(URLFetcherDeferralBypassTest);

class URLFetcherClientCertTest : public TestWithTaskEnvironment {
 public:
  using ClientCertHandlerCallback =
      base::RepeatingCallback<void(const GURL&,
                                   scoped_refptr<SSLCertRequestInfo>,
                                   SelectClientCertificateCallback)>;

  URLFetcherClientCertTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        builder_(CreateTestURLRequestContextBuilder()) {
    builder_->set_unexportable_key_service(
        std::make_unique<unexportable_keys::FakeUnexportableKeyService>());
  }

  void SetUpServerAndContext(ClientCertHandlerCallback client_cert_handler,
                             bool has_session_service,
                             const EmbeddedTestServer::HandleRequestCallback&
                                 request_handler = base::NullCallback()) {
    builder_->set_has_device_bound_session_service(has_session_service);
    if (!client_cert_handler.is_null()) {
      builder_->set_device_bound_sessions_client_cert_handler(
          std::move(client_cert_handler));
    }
    net::SSLServerConfig ssl_config;
    ssl_config.client_cert_type =
        SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
    test_server_.SetSSLConfig(EmbeddedTestServer::CERT_OK, ssl_config);
    if (!request_handler.is_null()) {
      test_server_.RegisterRequestHandler(request_handler);
    }
    ASSERT_TRUE(test_server_.Start());
    context_ = builder_->Build();
  }

  URLRequestContext* context() { return context_.get(); }
  EmbeddedTestServer& test_server() { return test_server_; }

 private:
  EmbeddedTestServer test_server_;
  std::unique_ptr<URLRequestContextBuilder> builder_;
  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(URLFetcherClientCertTest, CertificateSelectionCancelled) {
  base::MockRepeatingCallback<void(const GURL&,
                                   scoped_refptr<SSLCertRequestInfo>,
                                   SelectClientCertificateCallback)>
      client_cert_handler;

  EXPECT_CALL(client_cert_handler, Run)
      .WillOnce(RunOnceCallback<2>(nullptr, nullptr, /*cancel=*/true));

  ASSERT_NO_FATAL_FAILURE(SetUpServerAndContext(client_cert_handler.Get(),
                                                /*has_session_service=*/true));
  ASSERT_NE(context()->device_bound_session_service(), nullptr);

  auto fetcher = std::make_unique<URLFetcher>(
      context(), test_server().GetURL("/"), std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

TEST_F(URLFetcherClientCertTest, CertificateSelectionWithCert) {
  std::unique_ptr<FakeClientCertIdentity> identity =
      FakeClientCertIdentity::CreateFromCertAndKeyFiles(
          GetTestCertsDirectory(), "client_1.pem", "client_1.pk8");
  ASSERT_TRUE(identity);

  // Take reference to cert and private key so they outlive the callback.
  scoped_refptr<X509Certificate> cert = identity->certificate();
  scoped_refptr<SSLPrivateKey> private_key = identity->ssl_private_key();

  base::MockRepeatingCallback<void(const GURL&,
                                   scoped_refptr<SSLCertRequestInfo>,
                                   SelectClientCertificateCallback)>
      client_cert_handler;

  EXPECT_CALL(client_cert_handler, Run)
      .WillOnce(RunOnceCallback<2>(cert, private_key, /*cancel=*/false));

  auto request_handler =
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        auto response = std::make_unique<test_server::BasicHttpResponse>();
        response->set_code(HTTP_OK);
        response->set_content("secure data");
        return response;
      });

  ASSERT_NO_FATAL_FAILURE(SetUpServerAndContext(client_cert_handler.Get(),
                                                /*has_session_service=*/true,
                                                request_handler));
  ASSERT_NE(context()->device_bound_session_service(), nullptr);

  auto fetcher = std::make_unique<URLFetcher>(
      context(), test_server().GetURL("/"), std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->TakeDataReceived(), "secure data");
}

TEST_F(URLFetcherClientCertTest, CertificateSelectionWithoutCert) {
  base::MockRepeatingCallback<void(const GURL&,
                                   scoped_refptr<SSLCertRequestInfo>,
                                   SelectClientCertificateCallback)>
      client_cert_handler;
  EXPECT_CALL(client_cert_handler, Run)
      .WillOnce(RunOnceCallback<2>(nullptr, nullptr, /*cancel=*/false));

  ASSERT_NO_FATAL_FAILURE(SetUpServerAndContext(client_cert_handler.Get(),
                                                /*has_session_service=*/true));
  ASSERT_NE(context()->device_bound_session_service(), nullptr);

  auto fetcher = std::make_unique<URLFetcher>(
      context(), test_server().GetURL("/"), std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(fetcher->net_error() == ERR_BAD_SSL_CLIENT_AUTH_CERT ||
              fetcher->net_error() == ERR_SOCKET_NOT_CONNECTED);
}

TEST_F(URLFetcherClientCertTest, CertificateSelectionNoSessionService) {
  ASSERT_NO_FATAL_FAILURE(SetUpServerAndContext(base::NullCallback(),
                                                /*has_session_service=*/false));
  ASSERT_EQ(context()->device_bound_session_service(), nullptr);

  auto fetcher = std::make_unique<URLFetcher>(
      context(), test_server().GetURL("/"), std::nullopt, /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

}  // namespace

}  // namespace net::device_bound_sessions

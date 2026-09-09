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
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/device_bound_session_mode.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::test::RunOnceCallback;
using testing::_;

namespace net::device_bound_sessions {

namespace {

class URLFetcherTest : public TestWithTaskEnvironment {
 protected:
  URLFetcherTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        context_(CreateTestURLRequestContextBuilder()->Build()) {
    URLRequestFailedJob::AddUrlHandler();
    URLRequestMockHTTPJob::AddUrlHandlers(GetTestNetDataDirectory());
    URLRequestMockDataJob::AddUrlHandler();
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

  GURL url = server.GetURL("/");
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "test data");
}

TEST_F(URLFetcherTest, AsyncErrorOnRead) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_ASYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
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
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
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

  GURL url = server.GetURL("/");
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false);

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

  GURL url = server.GetURL("/redirect");
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "target data");
}

TEST_F(URLFetcherTest, RedirectSecureSpecCompliantMetadataSynchronization) {
  // Verifies that a spec-compliant 3xx HTTP Redirect traversing within an
  // encrypted/localhost Cryptographic Trust boundary successfully invokes
  // OnReceivedRedirect, synchronizes W3C Fetch Metadata, and safely dispatches
  // the subsequent request segment to the wire.
  EmbeddedTestServer server;
  std::string captured_sec_fetch_site_header;

  server.RegisterRequestHandler(base::BindRepeating(
      [](std::string* captured_header, const test_server::HttpRequest& request)
          -> std::unique_ptr<test_server::HttpResponse> {
        if (request.relative_url == "/redirect-src") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_FOUND);
          response->AddCustomHeader("Location", "/redirect-dest");
          return response;
        } else if (request.relative_url == "/redirect-dest") {
          // Capture the precise header value transmitted from URLFetcher
          // upon executing OnReceivedRedirect.
          auto it = request.headers.find("Sec-Fetch-Site");
          if (it != request.headers.end()) {
            *captured_header = it->second;
          }
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_OK);
          response->set_content("destination payload");
          return response;
        }
        return nullptr;
      },
      &captured_sec_fetch_site_header));

  ASSERT_TRUE(server.Start());

  GURL initial_url = server.GetURL("/redirect-src");
  // Construct URLFetcher with a referring origin explicitly configured to match
  // the EmbeddedTestServer boundary (yielding "same-origin" or "same-site").
  url::Origin referring_origin = url::Origin::Create(initial_url);

  auto fetcher = std::make_unique<URLFetcher>(context(), initial_url,
                                              referring_origin, std::nullopt,
                                              /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  // 1. Assert the complete fetch pipeline resolved successfully.
  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "destination payload");

  // 2. Validate that URLFetcher::OnReceivedRedirect computed and propagated the
  // spec-compliant W3C Fetch-Metadata Sec-Fetch-Site assertion to the wire.
  EXPECT_EQ(captured_sec_fetch_site_header, "same-origin");
}

TEST_F(URLFetcherTest, RedirectInsecureProtocolDowngradeBlocked) {
  // Cryptographically asserts that URLFetcher::OnReceivedRedirect halts and
  // rejects the request state-machine utilizing net::ERR_UNSAFE_REDIRECT if an
  // outbound redirect target specifies an unencrypted, insecure protocol
  // (e.g. non-localhost http://), effectively mitigating plaintext DBSC
  // token/Origin leakage.
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        if (request.relative_url == "/trigger-insecure-downgrade") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_FOUND);
          // Instruct the Fetcher to redirect off-localhost onto an explicitly
          // unencrypted transport URL.
          response->AddCustomHeader("Location",
                                    "http://non-secure.example.org/plaintext");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(server.Start());

  GURL initial_url = server.GetURL("/trigger-insecure-downgrade");
  auto fetcher = std::make_unique<URLFetcher>(
      context(), initial_url, url::Origin::Create(initial_url), std::nullopt,
      /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  // Assert that URLFetcher intercepted the redirect, evaluated the target
  // scheme, identified the cryptographic trust-downgrade, and immediately
  // aborted via net::ERR_UNSAFE_REDIRECT.
  EXPECT_EQ(fetcher->net_error(), ERR_UNSAFE_REDIRECT);
  // Zero byte-data should be accepted or buffered from the adversarial
  // endpoint.
  EXPECT_EQ(fetcher->data_received(), "");
}

TEST_F(URLFetcherTest,
       RedirectInsecureProtocolDowngradeBlocked_DisarmsWatchdog) {
  GURL initial_url = URLRequestMockHTTPJob::GetMockUrl(
      "url_request_unittest/redirect302-to-echo");
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), initial_url, url::Origin::Create(initial_url), std::nullopt,
      /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(fetcher->net_error(), ERR_UNSAFE_REDIRECT);
  EXPECT_EQ(fetcher->data_received(), "");

  // Assert that the watchdog timer was stopped upon rejection and does not
  // fire later.
  FastForwardBy(kTimeout * 2);
  EXPECT_EQ(fetcher->net_error(), ERR_UNSAFE_REDIRECT);
}

TEST_F(URLFetcherTest, ImmediateErrorInOnResponseStarted) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::READ_SYNC, ERR_FAILED);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false);

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_FAILED);
  EXPECT_EQ(fetcher->data_received(), "");
}

TEST_F(URLFetcherTest, Start_WatchdogTimeout_HeadersStalled) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        return std::make_unique<test_server::HungResponse>();
      }));
  ASSERT_TRUE(server.Start());

  GURL url = server.GetURL("/");
  constexpr base::TimeDelta kTimeout = base::Seconds(2);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(kTimeout);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(fetcher->net_error(), ERR_TIMED_OUT);
}

TEST_F(URLFetcherTest, Start_WatchdogTimeout_BodyStalled) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        return std::make_unique<test_server::HungAfterHeadersHttpResponse>();
      }));
  ASSERT_TRUE(server.Start());

  GURL url = server.GetURL("/");
  constexpr base::TimeDelta kTimeout = base::Seconds(2);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(kTimeout);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(fetcher->net_error(), ERR_TIMED_OUT);
}

TEST_F(URLFetcherTest, Start_WatchdogTimeout_AcrossRedirect) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        if (request.relative_url == "/redirect-to-hung") {
          auto response = std::make_unique<test_server::BasicHttpResponse>();
          response->set_code(HTTP_FOUND);
          response->AddCustomHeader("Location", "/hung");
          return response;
        } else if (request.relative_url == "/hung") {
          return std::make_unique<test_server::HungResponse>();
        }
        return nullptr;
      }));
  ASSERT_TRUE(server.Start());

  GURL initial_url = server.GetURL("/redirect-to-hung");
  constexpr base::TimeDelta kTimeout = base::Seconds(2);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), initial_url, url::Origin::Create(initial_url), std::nullopt,
      /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(kTimeout);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(fetcher->net_error(), ERR_TIMED_OUT);
}

TEST_F(URLFetcherTest, Start_SuccessfulFetch_DisarmsWatchdog) {
  GURL url =
      URLRequestMockHTTPJob::GetMockUrl("url_request_unittest/simple.html");
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(fetcher->net_error(), OK);
  EXPECT_EQ(fetcher->data_received(), "hello\n");

  FastForwardBy(kTimeout * 2);
  EXPECT_EQ(fetcher->net_error(), OK);
}

TEST_F(URLFetcherTest, Start_NetworkError_DisarmsWatchdog) {
  GURL url = URLRequestFailedJob::GetMockHttpUrl(ERR_CONNECTION_FAILED);
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(fetcher->net_error(), ERR_CONNECTION_FAILED);

  FastForwardBy(kTimeout * 2);
  EXPECT_EQ(fetcher->net_error(), ERR_CONNECTION_FAILED);
}

TEST_F(URLFetcherTest, Start_CustomTimeout_TimesOutAtConfiguredDuration) {
  EmbeddedTestServer server;
  server.RegisterRequestHandler(
      base::BindRepeating([](const test_server::HttpRequest& request)
                              -> std::unique_ptr<test_server::HttpResponse> {
        return std::make_unique<test_server::HungResponse>();
      }));
  ASSERT_TRUE(server.Start());

  GURL url = server.GetURL("/");
  constexpr base::TimeDelta kShortTimeout = base::Milliseconds(300);
  static_assert(kShortTimeout == (kShortTimeout / 2) * 2,
                "kShortTimeout is not divisible by 2");

  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false, kShortTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(kShortTimeout / 2);
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(kShortTimeout / 2);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(fetcher->net_error(), ERR_TIMED_OUT);
}

TEST_F(URLFetcherTest, Start_ZeroTimeout_WatchdogDisabled) {
  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::START, ERR_IO_PENDING);
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false, /*timeout=*/base::TimeDelta());

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(base::Hours(1));
  EXPECT_FALSE(future.IsReady());
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
  GURL url("http://example.com");
  auto fetcher =
      std::make_unique<URLFetcher>(context(), url, url::Origin::Create(url),
                                   std::nullopt, /*is_refresh=*/true);
  net::DeviceBoundSessionMode expected_mode =
      IsParamFeatureEnabled() ? net::DeviceBoundSessionMode::kBypassDeferral
                              : net::DeviceBoundSessionMode::kAllowed;
  EXPECT_EQ(fetcher->request().device_bound_session_mode(), expected_mode);
}

TEST_P(URLFetcherDeferralBypassTest, ModeIsAllowedWhenNotRefresh) {
  GURL url("http://example.com");
  auto fetcher = std::make_unique<URLFetcher>(
      context(), url, url::Origin::Create(url), std::nullopt,
      /*is_refresh=*/false);
  EXPECT_EQ(fetcher->request().device_bound_session_mode(),
            net::DeviceBoundSessionMode::kAllowed);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(URLFetcherDeferralBypassTest);

// Note: This fixture runs an `EmbeddedTestServer(TYPE_HTTPS)` on a background
// thread and therefore uses the default `TimeSource::SYSTEM_TIME`. Under
// `TimeSource::MOCK_TIME`, `TaskEnvironment` advances virtual time whenever the
// main thread becomes idle waiting for localhost socket I/O from the background
// server thread, causing `SSLConnectJob::kSSLHandshakeTimeout` (30s) to expire
// prematurely before TLS handshakes complete. Client certificate tests
// requiring mock time (such as watchdog timeout tests) use
// `URLRequestMockDataJob` under `URLFetcherTest`.
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

  std::unique_ptr<URLFetcher> CreateDefaultFetcher() {
    GURL url = test_server_.GetURL("/");
    return std::make_unique<URLFetcher>(
        context(), url, url::Origin::Create(url),
        /*net_log_source=*/std::nullopt, /*is_refresh=*/false);
  }

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

  auto fetcher = CreateDefaultFetcher();

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

  auto fetcher = CreateDefaultFetcher();

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

  auto fetcher = CreateDefaultFetcher();

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

  auto fetcher = CreateDefaultFetcher();

  base::RunLoop run_loop;
  fetcher->Start(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(fetcher->net_error(), ERR_SSL_CLIENT_AUTH_CERT_NEEDED);
}

TEST_F(URLFetcherClientCertTest,
       CertSelectionCallbackInvokedAfterFetcherDestroyed) {
  SelectClientCertificateCallback saved_cert_callback;
  base::RunLoop cert_requested_run_loop;
  ClientCertHandlerCallback client_cert_handler = base::BindLambdaForTesting(
      [&](const GURL&, scoped_refptr<SSLCertRequestInfo>,
          SelectClientCertificateCallback cert_callback) {
        saved_cert_callback = std::move(cert_callback);
        cert_requested_run_loop.Quit();
      });

  ASSERT_NO_FATAL_FAILURE(SetUpServerAndContext(client_cert_handler,
                                                /*has_session_service=*/true));
  ASSERT_NE(context()->device_bound_session_service(), nullptr);

  auto fetcher = CreateDefaultFetcher();

  fetcher->Start(base::DoNothing());
  cert_requested_run_loop.Run();
  ASSERT_TRUE(saved_cert_callback);

  // Destroy the fetcher while cert selection is pending.
  fetcher.reset();

  // Invoking the cert selection callback after the fetcher has been destroyed
  // must safely do nothing and not crash.
  std::move(saved_cert_callback).Run(nullptr, nullptr, /*cancel=*/false);
}

// This test verifies that the watchdog timeout fires if client certificate
// selection stalls. It uses `URLRequestMockDataJob` under `URLFetcherTest`
// (which enables `TimeSource::MOCK_TIME`) rather than
// `URLFetcherClientCertTest`'s `EmbeddedTestServer(TYPE_HTTPS)`, because
// `MOCK_TIME` desynchronizes from background socket threads and causes
// premature TLS handshake timeouts. In-memory mocking via
// `URLRequestMockDataJob` allows testing the full 20-second watchdog duration
// deterministically in zero wall-clock time.
TEST_F(URLFetcherTest, Start_WatchdogTimeout_CertSelectionStalled) {
  SelectClientCertificateCallback saved_cert_callback;
  base::RunLoop cert_requested_run_loop;
  auto builder = CreateTestURLRequestContextBuilder();
  builder->set_unexportable_key_service(
      std::make_unique<unexportable_keys::FakeUnexportableKeyService>());
  builder->set_has_device_bound_session_service(true);
  builder->set_device_bound_sessions_client_cert_handler(
      base::BindLambdaForTesting(
          [&](const GURL&, scoped_refptr<SSLCertRequestInfo>,
              SelectClientCertificateCallback cert_callback) {
            saved_cert_callback = std::move(cert_callback);
            cert_requested_run_loop.Quit();
          }));
  auto context = builder->Build();
  ASSERT_NE(context->device_bound_session_service(), nullptr);

  constexpr base::TimeDelta kTimeout = base::Seconds(20);
  GURL url = URLRequestMockDataJob::GetMockUrlForClientCertificateRequest();
  auto fetcher = std::make_unique<URLFetcher>(
      context.get(), url, url::Origin::Create(url),
      /*net_log_source=*/std::nullopt, /*is_refresh=*/false, kTimeout);

  base::test::TestFuture<void> future;
  fetcher->Start(future.GetCallback());
  cert_requested_run_loop.Run();
  ASSERT_TRUE(saved_cert_callback);
  EXPECT_FALSE(future.IsReady());

  FastForwardBy(kTimeout);
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(fetcher->net_error(), ERR_TIMED_OUT);

  // Invoking the cert selection callback after the watchdog timeout has already
  // cancelled the request must not crash or trigger undefined behavior.
  std::move(saved_cert_callback).Run(nullptr, nullptr, /*cancel=*/false);
}

}  // namespace

}  // namespace net::device_bound_sessions

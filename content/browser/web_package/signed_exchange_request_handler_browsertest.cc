// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_cert_verifier_browser_test.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const uint64_t kSignatureHeaderDate = 1520834000;  // 2018-03-12T05:53:20Z
const uint64_t kSignatureHeaderExpires = 1520837600;  // 2018-03-12T06:53:20Z

constexpr char kExpectedSXGEnabledAcceptHeaderForPrefetch[] =
    "application/signed-exchange;v=b2;q=0.9,*/*;q=0.8";

class RedirectObserver : public WebContentsObserver {
 public:
  explicit RedirectObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RedirectObserver() override = default;

  void DidRedirectNavigation(NavigationHandle* handle) override {
    const net::HttpResponseHeaders* response = handle->GetResponseHeaders();
    if (response)
      response_code_ = response->response_code();
  }

  const base::Optional<int>& response_code() const { return response_code_; }

 private:
  base::Optional<int> response_code_;

  DISALLOW_COPY_AND_ASSIGN(RedirectObserver);
};

class AssertNavigationHandleFlagObserver : public WebContentsObserver {
 public:
  explicit AssertNavigationHandleFlagObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~AssertNavigationHandleFlagObserver() override = default;

  void DidFinishNavigation(NavigationHandle* handle) override {
    EXPECT_TRUE(static_cast<NavigationHandleImpl*>(handle)->IsSignedExchangeInnerResponse());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssertNavigationHandleFlagObserver);
};

}  // namespace

class SignedExchangeRequestHandlerBrowserTestBase
    : public CertVerifierBrowserTest {
 public:
  SignedExchangeRequestHandlerBrowserTestBase() {
    // This installs "root_ca_cert.pem" from which our test certificates are
    // created. (Needed for the tests that use real certificate, i.e.
    // RealCertVerifier)
    net::EmbeddedTestServer::RegisterTestCerts();
  }

  void SetUp() override {
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Time::UnixEpoch() +
        base::TimeDelta::FromSeconds(kSignatureHeaderDate));
    feature_list_.InitWithFeatures({features::kSignedHTTPExchange}, {});
    CertVerifierBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    interceptor_.reset();
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Optional<base::Time>());
  }

 protected:
  static scoped_refptr<net::X509Certificate> LoadCertificate(
      const std::string& cert_file) {
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath dir_path;
    base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
    dir_path = dir_path.AppendASCII("sxg");

    return net::CreateCertificateChainFromFile(
        dir_path, cert_file, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  }

  void InstallUrlInterceptor(const GURL& url, const std::string& data_path) {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      if (!interceptor_) {
        interceptor_ = std::make_unique<
            URLLoaderInterceptor>(base::BindRepeating(
            &SignedExchangeRequestHandlerBrowserTestBase::OnInterceptCallback,
            base::Unretained(this)));
      }
      interceptor_data_path_map_[url] = data_path;
    } else {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&InstallMockInterceptors, url, data_path));
    }
  }

  void InstallMockCert() {
    // Make the MockCertVerifier treat the certificate
    // "prime256v1-sha256.public.pem" as valid for "test.example.org".
    scoped_refptr<net::X509Certificate> original_cert =
        LoadCertificate("prime256v1-sha256.public.pem");
    net::CertVerifyResult dummy_result;
    dummy_result.verified_cert = original_cert;
    dummy_result.cert_status = net::OK;
    dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
    dummy_result.ocsp_result.revocation_status =
        net::OCSPRevocationStatus::GOOD;
    mock_cert_verifier()->AddResultForCertAndHost(
        original_cert, "test.example.org", dummy_result, net::OK);
  }

  void TriggerPrefetch(const GURL& url, bool expect_success) {
    const GURL prefetch_html_url = embedded_test_server()->GetURL(
        std::string("/sxg/prefetch.html#") + url.spec());
    base::string16 expected_title =
        base::ASCIIToUTF16(expect_success ? "OK" : "FAIL");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    NavigateToURL(shell(), prefetch_html_url);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  const base::HistogramTester histogram_tester_;

 private:
  static void InstallMockInterceptors(const GURL& url,
                                      const std::string& data_path) {
    base::FilePath root_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        url, net::URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
                 root_path.AppendASCII(data_path)));
  }

  bool OnInterceptCallback(URLLoaderInterceptor::RequestParams* params) {
    const auto it = interceptor_data_path_map_.find(params->url_request.url);
    if (it == interceptor_data_path_map_.end())
      return false;
    URLLoaderInterceptor::WriteResponse(it->second, params->client.get());
    return true;
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<URLLoaderInterceptor> interceptor_;
  std::map<GURL, std::string> interceptor_data_path_map_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeRequestHandlerBrowserTestBase);
};

enum class SignedExchangeRequestHandlerBrowserTestPrefetchParam {
  kPrefetchDisabled,
  kPrefetchEnabled
};

class SignedExchangeRequestHandlerBrowserTest
    : public SignedExchangeRequestHandlerBrowserTestBase,
      public testing::WithParamInterface<
          SignedExchangeRequestHandlerBrowserTestPrefetchParam> {
 public:
  SignedExchangeRequestHandlerBrowserTest() = default;

 protected:
  bool PrefetchIsEnabled() {
    return GetParam() == SignedExchangeRequestHandlerBrowserTestPrefetchParam::
                             kPrefetchEnabled;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SignedExchangeRequestHandlerBrowserTest);
};

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, Simple) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  if (PrefetchIsEnabled())
    TriggerPrefetch(url, true);

  base::string16 title = base::ASCIIToUTF16("https://test.example.org/test/");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  AssertNavigationHandleFlagObserver assert_navigation_handle_flag_observer(
      shell()->web_contents());

  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(303, redirect_observer.response_code());

  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  ASSERT_TRUE(entry->GetSSL().certificate);

  // "test.example.org.public.pem.cbor" is generated from
  // "prime256v1-sha256.public.pem". So the SHA256 of the certificates must
  // match.
  const net::SHA256HashValue fingerprint =
      net::X509Certificate::CalculateFingerprint256(
          entry->GetSSL().certificate->cert_buffer());
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  const net::SHA256HashValue original_fingerprint =
      net::X509Certificate::CalculateFingerprint256(
          original_cert->cert_buffer());
  EXPECT_EQ(original_fingerprint, fingerprint);
  histogram_tester_.ExpectUniqueSample("SignedExchange.LoadResult",
                                       SignedExchangeLoadResult::kSuccess,
                                       PrefetchIsEnabled() ? 2 : 1);
  histogram_tester_.ExpectTotalCount(
      "SignedExchange.Time.CertificateFetch.Success",
      PrefetchIsEnabled() ? 2 : 1);
  if (PrefetchIsEnabled()) {
    histogram_tester_.ExpectUniqueSample("SignedExchange.Prefetch.LoadResult",
                                         SignedExchangeLoadResult::kSuccess, 1);
    histogram_tester_.ExpectUniqueSample(
        "SignedExchange.Prefetch.Recall.30Seconds", true, 1);
    histogram_tester_.ExpectUniqueSample(
        "SignedExchange.Prefetch.Precision.30Seconds", true, 1);
  } else {
    histogram_tester_.ExpectUniqueSample(
        "SignedExchange.Prefetch.Recall.30Seconds", false, 1);
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       InvalidContentType) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/sxg/test.example.org_test_invalid_content_type.sxg");
  if (PrefetchIsEnabled())
    TriggerPrefetch(url, false);

  base::string16 title = base::ASCIIToUTF16("Fallback URL response");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(303, redirect_observer.response_code());
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.LoadResult", SignedExchangeLoadResult::kVersionMismatch,
      PrefetchIsEnabled() ? 2 : 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, Expired) {
  SignedExchangeHandler::SetVerificationTimeForTesting(
      base::Time::UnixEpoch() +
      base::TimeDelta::FromSeconds(kSignatureHeaderExpires + 1));

  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  base::string16 title = base::ASCIIToUTF16("Fallback URL response");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(303, redirect_observer.response_code());
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.LoadResult",
      SignedExchangeLoadResult::kSignatureVerificationError, 1);
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.SignatureVerificationResult",
      SignedExchangeSignatureVerifier::Result::kErrInvalidTimestamp, 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       RedirectBrokenSignedExchanges) {
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  constexpr const char* kBrokenExchanges[] = {
      "/sxg/test.example.org_test_invalid_magic_string.sxg",
      "/sxg/test.example.org_test_invalid_cbor_header.sxg",
  };

  for (const auto* broken_exchange : kBrokenExchanges) {
    SCOPED_TRACE(testing::Message()
                 << "testing broken exchange: " << broken_exchange);

    GURL url = embedded_test_server()->GetURL(broken_exchange);

    if (PrefetchIsEnabled())
      TriggerPrefetch(url, false);

    base::string16 title = base::ASCIIToUTF16("Fallback URL response");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), url);
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }
  histogram_tester_.ExpectTotalCount("SignedExchange.LoadResult",
                                     PrefetchIsEnabled() ? 4 : 2);
  histogram_tester_.ExpectBucketCount(
      "SignedExchange.LoadResult", SignedExchangeLoadResult::kVersionMismatch,
      PrefetchIsEnabled() ? 2 : 1);
  histogram_tester_.ExpectBucketCount(
      "SignedExchange.LoadResult", SignedExchangeLoadResult::kHeaderParseError,
      PrefetchIsEnabled() ? 2 : 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, CertNotFound) {
  InstallUrlInterceptor(GURL("https://cert.example.org/cert.msg"),
                        "content/test/data/sxg/404.msg");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  if (PrefetchIsEnabled())
    TriggerPrefetch(url, false);

  base::string16 title = base::ASCIIToUTF16("Fallback URL response");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.LoadResult", SignedExchangeLoadResult::kCertFetchError,
      PrefetchIsEnabled() ? 2 : 1);
  histogram_tester_.ExpectTotalCount(
      "SignedExchange.Time.CertificateFetch.Failure",
      PrefetchIsEnabled() ? 2 : 1);
}

INSTANTIATE_TEST_CASE_P(
    SignedExchangeRequestHandlerBrowserTest,
    SignedExchangeRequestHandlerBrowserTest,
    testing::Values(
        SignedExchangeRequestHandlerBrowserTestPrefetchParam::kPrefetchDisabled,
        SignedExchangeRequestHandlerBrowserTestPrefetchParam::
            kPrefetchEnabled));

class SignedExchangeRequestHandlerRealCertVerifierBrowserTest
    : public SignedExchangeRequestHandlerBrowserTestBase {
 public:
  SignedExchangeRequestHandlerRealCertVerifierBrowserTest() {
    // Use "real" CertVerifier.
    disable_mock_cert_verifier();
  }
};

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerRealCertVerifierBrowserTest,
                       Basic) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  // "test.example.org_test.sxg" should pass CertVerifier::Verify() and then
  // fail at SignedExchangeHandler::CheckOCSPStatus() because of the dummy OCSP
  // response.
  // TODO(https://crbug.com/815024): Make this test pass the OCSP check. We'll
  // need to either generate an OCSP response on the fly, or override the OCSP
  // verification time.
  base::string16 title = base::ASCIIToUTF16("Fallback URL response");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  // Verify that it failed at the OCSP check step.
  histogram_tester_.ExpectUniqueSample("SignedExchange.LoadResult",
                                       SignedExchangeLoadResult::kOCSPError, 1);
}

enum class SignedExchangeRequestHandlerWithServiceWorkerBrowserTestParam {
  kLegacy,
  kServiceWorkerServicification
};

class SignedExchangeRequestHandlerWithServiceWorkerBrowserTest
    : public SignedExchangeRequestHandlerBrowserTestBase,
      public testing::WithParamInterface<
          SignedExchangeRequestHandlerWithServiceWorkerBrowserTestParam> {
 public:
  SignedExchangeRequestHandlerWithServiceWorkerBrowserTest() = default;
  void SetUp() override {
    if (GetParam() ==
        SignedExchangeRequestHandlerWithServiceWorkerBrowserTestParam ::
            kServiceWorkerServicification) {
      feature_list_.InitWithFeatures(
          {blink::features::kServiceWorkerServicification}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {blink::features::kServiceWorkerServicification});
    }
    SignedExchangeRequestHandlerBrowserTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(
      SignedExchangeRequestHandlerWithServiceWorkerBrowserTest);
};

INSTANTIATE_TEST_CASE_P(
    SignedExchangeRequestHandlerWithServiceWorkerBrowserTest,
    SignedExchangeRequestHandlerWithServiceWorkerBrowserTest,
    testing::Values(
        SignedExchangeRequestHandlerWithServiceWorkerBrowserTestParam::kLegacy,
        SignedExchangeRequestHandlerWithServiceWorkerBrowserTestParam::
            kServiceWorkerServicification));

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerWithServiceWorkerBrowserTest,
                       Simple) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallMockCert();

  const GURL install_sw_url =
      GURL("https://test.example.org/test/publisher-service-worker.html");

  InstallUrlInterceptor(install_sw_url,
                        "content/test/data/sxg/publisher-service-worker.html");
  InstallUrlInterceptor(
      GURL("https://test.example.org/test/publisher-service-worker.js"),
      "content/test/data/sxg/publisher-service-worker.js");
  {
    base::string16 title = base::ASCIIToUTF16("Done");
    TitleWatcher title_watcher(shell()->web_contents(), title);
    NavigateToURL(shell(), install_sw_url);
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  base::string16 title = base::ASCIIToUTF16("https://test.example.org/test/");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16("Generated"));
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

struct SignedExchangeAcceptHeaderBrowserTestParam {
  SignedExchangeAcceptHeaderBrowserTestParam(
      bool sxg_enabled,
      bool sxg_origin_trial_enabled,
      bool sxg_accept_header_enabled,
      bool service_worker_servicification_enabled)
      : sxg_enabled(sxg_enabled),
        sxg_origin_trial_enabled(sxg_origin_trial_enabled),
        sxg_accept_header_enabled(sxg_accept_header_enabled),
        service_worker_servicification_enabled(
            service_worker_servicification_enabled) {}
  const bool sxg_enabled;
  const bool sxg_origin_trial_enabled;
  const bool sxg_accept_header_enabled;
  const bool service_worker_servicification_enabled;
};

class SignedExchangeAcceptHeaderBrowserTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<
          SignedExchangeAcceptHeaderBrowserTestParam> {
 public:
  using self = SignedExchangeAcceptHeaderBrowserTest;
  SignedExchangeAcceptHeaderBrowserTest()
      : enabled_https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        disabled_https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SignedExchangeAcceptHeaderBrowserTest() override = default;

 protected:
  void SetUp() override {
    std::vector<base::Feature> enable_features;
    std::vector<base::Feature> disable_features;
    if (GetParam().sxg_enabled) {
      enable_features.push_back(features::kSignedHTTPExchange);
    } else {
      disable_features.push_back(features::kSignedHTTPExchange);
    }
    if (GetParam().sxg_origin_trial_enabled) {
      enable_features.push_back(features::kSignedHTTPExchangeOriginTrial);
    } else {
      disable_features.push_back(features::kSignedHTTPExchangeOriginTrial);
    }
    if (GetParam().service_worker_servicification_enabled) {
      enable_features.push_back(blink::features::kServiceWorkerServicification);
    } else {
      disable_features.push_back(
          blink::features::kServiceWorkerServicification);
    }
    feature_list_.InitWithFeatures(enable_features, disable_features);

    enabled_https_server_.ServeFilesFromSourceDirectory("content/test/data");
    enabled_https_server_.RegisterRequestHandler(
        base::BindRepeating(&self::RedirectResponseHandler));
    enabled_https_server_.RegisterRequestMonitor(
        base::BindRepeating(&self::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(enabled_https_server_.Start());

    disabled_https_server_.ServeFilesFromSourceDirectory("content/test/data");
    disabled_https_server_.RegisterRequestHandler(
        base::BindRepeating(&self::RedirectResponseHandler));
    disabled_https_server_.RegisterRequestMonitor(
        base::BindRepeating(&self::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(disabled_https_server_.Start());

    if (GetParam().sxg_accept_header_enabled) {
      std::map<std::string, std::string> feature_parameters;
      feature_parameters["OriginsList"] =
          base::StringPrintf("127.0.0.1:%u", enabled_https_server_.port());
      feature_list_for_accept_header_.InitAndEnableFeatureWithParameters(
          features::kSignedHTTPExchangeAcceptHeader, feature_parameters);
    }
    ContentBrowserTest::SetUp();
  }

  void NavigateAndWaitForTitle(const GURL& url, const std::string title) {
    base::string16 expected_title = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    NavigateToURL(shell(), url);
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  bool ShouldHaveSXGAcceptHeaderInEnabledOrigin() const {
    return GetParam().sxg_enabled || (GetParam().sxg_origin_trial_enabled &&
                                      GetParam().sxg_accept_header_enabled);
  }

  bool ShouldHaveSXGAcceptHeaderInDisabledOrigin() const {
    return GetParam().sxg_enabled;
  }

  void CheckAcceptHeader(const GURL& url, bool is_navigation) {
    const bool is_enabled_origin =
        url.IntPort() == enabled_https_server_.port();
    const bool should_have_sxg =
        is_enabled_origin ? ShouldHaveSXGAcceptHeaderInEnabledOrigin()
                          : ShouldHaveSXGAcceptHeaderInDisabledOrigin();
    const auto accept_header = GetInterceptedAcceptHeader(url);
    ASSERT_TRUE(accept_header);
    EXPECT_EQ(
        *accept_header,
        should_have_sxg
            ? (is_navigation
                   ? std::string(network::kFrameAcceptHeader) +
                         std::string(kAcceptHeaderSignedExchangeSuffix)
                   : std::string(kExpectedSXGEnabledAcceptHeaderForPrefetch))
            : (is_navigation ? std::string(network::kFrameAcceptHeader)
                             : std::string(network::kDefaultAcceptHeader)));
  }

  void CheckNavigationAcceptHeader(const std::vector<GURL>& urls) {
    for (const auto& url : urls) {
      SCOPED_TRACE(url);
      CheckAcceptHeader(url, true /* is_navigation */);
    }
  }

  void CheckPrefetchAcceptHeader(const std::vector<GURL>& urls) {
    for (const auto& url : urls) {
      SCOPED_TRACE(url);
      CheckAcceptHeader(url, false /* is_navigation */);
    }
  }

  base::Optional<std::string> GetInterceptedAcceptHeader(
      const GURL& url) const {
    const auto it = url_accept_header_map_.find(url);
    if (it == url_accept_header_map_.end())
      return base::nullopt;
    return it->second;
  }

  void ClearInterceptedAcceptHeaders() { url_accept_header_map_.clear(); }

  net::EmbeddedTestServer enabled_https_server_;
  net::EmbeddedTestServer disabled_https_server_;

 private:
  static std::unique_ptr<net::test_server::HttpResponse>
  RedirectResponseHandler(const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/r?",
                          base::CompareCase::SENSITIVE)) {
      return std::unique_ptr<net::test_server::HttpResponse>();
    }
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", request.relative_url.substr(3));
    http_response->AddCustomHeader("Cache-Control", "no-cache");
    return std::move(http_response);
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    const auto it = request.headers.find(std::string(network::kAcceptHeader));
    if (it == request.headers.end())
      return;
    url_accept_header_map_[request.base_url.Resolve(request.relative_url)] =
        it->second;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_accept_header_;

  std::map<GURL, std::string> url_accept_header_map_;
};

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest, EnabledOrigin) {
  const GURL enabled_test_url = enabled_https_server_.GetURL("/sxg/test.html");
  EXPECT_EQ(ShouldHaveSXGAcceptHeaderInEnabledOrigin(),
            signed_exchange_utils::ShouldAdvertiseAcceptHeader(
                url::Origin::Create(enabled_test_url)));
  NavigateAndWaitForTitle(enabled_test_url, enabled_test_url.spec());
  CheckNavigationAcceptHeader({enabled_test_url});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest, DisabledOrigin) {
  const GURL disabled_test_url =
      disabled_https_server_.GetURL("/sxg/test.html");
  EXPECT_EQ(GetParam().sxg_enabled,
            signed_exchange_utils::ShouldAdvertiseAcceptHeader(
                url::Origin::Create(disabled_test_url)));

  NavigateAndWaitForTitle(disabled_test_url, disabled_test_url.spec());
  CheckNavigationAcceptHeader({disabled_test_url});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       RedirectEnabledToDisabledToEnabled) {
  const GURL enabled_test_url = enabled_https_server_.GetURL("/sxg/test.html");
  const GURL redirect_disabled_to_enabled_url =
      disabled_https_server_.GetURL("/r?" + enabled_test_url.spec());
  const GURL redirect_enabled_to_disabled_to_enabled_url =
      enabled_https_server_.GetURL("/r?" +
                                   redirect_disabled_to_enabled_url.spec());
  NavigateAndWaitForTitle(redirect_enabled_to_disabled_to_enabled_url,
                          enabled_test_url.spec());

  CheckNavigationAcceptHeader({redirect_enabled_to_disabled_to_enabled_url,
                               redirect_disabled_to_enabled_url,
                               enabled_test_url});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       RedirectDisabledToEnabledToDisabled) {
  const GURL disabled_test_url =
      disabled_https_server_.GetURL("/sxg/test.html");
  const GURL redirect_enabled_to_disabled_url =
      enabled_https_server_.GetURL("/r?" + disabled_test_url.spec());
  const GURL redirect_disabled_to_enabled_to_disabled_url =
      disabled_https_server_.GetURL("/r?" +
                                    redirect_enabled_to_disabled_url.spec());
  NavigateAndWaitForTitle(redirect_disabled_to_enabled_to_disabled_url,
                          disabled_test_url.spec());

  CheckNavigationAcceptHeader({redirect_disabled_to_enabled_to_disabled_url,
                               redirect_enabled_to_disabled_url,
                               disabled_test_url});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       PrefetchEnabledPageEnabledTarget) {
  const GURL enabled_target = enabled_https_server_.GetURL("/sxg/hello.txt");
  const GURL enabled_page_url = enabled_https_server_.GetURL(
      std::string("/sxg/prefetch.html#") + enabled_target.spec());
  NavigateAndWaitForTitle(enabled_page_url, "OK");
  CheckPrefetchAcceptHeader({enabled_target});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       PrefetchEnabledPageDisabledTarget) {
  const GURL disabled_target = disabled_https_server_.GetURL("/sxg/hello.txt");
  const GURL enabled_page_url = enabled_https_server_.GetURL(
      std::string("/sxg/prefetch.html#") + disabled_target.spec());
  NavigateAndWaitForTitle(enabled_page_url, "OK");
  CheckPrefetchAcceptHeader({disabled_target});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       PrefetchDisabledPageEnabledTarget) {
  const GURL enabled_target = enabled_https_server_.GetURL("/sxg/hello.txt");
  const GURL disabled_page_url = disabled_https_server_.GetURL(
      std::string("/sxg/prefetch.html#") + enabled_target.spec());
  NavigateAndWaitForTitle(disabled_page_url, "OK");
  CheckPrefetchAcceptHeader({enabled_target});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       PrefetchDisabledPageDisabledTarget) {
  const GURL disabled_target = disabled_https_server_.GetURL("/sxg/hello.txt");
  const GURL disabled_page_url = disabled_https_server_.GetURL(
      std::string("/sxg/prefetch.html#") + disabled_target.spec());
  NavigateAndWaitForTitle(disabled_page_url, "OK");
  CheckPrefetchAcceptHeader({disabled_target});
}

IN_PROC_BROWSER_TEST_P(
    SignedExchangeAcceptHeaderBrowserTest,
    PrefetchEnabledPageRedirectFromDisabledToEnabledToDisabledTarget) {
  const GURL disabled_target = disabled_https_server_.GetURL("/sxg/hello.txt");
  const GURL redirect_enabled_to_disabled_url =
      enabled_https_server_.GetURL("/r?" + disabled_target.spec());
  const GURL redirect_disabled_to_enabled_to_disabled_url =
      disabled_https_server_.GetURL("/r?" +
                                    redirect_enabled_to_disabled_url.spec());

  const GURL enabled_page_url = enabled_https_server_.GetURL(
      std::string("/sxg/prefetch.html#") +
      redirect_disabled_to_enabled_to_disabled_url.spec());

  NavigateAndWaitForTitle(enabled_page_url, "OK");

  CheckPrefetchAcceptHeader({redirect_disabled_to_enabled_to_disabled_url,
                             redirect_enabled_to_disabled_url,
                             disabled_target});
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest, ServiceWorker) {
  NavigateAndWaitForTitle(
      enabled_https_server_.GetURL("/sxg/service-worker.html"), "Done");
  NavigateAndWaitForTitle(
      disabled_https_server_.GetURL("/sxg/service-worker.html"), "Done");

  const std::string frame_accept = std::string(network::kFrameAcceptHeader);
  const std::string frame_accept_with_sxg =
      frame_accept + std::string(kAcceptHeaderSignedExchangeSuffix);
  const std::vector<std::string> scopes = {"/sxg/sw-scope-generated/",
                                           "/sxg/sw-scope-navigation-preload/",
                                           "/sxg/sw-scope-no-respond-with/"};
  for (const auto& scope : scopes) {
    SCOPED_TRACE(scope);
    const bool is_generated_scope =
        scope == std::string("/sxg/sw-scope-generated/");
    const GURL enabled_target_url =
        enabled_https_server_.GetURL(scope + "test.html");
    const GURL disabled_target_url =
        disabled_https_server_.GetURL(scope + "test.html");
    const GURL redirect_disabled_to_enabled_target_url =
        disabled_https_server_.GetURL("/r?" + enabled_target_url.spec());
    const GURL redirect_enabled_to_disabled_to_enabled_target_url =
        enabled_https_server_.GetURL(
            "/r?" + redirect_disabled_to_enabled_target_url.spec());
    const GURL redirect_enabled_to_disabled_target_url =
        enabled_https_server_.GetURL("/r?" + disabled_target_url.spec());
    const GURL redirect_disabled_to_enabled_to_disabled_target_url =
        disabled_https_server_.GetURL(
            "/r?" + redirect_enabled_to_disabled_target_url.spec());

    const std::string expected_enabled_title =
        is_generated_scope ? (ShouldHaveSXGAcceptHeaderInEnabledOrigin()
                                  ? frame_accept_with_sxg
                                  : frame_accept)
                           : "Done";
    const std::string expected_disabled_title =
        is_generated_scope ? (ShouldHaveSXGAcceptHeaderInDisabledOrigin()
                                  ? frame_accept_with_sxg
                                  : frame_accept)
                           : "Done";
    const base::Optional<std::string> expected_enabled_target_accept_header =
        is_generated_scope ? base::nullopt
                           : base::Optional<std::string>(
                                 ShouldHaveSXGAcceptHeaderInEnabledOrigin()
                                     ? frame_accept_with_sxg
                                     : frame_accept);
    const base::Optional<std::string> expected_disabled_target_accept_header =
        is_generated_scope ? base::nullopt
                           : base::Optional<std::string>(
                                 ShouldHaveSXGAcceptHeaderInDisabledOrigin()
                                     ? frame_accept_with_sxg
                                     : frame_accept);

    NavigateAndWaitForTitle(enabled_target_url, expected_enabled_title);
    EXPECT_EQ(expected_enabled_target_accept_header,
              GetInterceptedAcceptHeader(enabled_target_url));
    ClearInterceptedAcceptHeaders();

    NavigateAndWaitForTitle(disabled_target_url, expected_disabled_title);
    EXPECT_EQ(expected_disabled_target_accept_header,
              GetInterceptedAcceptHeader(disabled_target_url));
    ClearInterceptedAcceptHeaders();

    NavigateAndWaitForTitle(redirect_disabled_to_enabled_target_url,
                            expected_enabled_title);
    CheckNavigationAcceptHeader({redirect_disabled_to_enabled_target_url});
    EXPECT_EQ(expected_enabled_target_accept_header,
              GetInterceptedAcceptHeader(enabled_target_url));
    ClearInterceptedAcceptHeaders();

    NavigateAndWaitForTitle(redirect_enabled_to_disabled_target_url,
                            expected_disabled_title);
    CheckNavigationAcceptHeader({redirect_enabled_to_disabled_target_url});
    EXPECT_EQ(expected_disabled_target_accept_header,
              GetInterceptedAcceptHeader(disabled_target_url));
    ClearInterceptedAcceptHeaders();

    NavigateAndWaitForTitle(redirect_enabled_to_disabled_to_enabled_target_url,
                            expected_enabled_title);
    CheckNavigationAcceptHeader(
        {redirect_enabled_to_disabled_to_enabled_target_url,
         redirect_disabled_to_enabled_target_url});
    EXPECT_EQ(expected_enabled_target_accept_header,
              GetInterceptedAcceptHeader(enabled_target_url));
    ClearInterceptedAcceptHeaders();

    NavigateAndWaitForTitle(redirect_disabled_to_enabled_to_disabled_target_url,
                            expected_disabled_title);
    CheckNavigationAcceptHeader(
        {redirect_disabled_to_enabled_to_disabled_target_url,
         redirect_enabled_to_disabled_target_url});
    EXPECT_EQ(expected_disabled_target_accept_header,
              GetInterceptedAcceptHeader(disabled_target_url));
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeAcceptHeaderBrowserTest,
                       ServiceWorkerPrefetch) {
  NavigateAndWaitForTitle(
      enabled_https_server_.GetURL("/sxg/service-worker-prefetch.html"),
      "Done");
  NavigateAndWaitForTitle(
      disabled_https_server_.GetURL("/sxg/service-worker-prefetch.html"),
      "Done");
  const std::string scope = "/sxg/sw-prefetch-scope/";
  const GURL enabled_target_url =
      enabled_https_server_.GetURL(scope + "test.html");
  const GURL disabled_target_url =
      disabled_https_server_.GetURL(scope + "test.html");

  const GURL enabled_prefetch_target =
      enabled_https_server_.GetURL(std::string("/sxg/hello.txt"));
  const GURL disabled_prefetch_target =
      disabled_https_server_.GetURL(std::string("/sxg/hello.txt"));
  const std::string load_prefetch_script = base::StringPrintf(
      "(function loadPrefetch(urls) {"
      "  for (let url of urls) {"
      "    let link = document.createElement('link');"
      "    link.rel = 'prefetch';"
      "    link.href = url;"
      "    document.body.appendChild(link);"
      "  }"
      "  function check() {"
      "    const entries = performance.getEntriesByType('resource');"
      "    const url_set = new Set(urls);"
      "    for (let entry of entries) {"
      "      url_set.delete(entry.name);"
      "    }"
      "    if (!url_set.size) {"
      "      window.domAutomationController.send(true);"
      "    } else {"
      "      setTimeout(check, 100);"
      "    }"
      "  }"
      "  check();"
      "})(['%s','%s'])",
      enabled_prefetch_target.spec().c_str(),
      disabled_prefetch_target.spec().c_str());
  bool unused = false;

  NavigateAndWaitForTitle(enabled_target_url, "Done");
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell()->web_contents(),
                                          load_prefetch_script, &unused));
  CheckPrefetchAcceptHeader(
      {enabled_prefetch_target, disabled_prefetch_target});
  ClearInterceptedAcceptHeaders();

  NavigateAndWaitForTitle(disabled_target_url, "Done");
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell()->web_contents(),
                                          load_prefetch_script, &unused));
  CheckPrefetchAcceptHeader(
      {enabled_prefetch_target, disabled_prefetch_target});
}

INSTANTIATE_TEST_CASE_P(
    SignedExchangeAcceptHeaderBrowserTest,
    SignedExchangeAcceptHeaderBrowserTest,
    testing::Values(
        SignedExchangeAcceptHeaderBrowserTestParam(false, false, false, false),
        SignedExchangeAcceptHeaderBrowserTestParam(false, false, false, true),
        SignedExchangeAcceptHeaderBrowserTestParam(false, false, true, false),
        SignedExchangeAcceptHeaderBrowserTestParam(false, false, true, true),
        SignedExchangeAcceptHeaderBrowserTestParam(false, true, false, false),
        SignedExchangeAcceptHeaderBrowserTestParam(false, true, false, true),
        SignedExchangeAcceptHeaderBrowserTestParam(false, true, true, false),
        SignedExchangeAcceptHeaderBrowserTestParam(false, true, true, true),
        SignedExchangeAcceptHeaderBrowserTestParam(true, false, false, false),
        SignedExchangeAcceptHeaderBrowserTestParam(true, false, false, true),
        SignedExchangeAcceptHeaderBrowserTestParam(true, false, true, false),
        SignedExchangeAcceptHeaderBrowserTestParam(true, false, true, true),
        SignedExchangeAcceptHeaderBrowserTestParam(true, true, false, false),
        SignedExchangeAcceptHeaderBrowserTestParam(true, true, false, true),
        SignedExchangeAcceptHeaderBrowserTestParam(true, true, true, false),
        SignedExchangeAcceptHeaderBrowserTestParam(true, true, true, true)));

}  // namespace content

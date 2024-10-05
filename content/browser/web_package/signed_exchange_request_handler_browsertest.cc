// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/loader/prefetch_url_loader_service_context.h"
#include "content/browser/loader/subresource_proxying_url_loader_service.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_cert_verifier_browser_test.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/features.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/transport_security_state.h"
#include "net/http/transport_security_state_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

constexpr char kLoadResultHistogram[] = "SignedExchange.LoadResult2";
constexpr char kPrefetchResultHistogram[] =
    "SignedExchange.Prefetch.LoadResult2";
constexpr char kRedirectLoopHistogram[] = "SignedExchange.FallbackRedirectLoop";

class RedirectObserver : public WebContentsObserver {
 public:
  explicit RedirectObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  RedirectObserver(const RedirectObserver&) = delete;
  RedirectObserver& operator=(const RedirectObserver&) = delete;

  ~RedirectObserver() override = default;

  void DidRedirectNavigation(NavigationHandle* handle) override {
    const net::HttpResponseHeaders* response = handle->GetResponseHeaders();
    if (response)
      response_code_ = response->response_code();
  }

  const std::optional<int>& response_code() const { return response_code_; }

 private:
  std::optional<int> response_code_;
};

class AssertNavigationHandleFlagObserver : public WebContentsObserver {
 public:
  explicit AssertNavigationHandleFlagObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  AssertNavigationHandleFlagObserver(
      const AssertNavigationHandleFlagObserver&) = delete;
  AssertNavigationHandleFlagObserver& operator=(
      const AssertNavigationHandleFlagObserver&) = delete;

  ~AssertNavigationHandleFlagObserver() override = default;

  void DidFinishNavigation(NavigationHandle* handle) override {
    EXPECT_TRUE(handle->IsSignedExchangeInnerResponse());
  }
};

class FinishNavigationObserver : public WebContentsObserver {
 public:
  FinishNavigationObserver(WebContents* contents,
                           base::OnceClosure done_closure)
      : WebContentsObserver(contents), done_closure_(std::move(done_closure)) {}

  FinishNavigationObserver(const FinishNavigationObserver&) = delete;
  FinishNavigationObserver& operator=(const FinishNavigationObserver&) = delete;

  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    error_code_ = navigation_handle->GetNetErrorCode();
    std::move(done_closure_).Run();
  }

  const std::optional<net::Error>& error_code() const { return error_code_; }

 private:
  base::OnceClosure done_closure_;
  std::optional<net::Error> error_code_;
};

class MockContentBrowserClient final
    : public ContentBrowserTestContentBrowserClient {
 public:
  std::string GetAcceptLangs(BrowserContext* context) override {
    return accept_langs_;
  }

  void SetAcceptLangs(const std::string& langs) { accept_langs_ = langs; }

 private:
  std::string accept_langs_ = "en";
};

}  // namespace

class SignedExchangeRequestHandlerBrowserTestBase
    : public CertVerifierBrowserTest {
 public:
  SignedExchangeRequestHandlerBrowserTestBase() {
    // Enable BackForwardCache for now as some tests are flaky when the previous
    // RenderFrameHost doesn't change on navigation (the histograms are not
    // recoded correctly).
    // TODO(crbug.com/40242189): Figure out why and fix.
    feature_list_.InitAndEnableFeature(features::kBackForwardCache);
  }

  SignedExchangeRequestHandlerBrowserTestBase(
      const SignedExchangeRequestHandlerBrowserTestBase&) = delete;
  SignedExchangeRequestHandlerBrowserTestBase& operator=(
      const SignedExchangeRequestHandlerBrowserTestBase&) = delete;

  void SetUp() override {
    sxg_test_helper_.SetUp();
    CertVerifierBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    CertVerifierBrowserTest::SetUpOnMainThread();

    inactive_rfh_deletion_observer_ =
        std::make_unique<InactiveRenderFrameHostDeletionObserver>(
            shell()->web_contents());
    client_ = std::make_unique<MockContentBrowserClient>();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
    client_.reset();
  }

 protected:
  void InstallUrlInterceptor(const GURL& url, const std::string& data_path) {
    sxg_test_helper_.InstallUrlInterceptor(url, data_path);
  }

  void InstallMockCert() {
    sxg_test_helper_.InstallMockCert(mock_cert_verifier());
  }

  void InstallMockCertChainInterceptor() {
    sxg_test_helper_.InstallMockCertChainInterceptor();
  }

  // Make the MockCertVerifier treat the signed exchange's certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org", but
  // issued by a known root.
  void InstallMockCertByKnownRoot() {
    scoped_refptr<net::X509Certificate> original_cert =
        SignedExchangeBrowserTestHelper::LoadCertificate();
    net::CertVerifyResult dummy_result;
    dummy_result.verified_cert = original_cert;
    dummy_result.cert_status = net::OK;
    dummy_result.ocsp_result.response_status = bssl::OCSPVerifyResult::PROVIDED;
    dummy_result.ocsp_result.revocation_status =
        bssl::OCSPRevocationStatus::GOOD;
    dummy_result.is_issued_by_known_root = true;
    mock_cert_verifier()->AddResultForCertAndHost(
        original_cert, "test.example.org", dummy_result, net::OK);
  }

  void SetAcceptLangs(const std::string& langs) {
    client_->SetAcceptLangs(langs);
    StoragePartitionImpl* partition =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition());
    partition->GetSubresourceProxyingURLLoaderService()
        ->prefetch_url_loader_service_context_for_testing()
        .SetAcceptLanguages(langs);
  }

  std::unique_ptr<InactiveRenderFrameHostDeletionObserver>
      inactive_rfh_deletion_observer_;

  const base::HistogramTester histogram_tester_;

  std::unique_ptr<MockContentBrowserClient> client_;

 private:
  base::test::ScopedFeatureList feature_list_;
  SignedExchangeBrowserTestHelper sxg_test_helper_;
};

class SignedExchangeRequestHandlerBrowserTest
    : public testing::WithParamInterface<bool>,
      public SignedExchangeRequestHandlerBrowserTestBase {
 public:
  SignedExchangeRequestHandlerBrowserTest() {
    // TODO(crbug.com/334954143) Fix the AcceptLanguage tests when turning on
    // the ReduceAcceptLanguage feature.
    scoped_feature_list_.InitWithFeatures(
        {}, {network::features::kReduceAcceptLanguage});
    use_prefetch_ = GetParam();
  }

  SignedExchangeRequestHandlerBrowserTest(
      const SignedExchangeRequestHandlerBrowserTest&) = delete;
  SignedExchangeRequestHandlerBrowserTest& operator=(
      const SignedExchangeRequestHandlerBrowserTest&) = delete;

  ~SignedExchangeRequestHandlerBrowserTest() = default;

 protected:
  bool UsePrefetch() const { return use_prefetch_; }

  void MaybeTriggerPrefetchSXG(const GURL& url, bool expect_success) {
    if (!UsePrefetch())
      return;
    const GURL prefetch_html_url = embedded_test_server()->GetURL(
        std::string("/sxg/prefetch.html#") + url.spec());
    std::u16string expected_title =
        base::ASCIIToUTF16(expect_success ? "OK" : "FAIL");
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), prefetch_html_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    if (expect_success)
      WaitUntilSXGIsCached(url);
  }

 private:
  class CacheObserver : public PrefetchedSignedExchangeCache::TestObserver {
   public:
    CacheObserver(const GURL& outer_url, base::OnceClosure quit_closure)
        : outer_url_(outer_url), quit_closure_(std::move(quit_closure)) {}

    CacheObserver(const CacheObserver&) = delete;
    CacheObserver& operator=(const CacheObserver&) = delete;

    ~CacheObserver() override = default;

    void OnStored(PrefetchedSignedExchangeCache* cache,
                  const GURL& outer_url) override {
      if (quit_closure_ && (outer_url_ == outer_url)) {
        std::move(quit_closure_).Run();
      }
    }

   private:
    const GURL outer_url_;
    base::OnceClosure quit_closure_;
  };

  void WaitUntilSXGIsCached(const GURL& url) {
    scoped_refptr<PrefetchedSignedExchangeCache> cache =
        static_cast<RenderFrameHostImpl*>(
            shell()->web_contents()->GetPrimaryMainFrame())
            ->EnsurePrefetchedSignedExchangeCache();

    if (cache->GetExchanges().find(url) != cache->GetExchanges().end())
      return;
    base::RunLoop run_loop;
    auto observer =
        std::make_unique<CacheObserver>(url, run_loop.QuitClosure());
    cache->AddObserverForTesting(observer.get());
    run_loop.Run();
    cache->RemoveObserverForTesting(observer.get());
  }

  bool use_prefetch_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, Simple) {
  InstallMockCert();
  InstallMockCertChainInterceptor();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  MaybeTriggerPrefetchSXG(url, true);

  if (!UsePrefetch()) {
    // Need to be in a page to execute JavaScript to trigger renderer initiated
    // navigation.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
  }

  std::u16string title = u"https://test.example.org/test/";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  AssertNavigationHandleFlagObserver assert_navigation_handle_flag_observer(
      shell()->web_contents());

  // PrefetchedSignedExchangeCache is used only for renderer initiated
  // navigation. So use JavaScript to trigger renderer initiated navigation.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             base::StringPrintf("location.href = '%s';", url.spec().c_str())));
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
      SignedExchangeBrowserTestHelper::LoadCertificate();
  const net::SHA256HashValue original_fingerprint =
      net::X509Certificate::CalculateFingerprint256(
          original_cert->cert_buffer());
  EXPECT_EQ(original_fingerprint, fingerprint);

  // Wait for the previous page's RFH to be deleted (if it changed) so that the
  // histograms will get updated.
  inactive_rfh_deletion_observer_->Wait();
  histogram_tester_.ExpectUniqueSample(kLoadResultHistogram,
                                       SignedExchangeLoadResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(
      "SignedExchange.Time.CertificateFetch.Success", 1);
  if (UsePrefetch()) {
    histogram_tester_.ExpectUniqueSample(kPrefetchResultHistogram,
                                         SignedExchangeLoadResult::kSuccess, 1);
    histogram_tester_.ExpectTotalCount("PrefetchedSignedExchangeCache.Count",
                                       1);
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, VariantMatch) {
  SetAcceptLangs("en-US,fr");
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_fr_variant.sxg");
  MaybeTriggerPrefetchSXG(url, true);

  if (!UsePrefetch()) {
    // Need to be in a page to execute JavaScript to trigger renderer initiated
    // navigation.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
  }

  std::u16string title = u"https://test.example.org/test/";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  // PrefetchedSignedExchangeCache is used only for renderer initiated
  // navigation. So use JavaScript to trigger renderer initiated navigation.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             base::StringPrintf("location.href = '%s';", url.spec().c_str())));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  // Wait for the previous page's RFH to be deleted (if it changed) so that the
  // histograms will get updated.
  inactive_rfh_deletion_observer_->Wait();
  histogram_tester_.ExpectUniqueSample(kLoadResultHistogram,
                                       SignedExchangeLoadResult::kSuccess, 1);
  if (UsePrefetch()) {
    histogram_tester_.ExpectTotalCount("PrefetchedSignedExchangeCache.Count",
                                       1);
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       VariantMismatch) {
  SetAcceptLangs("en-US,ja");
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_fr_variant.sxg");
  MaybeTriggerPrefetchSXG(url, false);

  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  histogram_tester_.ExpectUniqueSample(
      kLoadResultHistogram, SignedExchangeLoadResult::kVariantMismatch,
      UsePrefetch() ? 2 : 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       MissingNosniff) {
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/sxg/test.example.org_test_missing_nosniff.sxg");
  MaybeTriggerPrefetchSXG(url, false);

  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(303, redirect_observer.response_code());
  histogram_tester_.ExpectUniqueSample(
      kLoadResultHistogram, SignedExchangeLoadResult::kSXGServedWithoutNosniff,
      UsePrefetch() ? 2 : 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       InvalidContentType) {
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();
  InstallMockCertChainInterceptor();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/sxg/test.example.org_test_invalid_content_type.sxg");
  MaybeTriggerPrefetchSXG(url, false);

  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(303, redirect_observer.response_code());
  histogram_tester_.ExpectUniqueSample(
      kLoadResultHistogram, SignedExchangeLoadResult::kVersionMismatch,
      UsePrefetch() ? 2 : 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, Expired) {
  signed_exchange_utils::SetVerificationTimeForTesting(
      base::Time::UnixEpoch() +
      base::Seconds(SignedExchangeBrowserTestHelper::kSignatureHeaderExpires +
                    1));

  InstallMockCertChainInterceptor();
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  RedirectObserver redirect_observer(shell()->web_contents());
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  EXPECT_EQ(303, redirect_observer.response_code());
  histogram_tester_.ExpectUniqueSample(
      kLoadResultHistogram,
      SignedExchangeLoadResult::kSignatureVerificationError, 1);
  histogram_tester_.ExpectUniqueSample(
      "SignedExchange.SignatureVerificationResult",
      SignedExchangeSignatureVerifier::Result::kErrExpired, 1);
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

    MaybeTriggerPrefetchSXG(url, false);

    std::u16string title = u"Fallback URL response";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    GURL expected_commit_url = GURL("https://test.example.org/test/");
    EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }
  histogram_tester_.ExpectTotalCount(kLoadResultHistogram,
                                     UsePrefetch() ? 4 : 2);
  histogram_tester_.ExpectBucketCount(
      kLoadResultHistogram, SignedExchangeLoadResult::kVersionMismatch,
      UsePrefetch() ? 2 : 1);
  histogram_tester_.ExpectBucketCount(
      kLoadResultHistogram, SignedExchangeLoadResult::kHeaderParseError,
      UsePrefetch() ? 2 : 1);
}

// TODO(crbug.com/41460883): Fails pretty often on Android.
// TODO(crbug.com/40201215): Fails flakily on all platforms with Synchronous
// HTML Parsing enabled.
IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       DISABLED_BadMICE) {
  InstallMockCertChainInterceptor();
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test_bad_mice.sxg");

  MaybeTriggerPrefetchSXG(url, false);

  const std::u16string title_good = u"Reached End: false";
  const std::u16string title_bad = u"Reached End: true";
  TitleWatcher title_watcher(shell()->web_contents(), title_good);
  title_watcher.AlsoWaitForTitle(title_bad);
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title_good, title_watcher.WaitAndGetTitle());

  histogram_tester_.ExpectTotalCount(kLoadResultHistogram,
                                     UsePrefetch() ? 2 : 1);
  {
    SCOPED_TRACE(testing::Message()
                 << "testing SignedExchangeLoadResult::kMerkleIntegrityError");
    histogram_tester_.ExpectBucketCount(
        kLoadResultHistogram, SignedExchangeLoadResult::kMerkleIntegrityError,
        UsePrefetch() ? 2 : 1);
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, BadMICESmall) {
  InstallMockCertChainInterceptor();
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL(
      "/sxg/test.example.org_test_bad_mice_small.sxg");

  MaybeTriggerPrefetchSXG(url, false);

  // Note: TitleWatcher is not needed. NavigateToURL waits until navigation
  // complete.
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));

  histogram_tester_.ExpectTotalCount(kLoadResultHistogram,
                                     UsePrefetch() ? 2 : 1);
  {
    SCOPED_TRACE(testing::Message()
                 << "testing SignedExchangeLoadResult::kMerkleIntegrityError");
    histogram_tester_.ExpectBucketCount(
        kLoadResultHistogram, SignedExchangeLoadResult::kMerkleIntegrityError,
        UsePrefetch() ? 2 : 1);
  }
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest, CertNotFound) {
  InstallUrlInterceptor(GURL("https://cert.example.org/cert.msg"),
                        "content/test/data/sxg/404.msg");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  MaybeTriggerPrefetchSXG(url, false);

  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  histogram_tester_.ExpectUniqueSample(
      kLoadResultHistogram, SignedExchangeLoadResult::kCertFetchError,
      UsePrefetch() ? 2 : 1);
  histogram_tester_.ExpectTotalCount(
      "SignedExchange.Time.CertificateFetch.Failure", UsePrefetch() ? 2 : 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       VaryCookieSxgUsed) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_vary_cookie.sxg");
  MaybeTriggerPrefetchSXG(url, true);

  if (!UsePrefetch()) {
    // Need to be in a page to execute JavaScript to trigger renderer initiated
    // navigation.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
  }

  std::u16string title = u"https://test.example.org/test/";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  // PrefetchedSignedExchangeCache is used only for renderer initiated
  // navigation. So use JavaScript to trigger renderer initiated navigation.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             base::StringPrintf("location.href = '%s';", url.spec().c_str())));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  histogram_tester_.ExpectUniqueSample(kLoadResultHistogram,
                                       SignedExchangeLoadResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       VaryCookieSxgNotUsed) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");
  InstallMockCert();

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  SetCookie(shell()->web_contents()->GetBrowserContext(),
            GURL("https://test.example.org/test/"), "milk=1");

  GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_vary_cookie.sxg");
  MaybeTriggerPrefetchSXG(url, true);

  if (!UsePrefetch()) {
    // Need to be in a page to execute JavaScript to trigger renderer initiated
    // navigation.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));
  }

  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  // PrefetchedSignedExchangeCache is used only for renderer initiated
  // navigation. So use JavaScript to trigger renderer initiated navigation.
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(),
             base::StringPrintf("location.href = '%s';", url.spec().c_str())));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  if (UsePrefetch()) {
    // kSuccess is recorded for successful prefetch, and
    // kHadCookieForCookielessOnlySXG is recorded by not being used for actual
    // navigation.
    histogram_tester_.ExpectBucketCount(kLoadResultHistogram,
                                        SignedExchangeLoadResult::kSuccess, 1);
    histogram_tester_.ExpectBucketCount(
        kLoadResultHistogram,
        SignedExchangeLoadResult::kHadCookieForCookielessOnlySXG, 1);
    histogram_tester_.ExpectTotalCount(kLoadResultHistogram, 2);
  } else {
    histogram_tester_.ExpectUniqueSample(
        kLoadResultHistogram,
        SignedExchangeLoadResult::kHadCookieForCookielessOnlySXG, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignedExchangeRequestHandlerBrowserTest,
                         ::testing::Bool());

class SignedExchangeRequestHandlerDownloadBrowserTest
    : public SignedExchangeRequestHandlerBrowserTestBase {
 public:
  SignedExchangeRequestHandlerDownloadBrowserTest() = default;
  ~SignedExchangeRequestHandlerDownloadBrowserTest() override = default;

 protected:
  class DownloadObserver : public DownloadManager::Observer {
   public:
    DownloadObserver(DownloadManager* manager) : manager_(manager) {
      manager_->AddObserver(this);
    }
    ~DownloadObserver() override { manager_->RemoveObserver(this); }

    void WaitUntilDownloadCreated() { run_loop_.Run(); }

    const GURL& observed_url() const { return url_; }
    const std::string& observed_content_disposition() const {
      return content_disposition_;
    }

    // content::DownloadManager::Observer implementation.
    void OnDownloadCreated(content::DownloadManager* manager,
                           download::DownloadItem* item) override {
      url_ = item->GetURL();
      content_disposition_ = item->GetContentDisposition();
      run_loop_.Quit();
    }

   private:
    raw_ptr<DownloadManager> manager_;
    base::RunLoop run_loop_;
    GURL url_;
    std::string content_disposition_;
  };

  void SetUpOnMainThread() override {
    SignedExchangeRequestHandlerBrowserTestBase::SetUpOnMainThread();
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

 private:
  base::ScopedTempDir downloads_directory_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerDownloadBrowserTest,
                       Download) {
  std::unique_ptr<DownloadObserver> observer =
      std::make_unique<DownloadObserver>(
          shell()->web_contents()->GetBrowserContext()->GetDownloadManager());

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  const std::string load_sxg =
      "const iframe = document.createElement('iframe');"
      "iframe.src = './sxg/test.example.org_test_download.sxg';"
      "document.body.appendChild(iframe);";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), load_sxg));
  observer->WaitUntilDownloadCreated();
  EXPECT_EQ(
      embedded_test_server()->GetURL("/sxg/test.example.org_test_download.sxg"),
      observer->observed_url());
  EXPECT_EQ("attachment; filename=test.sxg",
            observer->observed_content_disposition());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerDownloadBrowserTest,
                       DownloadInnerResponse) {
  InstallMockCert();
  InstallMockCertChainInterceptor();
  std::unique_ptr<DownloadObserver> observer =
      std::make_unique<DownloadObserver>(
          shell()->web_contents()->GetBrowserContext()->GetDownloadManager());

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  const std::string load_sxg =
      "const iframe = document.createElement('iframe');"
      "iframe.src = './sxg/test.example.org_bad_content_type.sxg';"
      "document.body.appendChild(iframe);";
  // Since the inner response has an invalid Content-Type and MIME sniffing
  // is disabled for Signed Exchange inner response, this should download the
  // inner response of the exchange.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), load_sxg));
  observer->WaitUntilDownloadCreated();
  EXPECT_EQ(GURL("https://test.example.org/test/"), observer->observed_url());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerDownloadBrowserTest,
                       DataURLDownload) {
  const GURL sxg_url = GURL("data:application/signed-exchange,");
  std::unique_ptr<DownloadObserver> observer =
      std::make_unique<DownloadObserver>(
          shell()->web_contents()->GetBrowserContext()->GetDownloadManager());

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/empty.html")));

  const std::string load_sxg = base::StringPrintf(
      "const iframe = document.createElement('iframe');"
      "iframe.src = '%s';"
      "document.body.appendChild(iframe);",
      sxg_url.spec().c_str());
  EXPECT_TRUE(ExecJs(shell()->web_contents(), load_sxg));
  observer->WaitUntilDownloadCreated();
  EXPECT_EQ(sxg_url, observer->observed_url());
}

class SignedExchangeRequestHandlerRealCertVerifierBrowserTest
    : public SignedExchangeRequestHandlerBrowserTestBase {
 public:
  SignedExchangeRequestHandlerRealCertVerifierBrowserTest() {
    // Use "real" CertVerifier.
    disable_mock_cert_verifier();
    // This installs "root_ca_cert.pem" from which our test certificates are
    // created. (Needed for the tests that use real certificate, i.e.
    // RealCertVerifier)
    scoped_test_root_ = net::EmbeddedTestServer::RegisterTestCerts();
  }
  void SetUp() override {
    SignedExchangeHandler::SetShouldIgnoreCertValidityPeriodErrorForTesting(
        true);
    SignedExchangeRequestHandlerBrowserTestBase::SetUp();
  }
  void TearDown() override {
    SignedExchangeRequestHandlerBrowserTestBase::TearDown();
    SignedExchangeHandler::SetShouldIgnoreCertValidityPeriodErrorForTesting(
        false);
  }

 private:
  net::ScopedTestRoot scoped_test_root_;
};

// If this fails with ERR_CERT_DATE_INVALID, try to regenerate test data
// by running generate-test-certs.sh and generate-test-sxgs.sh in
// src/content/test/data/sxg. See https://crbug.com/1279652.
IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerRealCertVerifierBrowserTest,
                       Basic) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org-long-validity.public.pem.cbor");
  InstallUrlInterceptor(GURL("https://test.example.org/test/"),
                        "content/test/data/sxg/fallback.html");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/sxg/test.example.org_long_cert_validity.sxg");

  // This signed exchange should pass CertVerifier::Verify() and then fail at
  // SignedExchangeHandler::CheckOCSPStatus() because of the dummy OCSP
  // response.
  // TODO(crbug.com/40564303): Make this test pass the OCSP check. We'll
  // need to either generate an OCSP response on the fly, or override the OCSP
  // verification time.
  std::u16string title = u"Fallback URL response";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  // Verify that it failed at the OCSP check step.
  histogram_tester_.ExpectUniqueSample(kLoadResultHistogram,
                                       SignedExchangeLoadResult::kOCSPError, 1);
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       LogicalUrlInServiceWorkerScope) {
  // SW-scope: https://test.example.org/test/
  // SXG physical URL: http://127.0.0.1:PORT/sxg/test.example.org_test.sxg
  // SXG logical URL: https://test.example.org/test/
  InstallMockCert();
  InstallMockCertChainInterceptor();

  const GURL install_sw_url =
      GURL("https://test.example.org/test/publisher-service-worker.html");

  InstallUrlInterceptor(install_sw_url,
                        "content/test/data/sxg/publisher-service-worker.html");
  InstallUrlInterceptor(
      GURL("https://test.example.org/test/publisher-service-worker.js"),
      "content/test/data/sxg/publisher-service-worker.js");
  {
    std::u16string title = u"Done";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), install_sw_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  std::u16string title = u"https://test.example.org/test/";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  title_watcher.AlsoWaitForTitle(u"Generated");
  GURL expected_commit_url = GURL("https://test.example.org/test/");
  EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
  // The page content shoud be served from the signed exchange.
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
}

// TODO(crbug.com/40890897): Re-enable this test when de-flaked.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_NotControlledByDistributorsSW \
  DISABLED_NotControlledByDistributorsSW
#else
#define MAYBE_NotControlledByDistributorsSW NotControlledByDistributorsSW
#endif
IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       MAYBE_NotControlledByDistributorsSW) {
  // SW-scope: http://127.0.0.1:PORT/sxg/
  // SXG physical URL: http://127.0.0.1:PORT/sxg/test.example.org_test.sxg
  // SXG logical URL: https://test.example.org/test/
  InstallMockCert();
  InstallMockCertChainInterceptor();
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL install_sw_url = embedded_test_server()->GetURL(
      "/sxg/no-respond-with-service-worker.html");

  {
    std::u16string title = u"Done";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), install_sw_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  std::u16string title = u"https://test.example.org/test/";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg"),
      GURL("https://test.example.org/test/") /* expected_commit_url */));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  // The page must not be controlled by the service worker of the physical URL.
  EXPECT_EQ(false, EvalJs(shell(), "!!navigator.serviceWorker.controller"));
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       NotControlledBySameOriginDistributorsSW) {
  // SW-scope: https://test.example.org/scope/
  // SXG physical URL: https://test.example.org/scope/test.example.org_test.sxg
  // SXG logical URL: https://test.example.org/test/
  InstallMockCert();
  InstallMockCertChainInterceptor();

  InstallUrlInterceptor(GURL("https://test.example.org/scope/test.sxg"),
                        "content/test/data/sxg/test.example.org_test.sxg");

  const GURL install_sw_url = GURL(
      "https://test.example.org/scope/no-respond-with-service-worker.html");

  InstallUrlInterceptor(
      install_sw_url,
      "content/test/data/sxg/no-respond-with-service-worker.html");
  InstallUrlInterceptor(
      GURL("https://test.example.org/scope/no-respond-with-service-worker.js"),
      "content/test/data/sxg/no-respond-with-service-worker.js");

  {
    std::u16string title = u"Done";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), install_sw_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  std::u16string title = u"https://test.example.org/test/";
  TitleWatcher title_watcher(shell()->web_contents(), title);
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://test.example.org/scope/test.sxg"),
      GURL("https://test.example.org/test/") /* expected_commit_url */));
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  // The page must not be controlled by the service worker of the physical URL.
  EXPECT_EQ(false, EvalJs(shell(), "!!navigator.serviceWorker.controller"));
}

IN_PROC_BROWSER_TEST_P(SignedExchangeRequestHandlerBrowserTest,
                       RegisterServiceWorkerFromSignedExchange) {
  // SXG physical URL: http://127.0.0.1:PORT/sxg/test.example.org_test.sxg
  // SXG logical URL: https://test.example.org/test/
  InstallMockCert();
  InstallMockCertChainInterceptor();

  InstallUrlInterceptor(
      GURL("https://test.example.org/test/publisher-service-worker.js"),
      "content/test/data/sxg/publisher-service-worker.js");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  GURL expected_commit_url = GURL("https://test.example.org/test/");

  {
    std::u16string title = u"https://test.example.org/test/";
    TitleWatcher title_watcher(shell()->web_contents(), title);
    EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
    EXPECT_EQ(title, title_watcher.WaitAndGetTitle());
  }

  const std::string register_sw_script =
      "(async function() {"
      "  try {"
      "    const registration = await navigator.serviceWorker.register("
      "        'publisher-service-worker.js', {scope: './'});"
      "    return true;"
      "  } catch (e) {"
      "    return false;"
      "  }"
      "})();";
  // serviceWorker.register() fails because the document URL of
  // ServiceWorkerHost is empty.
  EXPECT_EQ(false, EvalJs(shell()->web_contents(), register_sw_script));
}

class SignedExchangeAcceptHeaderBrowserTest : public ContentBrowserTest {
 public:
  using self = SignedExchangeAcceptHeaderBrowserTest;
  SignedExchangeAcceptHeaderBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~SignedExchangeAcceptHeaderBrowserTest() override = default;

 protected:
  void SetUp() override {
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&self::RedirectResponseHandler));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &self::FallbackSxgResponseHandler, base::Unretained(this)));
    https_server_.RegisterRequestMonitor(
        base::BindRepeating(&self::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());

    ContentBrowserTest::SetUp();
  }

  void NavigateAndWaitForTitle(const GURL& url, const std::string& title) {
    std::u16string expected_title = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void NavigateWithRedirectAndWaitForTitle(const GURL& url,
                                           const GURL& expected_commit_url,
                                           const std::string& title) {
    std::u16string expected_title = base::ASCIIToUTF16(title);
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), url, expected_commit_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  void CheckAcceptHeader(const GURL& url,
                         bool is_navigation,
                         bool is_fallback) {
    const auto accept_header = GetInterceptedAcceptHeader(url);
    ASSERT_TRUE(accept_header);
    EXPECT_EQ(*accept_header,
              !is_fallback
                  ? base::StrCat({kFrameAcceptHeaderValue,
                                  kAcceptHeaderSignedExchangeSuffix})
                  : (is_navigation
                         ? std::string(kFrameAcceptHeaderValue)
                         : std::string(network::kDefaultAcceptHeaderValue)));
  }

  void CheckNavigationAcceptHeader(const std::vector<GURL>& urls) {
    for (const auto& url : urls) {
      SCOPED_TRACE(url);
      CheckAcceptHeader(url, true /* is_navigation */, false /* is_fallback */);
    }
  }

  void CheckPrefetchAcceptHeader(const std::vector<GURL>& urls) {
    for (const auto& url : urls) {
      SCOPED_TRACE(url);
      CheckAcceptHeader(url, false /* is_navigation */,
                        false /* is_fallback */);
    }
  }

  void CheckFallbackAcceptHeader(const std::vector<GURL>& urls) {
    for (const auto& url : urls) {
      SCOPED_TRACE(url);
      CheckAcceptHeader(url, true /* is_navigation */, true /* is_fallback */);
    }
  }

  std::optional<std::string> GetInterceptedAcceptHeader(const GURL& url) const {
    base::AutoLock lock(url_accept_header_map_lock_);
    const auto it = url_accept_header_map_.find(url);
    if (it == url_accept_header_map_.end())
      return std::nullopt;
    return it->second;
  }

  void ClearInterceptedAcceptHeaders() {
    base::AutoLock lock(url_accept_header_map_lock_);
    url_accept_header_map_.clear();
  }

  net::EmbeddedTestServer https_server_;

 private:
  static std::unique_ptr<net::test_server::HttpResponse>
  RedirectResponseHandler(const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, "/r?",
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", request.relative_url.substr(3));
    http_response->AddCustomHeader("Cache-Control", "no-cache");
    return std::move(http_response);
  }

  // Responds with a prologue-only signed exchange that triggers a fallback
  // redirect.
  std::unique_ptr<net::test_server::HttpResponse> FallbackSxgResponseHandler(
      const net::test_server::HttpRequest& request) {
    const std::string prefix = "/fallback_sxg?";
    if (!base::StartsWith(request.relative_url, prefix,
                          base::CompareCase::SENSITIVE)) {
      return nullptr;
    }
    std::string fallback_url(request.relative_url.substr(prefix.length()));
    if (fallback_url.empty()) {
      // If fallback URL is not specified, fallback to itself.
      fallback_url = https_server_.GetURL(prefix).spec();
    }

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/signed-exchange;v=b3");

    std::string sxg("sxg1-b3", 8);
    sxg.push_back(fallback_url.length() >> 8);
    sxg.push_back(fallback_url.length() & 0xff);
    sxg += fallback_url;
    // FallbackUrlAndAfter() requires 6 more bytes for sizes of next fields.
    sxg.resize(sxg.length() + 6);

    http_response->set_content(sxg);
    return std::move(http_response);
  }

  void MonitorRequest(const net::test_server::HttpRequest& request) {
    const auto it = request.headers.find(net::HttpRequestHeaders::kAccept);
    if (it == request.headers.end())
      return;
    // Note this method is called on the EmbeddedTestServer's background thread.
    base::AutoLock lock(url_accept_header_map_lock_);
    url_accept_header_map_[request.base_url.Resolve(request.relative_url)] =
        it->second;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedFeatureList feature_list_for_accept_header_;

  // url_accept_header_map_ is accessed both on the main thread and on the
  // EmbeddedTestServer's background thread via MonitorRequest(), so it must be
  // locked.
  mutable base::Lock url_accept_header_map_lock_;
  std::map<GURL, std::string> url_accept_header_map_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest, Simple) {
  const GURL test_url = https_server_.GetURL("/sxg/test.html");
  NavigateAndWaitForTitle(test_url, test_url.spec());
  CheckNavigationAcceptHeader({test_url});
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest, Redirect) {
  const GURL test_url = https_server_.GetURL("/sxg/test.html");
  const GURL redirect_url = https_server_.GetURL("/r?" + test_url.spec());
  const GURL redirect_redirect_url =
      https_server_.GetURL("/r?" + redirect_url.spec());
  NavigateWithRedirectAndWaitForTitle(redirect_redirect_url, test_url,
                                      test_url.spec());

  CheckNavigationAcceptHeader({redirect_redirect_url, redirect_url, test_url});
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest,
                       FallbackRedirect) {
  const GURL fallback_url = https_server_.GetURL("/sxg/test.html");
  const GURL test_url =
      https_server_.GetURL("/fallback_sxg?" + fallback_url.spec());
  NavigateWithRedirectAndWaitForTitle(test_url, fallback_url,
                                      fallback_url.spec());

  CheckNavigationAcceptHeader({test_url});
  CheckFallbackAcceptHeader({fallback_url});
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest,
                       FallbackRedirectLoop) {
  const base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  FinishNavigationObserver finish_navigation_observer(shell()->web_contents(),
                                                      run_loop.QuitClosure());
  const GURL test_url = https_server_.GetURL("/fallback_sxg?");
  EXPECT_FALSE(NavigateToURL(shell()->web_contents(), test_url));
  run_loop.Run();
  ASSERT_TRUE(finish_navigation_observer.error_code())
      << "Unexpected navigation success: " << test_url;
  EXPECT_EQ(net::ERR_TOO_MANY_REDIRECTS,
            *finish_navigation_observer.error_code());
  histogram_tester.ExpectUniqueSample(kRedirectLoopHistogram, true, 1);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest,
                       PrefetchEnabledPageEnabledTarget) {
  const GURL target = https_server_.GetURL("/sxg/hello.txt");
  const GURL page_url =
      https_server_.GetURL(std::string("/sxg/prefetch.html#") + target.spec());
  NavigateAndWaitForTitle(page_url, "OK");
  CheckPrefetchAcceptHeader({target});
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest,
                       PrefetchRedirect) {
  const GURL target = https_server_.GetURL("/sxg/hello.txt");
  const GURL redirect_url = https_server_.GetURL("/r?" + target.spec());
  const GURL redirect_redirect_url =
      https_server_.GetURL("/r?" + redirect_url.spec());

  const GURL page_url = https_server_.GetURL(
      std::string("/sxg/prefetch.html#") + redirect_redirect_url.spec());

  NavigateAndWaitForTitle(page_url, "OK");

  CheckPrefetchAcceptHeader({redirect_redirect_url, redirect_url, target});
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest, ServiceWorker) {
  NavigateAndWaitForTitle(https_server_.GetURL("/sxg/service-worker.html"),
                          "Done");

  const std::string frame_accept = kFrameAcceptHeaderValue;
  const std::string frame_accept_with_sxg =
      frame_accept + std::string(kAcceptHeaderSignedExchangeSuffix);
  const std::vector<std::string> scopes = {"/sxg/sw-scope-generated/",
                                           "/sxg/sw-scope-navigation-preload/",
                                           "/sxg/sw-scope-no-respond-with/"};
  for (const auto& scope : scopes) {
    SCOPED_TRACE(scope);
    const bool is_generated_scope =
        scope == std::string("/sxg/sw-scope-generated/");
    const GURL target_url = https_server_.GetURL(scope + "test.html");
    const GURL redirect_target_url =
        https_server_.GetURL("/r?" + target_url.spec());
    const GURL redirect_redirect_target_url =
        https_server_.GetURL("/r?" + redirect_target_url.spec());

    const std::string expected_title =
        is_generated_scope ? frame_accept_with_sxg : "Done";
    const std::optional<std::string> expected_target_accept_header =
        is_generated_scope ? std::nullopt
                           : std::optional<std::string>(frame_accept_with_sxg);

    NavigateAndWaitForTitle(target_url, expected_title);
    EXPECT_EQ(expected_target_accept_header,
              GetInterceptedAcceptHeader(target_url));
    ClearInterceptedAcceptHeaders();

    NavigateWithRedirectAndWaitForTitle(redirect_target_url, target_url,
                                        expected_title);
    CheckNavigationAcceptHeader({redirect_target_url});
    EXPECT_EQ(expected_target_accept_header,
              GetInterceptedAcceptHeader(target_url));
    ClearInterceptedAcceptHeaders();

    NavigateWithRedirectAndWaitForTitle(redirect_redirect_target_url,
                                        target_url, expected_title);
    CheckNavigationAcceptHeader(
        {redirect_redirect_target_url, redirect_target_url});
    EXPECT_EQ(expected_target_accept_header,
              GetInterceptedAcceptHeader(target_url));
    ClearInterceptedAcceptHeaders();
  }
}

IN_PROC_BROWSER_TEST_F(SignedExchangeAcceptHeaderBrowserTest,
                       ServiceWorkerPrefetch) {
  NavigateAndWaitForTitle(
      https_server_.GetURL("/sxg/service-worker-prefetch.html"), "Done");
  const std::string scope = "/sxg/sw-prefetch-scope/";
  const GURL target_url = https_server_.GetURL(scope + "test.html");

  const GURL prefetch_target =
      https_server_.GetURL(std::string("/sxg/hello.txt"));
  const std::string load_prefetch_script = base::StringPrintf(
      "(function loadPrefetch(urls) {"
      "  for (let url of urls) {"
      "    let link = document.createElement('link');"
      "    link.rel = 'prefetch';"
      "    link.href = url;"
      "    document.body.appendChild(link);"
      "  }"
      "  async function check() {"
      "    while (true) {"
      "      const entries = performance.getEntriesByType('resource');"
      "      const url_set = new Set(urls);"
      "      for (let entry of entries) {"
      "        url_set.delete(entry.name);"
      "      }"
      "      if (!url_set.size) {"
      "        return true;"
      "      } else {"
      "        await new Promise(resolve => setTimeout(resolve, 100));"
      "      }"
      "    }"
      "  }"
      "  return check();"
      "})(['%s'])",
      prefetch_target.spec().c_str());

  NavigateAndWaitForTitle(target_url, "Done");
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), load_prefetch_script));
  CheckPrefetchAcceptHeader({prefetch_target});
  ClearInterceptedAcceptHeaders();
}

#if BUILDFLAG(ENABLE_REPORTING)

class SignedExchangeReportingBrowserTest
    : public SignedExchangeRequestHandlerBrowserTest {
 public:
  SignedExchangeReportingBrowserTest() {
    feature_list_.InitAndEnableFeature(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
  }

  ~SignedExchangeReportingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SignedExchangeRequestHandlerBrowserTestBase::SetUpOnMainThread();

    // Make all attempts to connect to the domain the SXG is for fail with a DNS
    // error. Without this, they're fail with ERR_NOT_IMPLEMENTED. Making
    // requests fail with ERR_NAME_NOT_RESOLVED instead better matches what
    // happens in production.
    host_resolver()->AddSimulatedFailure("test.example.org");

    host_resolver()->AddRule("prefetch-origin.test", "127.0.0.1");

    // Set up certificate for the report server.
    net::CertVerifyResult ssl_server_result;
    ssl_server_result.verified_cert = ssl_server_.GetCertificate();
    ssl_server_result.is_issued_by_known_root = false;
    ssl_server_result.policy_compliance =
        net::ct::CTPolicyCompliance::CT_POLICY_COMPLIES_VIA_SCTS;
    mock_cert_verifier()->AddResultForCert(ssl_server_.GetCertificate(),
                                           ssl_server_result, net::OK);

    // Make the MockCertVerifier treat the signed exchange's certificate
    // "prime256v1-sha256.public.pem" as invalid for "test.example.org", which
    // should generate a report.
    scoped_refptr<net::X509Certificate> original_cert =
        SignedExchangeBrowserTestHelper::LoadCertificate();
    net::CertVerifyResult dummy_result;
    dummy_result.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
    dummy_result.verified_cert = original_cert;
    mock_cert_verifier()->AddResultForCertAndHost(
        original_cert, "test.example.org", dummy_result, net::OK);
    InstallMockCertChainInterceptor();

    learn_report_to_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &ssl_server_, kLearnReportToPath);
    report_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &ssl_server_, kReportPath);

    // Set up server used to serve the signed exchange.
    ssl_server_.ServeFilesFromSourceDirectory("content/test/data");
    EXPECT_TRUE(ssl_server_.Start());
  }

  const GURL sxg_url() const {
    return ssl_server_.GetURL("/sxg/test.example.org_test.sxg");
  }

  // The URL the SXG resource claims to be for.
  static GURL sxg_validity_url() {
    return GURL("https://test.example.org/test/");
  }

 protected:
  const char* kLearnReportToPath = "/report-to";
  const char* kReportPath = "/report";

  base::test::ScopedFeatureList feature_list_;

  // This server both serves the signed exchange and receives the report for the
  // certificate error.
  net::test_server::EmbeddedTestServer ssl_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  std::unique_ptr<net::test_server::ControllableHttpResponse>
      learn_report_to_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> report_response_;
};

IN_PROC_BROWSER_TEST_P(SignedExchangeReportingBrowserTest,
                       CertErrorSendsReport) {
  GURL report_url = ssl_server_.GetURL(kReportPath);

  // Get Report-To and NEL information for the server that serves the signed
  // exchange. The same site is also used for the NetworkAnonymizationKey. This
  // should result in sending a report to that server a request for that signed
  // exchange fails with a certificate error.
  {
    GURL learn_report_to_url = ssl_server_.GetURL(kLearnReportToPath);
    TestNavigationObserver same_tab_observer(shell()->web_contents(),
                                             1 /* number_of_navigations */);
    NavigationController::LoadURLParams params(learn_report_to_url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    shell()->web_contents()->GetController().LoadURLWithParams(params);
    learn_report_to_response_->WaitForRequest();
    learn_report_to_response_->Send("HTTP/1.1 200 OK\r\n");
    learn_report_to_response_->Send("Report-To: {\"endpoints\":[{\"url\":\"" +
                                    report_url.spec() +
                                    "\"}],\"max_age\":86400}\r\n");
    learn_report_to_response_->Send(
        "NEL: {\"report_to\":\"default\",\"max_age\":86400}\r\n");
    learn_report_to_response_->Send("\r\n");
    learn_report_to_response_->Done();
    same_tab_observer.Wait();
  }

  if (UsePrefetch()) {
    // Matches MaybeTriggerPrefetchSXG(), but uses |ssl_server_| instead of
    // embedded_test_server().
    const GURL prefetch_html_url = ssl_server_.GetURL(
        std::string("/sxg/prefetch.html#") + sxg_url().spec());
    std::u16string expected_title = u"FAIL";
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), prefetch_html_url));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  } else {
    // Try to navigate to the signed exchange. The signed exchange fails due
    // to the bad certificate. That results in trying to load the
    // resource the SXG refers to directly, which should fail with
    // ERR_NAME_NOT_RESOLVED.
    NavigationHandleObserver observer(shell()->web_contents(), sxg_url());
    EXPECT_FALSE(NavigateToURL(shell(), sxg_url()));
    EXPECT_EQ(sxg_validity_url(),
              shell()->web_contents()->GetLastCommittedURL());
    EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, observer.net_error_code());
  }

  // Check that a report was received.
  report_response_->WaitForRequest();
  EXPECT_EQ(report_url, report_response_->http_request()->GetURL());
  EXPECT_EQ(net::test_server::METHOD_POST,
            report_response_->http_request()->method);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SignedExchangeReportingBrowserTest,
                         ::testing::Bool());

#endif  // BUILDFLAG(ENABLE_REPORTING)

class SignedExchangePKPBrowserTest
    : public SignedExchangeRequestHandlerBrowserTest {
 public:
  SignedExchangePKPBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kStaticKeyPinningEnforcement);
  }
  void SetUpOnMainThread() override {
    SignedExchangeRequestHandlerBrowserTest::SetUpOnMainThread();

    // Make all attempts to connect to the domain the SXG is for fail with a DNS
    // error. Without this, they're fail with ERR_NOT_IMPLEMENTED. Making
    // requests fail with ERR_NAME_NOT_RESOLVED instead better matches what
    // happens in production.
    host_resolver()->AddSimulatedFailure("test.example.org");

    // Use a mock cert issued by a known root, so that PKP violation will not be
    // bypassed. It also causes a CT failure, but PKP error takes precedence.
    InstallMockCertByKnownRoot();
    InstallMockCertChainInterceptor();

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    EXPECT_TRUE(embedded_test_server()->Start());
    EnableStaticPins(embedded_test_server()->port());
  }

  void TearDownOnMainThread() override {
    if (IsOutOfProcessNetworkService()) {
      mojo::ScopedAllowSyncCallForTesting allow_sync_call;

      mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
      GetNetworkService()->BindTestInterfaceForTesting(
          network_service_test.BindNewPipeAndPassReceiver());
      network_service_test->SetTransportSecurityStateSource(0);
    } else {
      RunOnIOThreadBlocking(
          base::BindOnce(&SignedExchangePKPBrowserTest::CleanUpOnIOThread,
                         base::Unretained(this)));
    }
    SignedExchangeRequestHandlerBrowserTest::TearDownOnMainThread();
  }

  void EnableStaticPins(int reporting_port) {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    StoragePartition* partition = shell()
                                      ->web_contents()
                                      ->GetBrowserContext()
                                      ->GetDefaultStoragePartition();
    partition->GetNetworkContext()->EnableStaticKeyPinningForTesting();
    partition->FlushNetworkInterfaceForTesting();

    if (IsOutOfProcessNetworkService()) {
      mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
      GetNetworkService()->BindTestInterfaceForTesting(
          network_service_test.BindNewPipeAndPassReceiver());
      network_service_test->SetTransportSecurityStateSource(reporting_port);
    } else {
      // TODO(crbug.com/40649862):  This code is not threadsafe, as the
      // network stack does not run on the IO thread. Ideally, the
      // NetworkServiceTest object would be set up in-process on the network
      // service's thread, and this path would be removed.
      RunOnIOThreadBlocking(base::BindOnce(
          &SignedExchangePKPBrowserTest::SetTransportSecurityStateSourceOnIO,
          base::Unretained(this), reporting_port));
    }
  }

 private:
  void RunOnIOThreadBlocking(base::OnceClosure task) {
    base::RunLoop run_loop;
    GetIOThreadTaskRunner({})->PostTaskAndReply(FROM_HERE, std::move(task),
                                                run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetTransportSecurityStateSourceOnIO(int reporting_port) {
    transport_security_state_source_ =
        std::make_unique<net::ScopedTransportSecurityStateSource>(
            reporting_port);
  }

  void CleanUpOnIOThread() { transport_security_state_source_.reset(); }

  // Only used when NetworkService is disabled. Accessed on IO thread.
  std::unique_ptr<net::ScopedTransportSecurityStateSource>
      transport_security_state_source_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SignedExchangePKPBrowserTest, PKPViolation) {
  GURL sxg_url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");
  GURL fallback_url("https://test.example.org/test/");

  MaybeTriggerPrefetchSXG(sxg_url, false);

  NavigationHandleObserver observer(shell()->web_contents(), sxg_url);
  EXPECT_FALSE(NavigateToURL(shell(), sxg_url));
  EXPECT_EQ(fallback_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(net::ERR_NAME_NOT_RESOLVED, observer.net_error_code());
  histogram_tester_.ExpectUniqueSample(
      kLoadResultHistogram, SignedExchangeLoadResult::kPKPViolationError,
      UsePrefetch() ? 2 : 1);
}

INSTANTIATE_TEST_SUITE_P(All, SignedExchangePKPBrowserTest, ::testing::Bool());

}  // namespace content

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/loader/cross_site_document_resource_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

using testing::Not;
using testing::HasSubstr;
using Action = network::CrossOriginReadBlocking::Action;

namespace {

enum CorbExpectations {
  kShouldBeBlocked = 1 << 0,
  kShouldBeSniffed = 1 << 1,
  kShouldLogContentLengthUma = 1 << 2,

  kShouldBeAllowedWithoutSniffing = 0,
  kShouldBeBlockedWithoutSniffing = kShouldBeBlocked,
  kShouldBeSniffedAndAllowed = kShouldBeSniffed,
  kShouldBeSniffedAndBlocked = kShouldBeSniffed | kShouldBeBlocked,
};

CorbExpectations operator|(CorbExpectations a, CorbExpectations b) {
  return static_cast<CorbExpectations>(static_cast<int>(a) |
                                       static_cast<int>(b));
}

std::ostream& operator<<(std::ostream& os, const CorbExpectations& value) {
  if (value == 0) {
    os << "(none)";
    return os;
  }

  os << "( ";
  if (0 != (value & kShouldBeBlocked))
    os << "kShouldBeBlocked ";
  if (0 != (value & kShouldBeSniffed))
    os << "kShouldBeSniffed ";
  if (0 != (value & kShouldLogContentLengthUma))
    os << "kShouldLogContentLengthUma ";
  os << ")";
  return os;
}

// Ensure the correct histograms are incremented for blocking events.
// Assumes the resource type is XHR.
void InspectHistograms(const base::HistogramTester& histograms,
                       const CorbExpectations& expectations,
                       const std::string& resource_name,
                       ResourceType resource_type) {
  // //services/network doesn't have access to content::ResourceType and
  // therefore cannot log some CORB UMAs.
  bool is_restricted_uma_expected = false;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    is_restricted_uma_expected = true;
    FetchHistogramsFromChildProcesses();
  }

  std::string bucket;
  if (base::MatchPattern(resource_name, "*.html")) {
    bucket = "HTML";
  } else if (base::MatchPattern(resource_name, "*.xml")) {
    bucket = "XML";
  } else if (base::MatchPattern(resource_name, "*.json")) {
    bucket = "JSON";
  } else if (base::MatchPattern(resource_name, "*.txt")) {
    bucket = "Plain";
  } else {
    bucket = "Others";
  }

  // Determine the appropriate histograms, including a start and end action
  // (which are verified in unit tests), a read size if it was sniffed, and
  // additional blocked metrics if it was blocked.
  base::HistogramTester::CountsMap expected_counts;
  std::string base = "SiteIsolation.XSD.Browser";
  expected_counts[base + ".Action"] = 2;
  if ((base::MatchPattern(resource_name, "*prefixed*") || bucket == "Others") &&
      (0 != (expectations & kShouldBeBlocked)) && !is_restricted_uma_expected) {
    expected_counts[base + ".BlockedForParserBreaker"] = 1;
  }
  if (0 != (expectations & kShouldBeSniffed))
    expected_counts[base + ".BytesReadForSniffing"] = 1;
  if (0 != (expectations & kShouldBeBlocked && !is_restricted_uma_expected)) {
    expected_counts[base + ".Blocked"] = 1;
    expected_counts[base + ".Blocked." + bucket] = 1;
  }
  if (0 != (expectations & kShouldBeBlocked)) {
    expected_counts[base + ".Blocked.ContentLength.WasAvailable"] = 1;
    bool should_have_content_length =
        0 != (expectations & kShouldLogContentLengthUma);
    histograms.ExpectBucketCount(base + ".Blocked.ContentLength.WasAvailable",
                                 should_have_content_length, 1);

    if (should_have_content_length)
      expected_counts[base + ".Blocked.ContentLength.ValueIfAvailable"] = 1;
  }

  // Make sure that the expected metrics, and only those metrics, were
  // incremented.
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("SiteIsolation.XSD.Browser"),
              testing::ContainerEq(expected_counts))
      << "For resource_name=" << resource_name
      << ", expectations=" << expectations;

  // Determine if the bucket for the resource type (XHR) was incremented.
  if (0 != (expectations & kShouldBeBlocked) && !is_restricted_uma_expected) {
    EXPECT_THAT(histograms.GetAllSamples(base + ".Blocked"),
                testing::ElementsAre(base::Bucket(resource_type, 1)))
        << "The wrong Blocked bucket was incremented.";
    EXPECT_THAT(histograms.GetAllSamples(base + ".Blocked." + bucket),
                testing::ElementsAre(base::Bucket(resource_type, 1)))
        << "The wrong Blocked bucket was incremented.";
  }

  // SiteIsolation.XSD.Browser.Action should always include kResponseStarted.
  histograms.ExpectBucketCount(base + ".Action",
                               static_cast<int>(Action::kResponseStarted), 1);

  // Second value in SiteIsolation.XSD.Browser.Action depends on |expectations|.
  Action expected_action = static_cast<Action>(-1);
  if (expectations & kShouldBeBlocked) {
    if (expectations & kShouldBeSniffed)
      expected_action = Action::kBlockedAfterSniffing;
    else
      expected_action = Action::kBlockedWithoutSniffing;
  } else {
    if (expectations & kShouldBeSniffed)
      expected_action = Action::kAllowedAfterSniffing;
    else
      expected_action = Action::kAllowedWithoutSniffing;
  }
  histograms.ExpectBucketCount(base + ".Action",
                               static_cast<int>(expected_action), 1);
}

// Helper for intercepting a resource request to the given URL and capturing the
// response headers and body.
//
// Note that after the request completes, the original requestor (e.g. the
// renderer) will see an injected request failure (this is easier to accomplish
// than forwarding the intercepted response to the original requestor),
class RequestInterceptor {
 public:
  // Start intercepting requests to |url_to_intercept|.
  explicit RequestInterceptor(const GURL& url_to_intercept)
      : url_to_intercept_(url_to_intercept),
        interceptor_(
            base::BindRepeating(&RequestInterceptor::InterceptorCallback,
                                base::Unretained(this))) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(url_to_intercept.is_valid());

    test_client_ptr_info_ = test_client_.CreateInterfacePtr().PassInterface();
  }

  // Waits until a request gets intercepted and completed.
  void WaitForRequestCompletion() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!request_completed_);
    test_client_.RunUntilComplete();

    // Read the intercepted response body into |body_|.
    if (test_client_.completion_status().error_code == net::OK) {
      char buffer[128];
      while (true) {
        uint32_t num_bytes = sizeof(buffer);
        auto result = test_client_.response_body().ReadData(
            buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
        if (result != MOJO_RESULT_OK)
          break;

        if (num_bytes == 0)
          break;

        body_ += std::string(buffer, num_bytes);
      }
    }

    // Wait until IO cleanup completes.
    base::RunLoop run_loop;
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&RequestInterceptor::CleanUpOnIOThread,
                       base::Unretained(this)),
        run_loop.QuitClosure());
    run_loop.Run();

    // Mark the request as completed (for DCHECK purposes).
    request_completed_ = true;
  }

  const network::URLLoaderCompletionStatus& completion_status() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_completed_);
    return test_client_.completion_status();
  }

  const network::ResourceResponseHead& response_head() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_completed_);
    return test_client_.response_head();
  }

  const std::string& response_body() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_completed_);
    return body_;
  }

  void Verify(CorbExpectations expectations) {
    if (0 != (expectations & kShouldBeBlocked)) {
      ASSERT_EQ(net::OK, completion_status().error_code);

      // Verify that the body is empty.
      EXPECT_EQ("", response_body());
      EXPECT_EQ(0, completion_status().decoded_body_length);

      // Verify that other response parts have been sanitized.
      EXPECT_EQ(0u, response_head().content_length);
      const std::string& headers = response_head().headers->raw_headers();
      EXPECT_THAT(headers, Not(HasSubstr("Content-Length")));
      EXPECT_THAT(headers, Not(HasSubstr("Content-Type")));

      // Verify that the console message would have been printed.
      EXPECT_TRUE(completion_status().should_report_corb_blocking);
    } else {
      EXPECT_FALSE(completion_status().should_report_corb_blocking);
    }
  }

 private:
  bool InterceptorCallback(URLLoaderInterceptor::RequestParams* params) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(params);

    if (url_to_intercept_ != params->url_request.url)
      return false;

    // Prevent more than one intercept.
    if (request_intercepted_)
      return false;
    request_intercepted_ = true;

    // Inject |test_client_| into the request.
    DCHECK(!original_client_);
    original_client_ = std::move(params->client);
    test_client_ptr_.Bind(std::move(test_client_ptr_info_));
    test_client_binding_ =
        std::make_unique<mojo::Binding<network::mojom::URLLoaderClient>>(
            test_client_ptr_.get(), mojo::MakeRequest(&params->client));

    // Forward the request to the original URLLoaderFactory.
    return false;
  }

  void CleanUpOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // Tell the |original_client_| that the request has completed (and that it
    // can release its URLLoaderClient.
    original_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_NOT_IMPLEMENTED));

    // Reset all temporary mojo bindings.
    original_client_.reset();
    test_client_binding_.reset();
    test_client_ptr_.reset();
  }

  const GURL url_to_intercept_;
  URLLoaderInterceptor interceptor_;

  // |test_client_ptr_info_| below is used to transition results of
  // |test_client_.CreateInterfacePtr()| into IO thread.
  network::mojom::URLLoaderClientPtrInfo test_client_ptr_info_;

  // UI thread state:
  network::TestURLLoaderClient test_client_;
  std::string body_;
  bool request_completed_ = false;

  // IO thread state:
  network::mojom::URLLoaderClientPtr original_client_;
  bool request_intercepted_ = false;
  network::mojom::URLLoaderClientPtr test_client_ptr_;
  std::unique_ptr<mojo::Binding<network::mojom::URLLoaderClient>>
      test_client_binding_;

  DISALLOW_COPY_AND_ASSIGN(RequestInterceptor);
};

}  // namespace

// These tests verify that the browser process blocks cross-site HTML, XML,
// JSON, and some plain text responses when they are not otherwise permitted
// (e.g., by CORS).  This ensures that such responses never end up in the
// renderer process where they might be accessible via a bug.  Careful attention
// is paid to allow other cross-site resources necessary for rendering,
// including cases that may be mislabeled as blocked MIME type.
class CrossSiteDocumentBlockingTestBase : public ContentBrowserTest {
 public:
  CrossSiteDocumentBlockingTestBase() = default;
  ~CrossSiteDocumentBlockingTestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // EmbeddedTestServer::InitializeAndListen() initializes its |base_url_|
    // which is required below. This cannot invoke Start() however as that kicks
    // off the "EmbeddedTestServer IO Thread" which then races with
    // initialization in ContentBrowserTest::SetUp(), http://crbug.com/674545.
    // Additionally the server should not be started prior to setting up
    // ControllableHttpResponse(s) in some individual tests below.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames and standard ports in URLs (i.e.,
    // without having to inject the port number into all URLs), which we can use
    // to create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + embedded_test_server()->host_port_pair().ToString() +
            ",EXCLUDE localhost");
  }

  void VerifyImgRequest(std::string resource, CorbExpectations expectations) {
    SCOPED_TRACE("... while testing via <img> tag");

    // Navigate to the test page while request interceptor is active.
    GURL resource_url(
        std::string("http://cross-origin.com/site_isolation/" + resource));
    RequestInterceptor interceptor(resource_url);
    EXPECT_TRUE(NavigateToURL(shell(), GURL("http://foo.com/title1.html")));

    // Issue the request that will be intercepted.
    base::HistogramTester histograms;
    const char kScriptTemplate[] = R"(
        var img = document.createElement('img');
        img.src = $1;
        document.body.appendChild(img); )";
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, resource_url)));
    interceptor.WaitForRequestCompletion();

    // Verify...
    InspectHistograms(histograms, expectations, resource, RESOURCE_TYPE_IMAGE);
    interceptor.Verify(expectations);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingTestBase);
};

enum class TestMode {
  kWithoutOutOfBlinkCors,
  kWithOutOfBlinkCors,
};
class CrossSiteDocumentBlockingTest
    : public CrossSiteDocumentBlockingTestBase,
      public testing::WithParamInterface<TestMode> {
 public:
  CrossSiteDocumentBlockingTest() {
    switch (GetParam()) {
      case TestMode::kWithoutOutOfBlinkCors:
        scoped_feature_list_.InitAndDisableFeature(
            network::features::kOutOfBlinkCORS);
        break;
      case TestMode::kWithOutOfBlinkCors:
        scoped_feature_list_.InitAndEnableFeature(
            network::features::kOutOfBlinkCORS);
        break;
    }
  }
  ~CrossSiteDocumentBlockingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingTest);
};

IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingTest, BlockImages) {
  embedded_test_server()->StartAcceptingConnections();

  // The following are files under content/test/data/site_isolation. All
  // should be disallowed for cross site XHR under the document blocking policy.
  //   valid.*        - Correctly labeled HTML/XML/JSON files.
  //   *.txt          - Plain text that sniffs as HTML, XML, or JSON.
  //   htmlN_dtd.*    - Various HTML templates to test.
  //   json-prefixed* - parser-breaking prefixes
  const char* blocked_resources[] = {"valid.html",
                                     "valid.xml",
                                     "valid.json",
                                     "html.txt",
                                     "xml.txt",
                                     "json.txt",
                                     "comment_valid.html",
                                     "html4_dtd.html",
                                     "html4_dtd.txt",
                                     "html5_dtd.html",
                                     "html5_dtd.txt",
                                     "json.js",
                                     "json-prefixed-1.js",
                                     "json-prefixed-2.js",
                                     "json-prefixed-3.js",
                                     "json-prefixed-4.js",
                                     "nosniff.json.js",
                                     "nosniff.json-prefixed.js"};
  for (const char* resource : blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    VerifyImgRequest(resource,
                     kShouldBeSniffedAndBlocked | kShouldLogContentLengthUma);
  }

  // These files should be disallowed without sniffing.
  //   nosniff.*   - Won't sniff correctly, but blocked because of nosniff.
  const char* nosniff_blocked_resources[] = {"nosniff.html", "nosniff.xml",
                                             "nosniff.json", "nosniff.txt"};
  for (const char* resource : nosniff_blocked_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    VerifyImgRequest(resource, kShouldBeBlockedWithoutSniffing);
  }

  // These files are allowed for XHR under the document blocking policy because
  // the sniffing logic determines they are not actually documents.
  //   *js.*   - JavaScript mislabeled as a document.
  //   jsonp.* - JSONP (i.e., script) mislabeled as a document.
  //   img.*   - Contents that won't match the document label.
  //   valid.* - Correctly labeled responses of non-document types.
  const char* sniff_allowed_resources[] = {"html-prefix.txt",
                                           "js.html",
                                           "comment_js.html",
                                           "js.xml",
                                           "js.json",
                                           "js.txt",
                                           "jsonp.html",
                                           "jsonp.xml",
                                           "jsonp.json",
                                           "jsonp.txt",
                                           "img.html",
                                           "img.xml",
                                           "img.json",
                                           "img.txt",
                                           "valid.js",
                                           "json-list.js",
                                           "nosniff.json-list.js",
                                           "js-html-polyglot.html",
                                           "js-html-polyglot2.html"};
  for (const char* resource : sniff_allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    VerifyImgRequest(resource, kShouldBeSniffedAndAllowed);
  }
}

IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingTest, BlockFetches) {
  embedded_test_server()->StartAcceptingConnections();
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // These files should be allowed for XHR under the document blocking policy.
  //   cors.*  - Correctly labeled documents with valid CORS headers.
  const char* allowed_resources[] = {"cors.html", "cors.xml", "cors.json",
                                     "cors.txt"};
  for (const char* resource : allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));
    base::HistogramTester histograms;
    bool was_blocked;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf("sendRequest('%s');", resource),
        &was_blocked));
    EXPECT_FALSE(was_blocked);
    InspectHistograms(histograms, kShouldBeAllowedWithoutSniffing, resource,
                      RESOURCE_TYPE_XHR);
  }
}

IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingTest, BlockForVariousTargets) {
  // This webpage loads a cross-site HTML page in different targets such as
  // <img>,<link>,<embed>, etc. Since the requested document is blocked, and one
  // character string (' ') is returned instead, this tests that the renderer
  // does not crash even when it receives a response body which is " ", whose
  // length is different from what's described in "content-length" for such
  // different targets.

  // TODO(nick): Split up these cases, and add positive assertions here about
  // what actually happens in these various resource-block cases.
  embedded_test_server()->StartAcceptingConnections();
  GURL foo("http://foo.com/cross_site_document_blocking/request_target.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo));

  // TODO(creis): Wait for all the subresources to load and ensure renderer
  // process is still alive.
}

// Checks to see that CORB blocking applies to processes hosting error pages.
// Regression test for https://crbug.com/814913.
IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingTest,
                       BlockRequestFromErrorPage) {
  embedded_test_server()->StartAcceptingConnections();
  GURL error_url = embedded_test_server()->GetURL("bar.com", "/close-socket");
  GURL subresource_url =
      embedded_test_server()->GetURL("foo.com", "/site_isolation/json.js");

  // Load |error_url| and expect a network error page.
  TestNavigationObserver observer(shell()->web_contents());
  EXPECT_FALSE(NavigateToURL(shell(), error_url));
  EXPECT_EQ(error_url, observer.last_navigation_url());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());

  // Add a <script> tag whose src is a CORB-protected resource. Expect no
  // window.onerror to result, because no syntax error is generated by the empty
  // response.
  std::string script = R"((subresource_url => {
    window.onerror = () => domAutomationController.send("CORB BYPASSED");
    var script = document.createElement('script');
    script.src = subresource_url;
    script.onload = () => domAutomationController.send("CORB WORKED");
    document.body.appendChild(script);
    }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      shell(), script + "('" + subresource_url.spec() + "')", &result));

  EXPECT_EQ("CORB WORKED", result);
}

IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingTest, BlockHeaders) {
  embedded_test_server()->StartAcceptingConnections();

  // Prepare to intercept the network request at the IPC layer.
  // This has to be done before the RenderFrameHostImpl is created.
  //
  // Note: we want to verify that the blocking prevents the data from being sent
  // over IPC.  Testing later (e.g. via Response/Headers Web APIs) might give a
  // false sense of security, since some sanitization happens inside the
  // renderer (e.g. via FetchResponseData::CreateCORSFilteredResponse).
  GURL bar_url("http://bar.com/cross_site_document_blocking/headers-test.json");
  RequestInterceptor interceptor(bar_url);

  // Navigate to the test page.
  GURL foo_url("http://foo.com/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = R"(
      var img = document.createElement('img');
      img.src = $1;
      document.body.appendChild(img); )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, bar_url)));
  interceptor.WaitForRequestCompletion();

  // Verify that the response completed successfully, was blocked and was logged
  // as having initially a non-empty body.
  interceptor.Verify(kShouldBeBlockedWithoutSniffing |
                     kShouldLogContentLengthUma);

  // Verify that most response headers have been removed by CORB.
  const std::string& headers =
      interceptor.response_head().headers->raw_headers();
  EXPECT_THAT(headers, HasSubstr("Access-Control-Allow-Origin: https://other"));
  EXPECT_THAT(headers, Not(HasSubstr("Cache-Control")));
  EXPECT_THAT(headers, Not(HasSubstr("Content-Language")));
  EXPECT_THAT(headers, Not(HasSubstr("Content-Length")));
  EXPECT_THAT(headers, Not(HasSubstr("Content-Type")));
  EXPECT_THAT(headers, Not(HasSubstr("Expires")));
  EXPECT_THAT(headers, Not(HasSubstr("Last-Modified")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretCookieKey")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretCookieValue")));
  EXPECT_THAT(headers, Not(HasSubstr("Pragma")));
  EXPECT_THAT(headers, Not(HasSubstr("X-Content-Type-Options")));
  EXPECT_THAT(headers, Not(HasSubstr("X-My-Secret-Header")));

  // Verify that the body is empty.
  EXPECT_EQ("", interceptor.response_body());
  EXPECT_EQ(0, interceptor.completion_status().decoded_body_length);

  // Verify that other response parts have been sanitized.
  EXPECT_EQ(0u, interceptor.response_head().content_length);
}

IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingTest, PrefetchIsNotImpacted) {
  // Prepare for intercepting the resource request for testing prefetching.
  const char* kPrefetchResourcePath = "/prefetch-test";
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      kPrefetchResourcePath);

  // Navigate to a webpage containing a cross-origin frame.
  embedded_test_server()->StartAcceptingConnections();
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Inject a cross-origin <link rel="prefetch" ...> into the main frame.
  // TODO(lukasza): https://crbug.com/827633#c5: We might need to switch to
  // listening to the onload event below (after/if CORB starts to consistently
  // avoid injecting net errors).
  const char* prefetch_injection_script_template = R"(
      var link = document.createElement("link");
      link.rel = "prefetch";
      link.href = "/cross-site/b.com%s";
      link.as = "fetch";

      window.is_prefetch_done = false;
      function mark_prefetch_as_done() { window.is_prefetch_done = true }
      link.onerror = mark_prefetch_as_done;

      document.getElementsByTagName('head')[0].appendChild(link);
  )";
  std::string prefetch_injection_script = base::StringPrintf(
      prefetch_injection_script_template, kPrefetchResourcePath);
  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(), prefetch_injection_script));

  // Respond to the prefetch request in a way that:
  // 1) will enable caching
  // 2) won't finish until after CORB has blocked the response.
  FetchHistogramsFromChildProcesses();
  base::HistogramTester histograms;
  std::string response_bytes =
      "HTTP/1.1 200 OK\r\n"
      "Cache-Control: public, max-age=10\r\n"
      "Content-Type: text/html\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "\r\n"
      "<p>contents of the response</p>";
  response.WaitForRequest();
  response.Send(response_bytes);

  // Verify that CORB blocked the response.
  // TODO(lukasza): https://crbug.com/827633#c5: We might need to switch to
  // listening to the onload event below (after/if CORB starts to consistently
  // avoid injecting net errors).
  std::string wait_script = R"(
      function notify_prefetch_is_done() { domAutomationController.send(123); }

      if (window.is_prefetch_done) {
        // Can notify immediately if |window.is_prefetch_done| has already been
        // set by |prefetch_injection_script|.
        notify_prefetch_is_done();
      } else {
        // Otherwise wait for CORB's empty response to reach the renderer.
        link = document.getElementsByTagName('link')[0];
        link.onerror = notify_prefetch_is_done;
      }
  )";
  int answer;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(shell()->web_contents(), wait_script,
                                         &answer));
  EXPECT_EQ(123, answer);
  InspectHistograms(histograms, kShouldBeBlockedWithoutSniffing, "x.html",
                    RESOURCE_TYPE_PREFETCH);

  // Finish the HTTP response - this should store the response in the cache.
  response.Done();

  // Stop the HTTP server - this means the only way to get the response in
  // the |fetch_script| below is to get it from the cache (e.g. if the request
  // goes to the network there will be no HTTP server to handle it).
  // Note that stopping the HTTP server is not strictly required for the test to
  // be robust - ControllableHttpResponse handles only a single request, so
  // wouldn't handle the |fetch_script| request even if the HTTP server was
  // still running.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Verify that the cached response is available to the same-origin subframe
  // (e.g. that the network cache in the browser process got populated despite
  // CORB blocking).
  const char* fetch_script_template = R"(
      fetch('%s')
          .then(response => response.text())
          .then(responseBody => {
              domAutomationController.send(responseBody);
          })
          .catch(error => {
              var errorMessage = 'error: ' + error;
              console.log(errorMessage);
              domAutomationController.send(errorMessage);
          }); )";
  std::string fetch_script =
      base::StringPrintf(fetch_script_template, kPrefetchResourcePath);
  std::string response_body;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(shell()->web_contents()->GetAllFrames()[1],
                                    fetch_script, &response_body));
  EXPECT_EQ("<p>contents of the response</p>", response_body);
}

INSTANTIATE_TEST_CASE_P(WithoutOutOfBlinkCors,
                        CrossSiteDocumentBlockingTest,
                        ::testing::Values(TestMode::kWithoutOutOfBlinkCors));

INSTANTIATE_TEST_CASE_P(WithOutOfBlinkCors,
                        CrossSiteDocumentBlockingTest,
                        ::testing::Values(TestMode::kWithOutOfBlinkCors));

// This test class sets up a service worker that can be used to try to respond
// to same-origin requests with cross-origin responses.
class CrossSiteDocumentBlockingServiceWorkerTest : public ContentBrowserTest {
 public:
  CrossSiteDocumentBlockingServiceWorkerTest()
      : service_worker_https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        cross_origin_https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~CrossSiteDocumentBlockingServiceWorkerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
    ContentBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    SetupCrossSiteRedirector(embedded_test_server());

    service_worker_https_server_.ServeFilesFromSourceDirectory(
        "content/test/data");
    ASSERT_TRUE(service_worker_https_server_.Start());

    cross_origin_https_server_.ServeFilesFromSourceDirectory(
        "content/test/data");
    cross_origin_https_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(cross_origin_https_server_.Start());

    // Sanity check of test setup - the 2 https servers should be cross-site
    // (the second server should have a different hostname because of the call
    // to SetSSLConfig with CERT_COMMON_NAME_IS_DOMAIN argument).
    ASSERT_FALSE(SiteInstance::IsSameWebSite(
        shell()->web_contents()->GetBrowserContext(),
        GetURLOnServiceWorkerServer("/"), GetURLOnCrossOriginServer("/")));
  }

  GURL GetURLOnServiceWorkerServer(const std::string& path) {
    return service_worker_https_server_.GetURL(path);
  }

  GURL GetURLOnCrossOriginServer(const std::string& path) {
    return cross_origin_https_server_.GetURL(path);
  }

  void StopCrossOriginServer() {
    EXPECT_TRUE(cross_origin_https_server_.ShutdownAndWaitUntilComplete());
  }

  void SetUpServiceWorker() {
    GURL url = GetURLOnServiceWorkerServer(
        "/cross_site_document_blocking/request.html");
    ASSERT_TRUE(NavigateToURL(shell(), url));

    // Register the service worker.
    bool is_script_done;
    std::string script = R"(
        navigator.serviceWorker
            .register('/cross_site_document_blocking/service_worker.js')
            .then(registration => navigator.serviceWorker.ready)
            .then(function(r) { domAutomationController.send(true); })
            .catch(function(e) {
                console.log('error: ' + e);
                domAutomationController.send(false);
            }); )";
    ASSERT_TRUE(ExecuteScriptAndExtractBool(shell(), script, &is_script_done));
    ASSERT_TRUE(is_script_done);

    // Navigate again to the same URL - the service worker should be 1) active
    // at this time (because of waiting for |navigator.serviceWorker.ready|
    // above) and 2) controlling the current page (because of the reload).
    ASSERT_TRUE(NavigateToURL(shell(), url));
    bool is_controlled_by_service_worker;
    ASSERT_TRUE(ExecuteScriptAndExtractBool(
        shell(),
        "domAutomationController.send(!!navigator.serviceWorker.controller)",
        &is_controlled_by_service_worker));
    ASSERT_TRUE(is_controlled_by_service_worker);
  }

 private:
  // The test requires 2 https servers, because:
  // 1. Service workers are only supported on secure origins.
  // 2. One of tests requires fetching cross-origin resources from the
  //    original page and/or service worker - the target of the fetch needs to
  //    be a https server to avoid hitting the mixed content error.
  net::EmbeddedTestServer service_worker_https_server_;
  net::EmbeddedTestServer cross_origin_https_server_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingServiceWorkerTest);
};

// Issue a cross-origin request that will be handled entirely within a service
// worker (without reaching the network - the cross-origin response will be
// "faked" within the same-origin service worker, because the service worker
// used by the test recognizes the "data_from_service_worker" suffix in the
// URL).  This testcase is designed to hit the case in
// CrossSiteDocumentResourceHandler::ShouldBlockBasedOnHeaders where
// |response_type_via_service_worker| is equal to |kDefault|.  See also
// https://crbug.com/803672.
//
// TODO(lukasza): https://crbug.com/715640: This test might become invalid
// after servicification of service workers.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingServiceWorkerTest, NoNetwork) {
  // Skip this test when servicification of service workers (S13nServiceWorker)
  // is enabled because the browser process doesn't see the request or response
  // when the request is handled entirely within the service worker.
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;

  SetUpServiceWorker();

  base::HistogramTester histograms;
  std::string response;
  std::string script = R"(
      // Any cross-origin URL ending with .../data_from_service_worker can be
      // used here - it will be intercepted by the service worker and will never
      // go to the network.
      fetch('https://bar.com/data_from_service_worker')
          .then(response => response.text())
          .then(responseText => {
              domAutomationController.send(responseText);
          })
          .catch(error => {
              var errorMessage = 'error: ' + error;
              console.log(errorMessage);
              domAutomationController.send(errorMessage);
          }); )";
  EXPECT_TRUE(ExecuteScriptAndExtractString(shell(), script, &response));

  // Verify that CORB didn't block the response (since it was "faked" within the
  // service worker and didn't cross any security boundaries).
  EXPECT_EQ("Response created by service worker", response);
  InspectHistograms(histograms, kShouldBeAllowedWithoutSniffing, "blah.html",
                    RESOURCE_TYPE_XHR);
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingServiceWorkerTest,
                       NetworkToServiceWorkerResponse) {
  SetUpServiceWorker();

  // Make sure that the histograms generated by a service worker registration
  // have been recorded.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    FetchHistogramsFromChildProcesses();

  // Build a script for XHR-ing a cross-origin, nosniff HTML document.
  GURL cross_origin_url =
      GetURLOnCrossOriginServer("/site_isolation/nosniff.txt");
  const char* script_template = R"(
      fetch('%s', { mode: 'no-cors' })
          .then(response => response.text())
          .then(responseText => {
              domAutomationController.send(responseText);
          })
          .catch(error => {
              var errorMessage = 'error: ' + error;
              domAutomationController.send(errorMessage);
          }); )";
  std::string script =
      base::StringPrintf(script_template, cross_origin_url.spec().c_str());

  // The service worker will forward the request to the network, but a response
  // will be intercepted by the service worker and replaced with a new,
  // artificial error.
  base::HistogramTester histograms;
  std::string response;
  EXPECT_TRUE(ExecuteScriptAndExtractString(shell(), script, &response));

  // Verify that CORB blocked the response from the network (from
  // |cross_origin_https_server_|) to the service worker.
  InspectHistograms(histograms, kShouldBeBlockedWithoutSniffing, "network.txt",
                    RESOURCE_TYPE_XHR);

  // Verify that the service worker replied with an expected error.
  // Replying with an error means that CORB is only active once (for the
  // initial, real network request) and therefore the test doesn't get
  // confused (second successful response would have added noise to the
  // histograms captured by the test).
  EXPECT_EQ("error: TypeError: Failed to fetch", response);
}

// Test class to verify that --disable-web-security turns off CORB.
class CrossSiteDocumentBlockingDisableWebSecurityTest
    : public CrossSiteDocumentBlockingTestBase {
 public:
  CrossSiteDocumentBlockingDisableWebSecurityTest() {}
  ~CrossSiteDocumentBlockingDisableWebSecurityTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    CrossSiteDocumentBlockingTestBase::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingDisableWebSecurityTest);
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingDisableWebSecurityTest,
                       DisableBlocking) {
  // Load a page that issues illegal cross-site document requests.
  embedded_test_server()->StartAcceptingConnections();
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  bool was_blocked;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "sendRequest(\"valid.html\");", &was_blocked));
  EXPECT_FALSE(was_blocked);
}

// Test class to verify that documents are blocked for isolated origins as well.
class CrossSiteDocumentBlockingIsolatedOriginTest
    : public CrossSiteDocumentBlockingTestBase {
 public:
  CrossSiteDocumentBlockingIsolatedOriginTest() {}
  ~CrossSiteDocumentBlockingIsolatedOriginTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "http://bar.com");
    CrossSiteDocumentBlockingTestBase::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentBlockingIsolatedOriginTest);
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingIsolatedOriginTest,
                       BlockDocumentsFromIsolatedOrigin) {
  embedded_test_server()->StartAcceptingConnections();
  if (AreAllSitesIsolatedForTesting())
    return;

  // Load a page that issues illegal cross-site document requests to the
  // isolated origin.
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  bool was_blocked;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "sendRequest(\"valid.html\");", &was_blocked));
  EXPECT_TRUE(was_blocked);
}

}  // namespace content

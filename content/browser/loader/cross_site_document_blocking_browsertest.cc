// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {

using testing::Not;
using testing::HasSubstr;

namespace {

enum OrbExpectations {
  kShouldBeBlocked = 1 << 0,
  kShouldBeSniffed = 1 << 1,

  kBlockResourcesAsError = 1 << 2,

  kShouldBeAllowedWithoutSniffing = 0,
  kShouldBeBlockedWithoutSniffing = kShouldBeBlocked,
  kShouldBeSniffedAndAllowed = kShouldBeSniffed,
  kShouldBeSniffedAndBlocked = kShouldBeSniffed | kShouldBeBlocked,
};

constexpr OrbExpectations BlockAsError(const OrbExpectations expectations) {
  return OrbExpectations(expectations | kBlockResourcesAsError);
}

// Gets contents of a file at //content/test/data/<dir>/<file>.
std::string GetTestFileContents(const char* dir, const char* file) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath path = GetTestFilePath(dir, file);
  std::string result;
  EXPECT_TRUE(ReadFileToString(path, &result));
  return result;
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

    pending_test_client_remote_ = test_client_.CreateRemote();
  }

  ~RequestInterceptor() {
    WaitForCleanUpOnInterceptorThread(
        network::mojom::URLResponseHead::New(), "",
        network::URLLoaderCompletionStatus(net::ERR_NOT_IMPLEMENTED));
  }

  // No copy constructor or assignment operator.
  RequestInterceptor(const RequestInterceptor&) = delete;
  RequestInterceptor& operator=(const RequestInterceptor&) = delete;

  // Waits until a request gets intercepted and completed.
  void WaitForRequestCompletion() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!request_completed_);
    test_client_.RunUntilComplete();

    // Read the intercepted response body into |body_|.
    if (test_client_.completion_status().error_code == net::OK) {
      base::RunLoop run_loop;
      ReadBody(run_loop.QuitClosure());
      run_loop.Run();
    }

    // Wait until IO cleanup completes.
    WaitForCleanUpOnInterceptorThread(test_client_.response_head().Clone(),
                                      body_, test_client_.completion_status());

    // Mark the request as completed (for DCHECK purposes).
    request_completed_ = true;
  }

  const network::URLLoaderCompletionStatus& completion_status() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_completed_);
    return test_client_.completion_status();
  }

  const network::mojom::URLResponseHeadPtr& response_head() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_completed_);
    return test_client_.response_head();
  }

  const std::string& response_body() const {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(request_completed_);
    return body_;
  }

  void Verify(OrbExpectations expectations,
              const std::string& expected_resource_body) {
    if (0 != (expectations & kShouldBeBlocked)) {
      // Verify that the body is empty.
      EXPECT_EQ("", response_body());
      EXPECT_EQ(0, completion_status().decoded_body_length);

      // Verify that the console message would have been printed.
      EXPECT_TRUE(completion_status().should_report_orb_blocking);

      // Verify the response code & headers, which depends on whether the
      // response is blocked as an error, or as an empty response.
      if (0 != (expectations & kBlockResourcesAsError)) {
        ASSERT_EQ(net::ERR_BLOCKED_BY_ORB, completion_status().error_code);
        ASSERT_FALSE(response_head());
      } else {
        ASSERT_EQ(net::OK, completion_status().error_code);

        // Verify that response has no content.
        EXPECT_EQ(0u, response_head()->content_length);

        // Verify that response headers have been sanitized.
        size_t iter = 0;
        std::string name, value;
        EXPECT_FALSE(response_head()->headers->EnumerateHeaderLines(
            &iter, &name, &value));
        EXPECT_EQ(iter, 0u);
      }
    } else {
      ASSERT_EQ(net::OK, completion_status().error_code);
      EXPECT_FALSE(completion_status().should_report_orb_blocking);
      EXPECT_EQ(expected_resource_body, response_body());
    }
  }

  void InjectRequestInitiator(const url::Origin& request_initiator) {
    request_initiator_to_inject_ = request_initiator;
  }

  void InjectFetchMode(network::mojom::RequestMode request_mode) {
    request_mode_to_inject_ = request_mode;
  }

 private:
  void ReadBody(base::OnceClosure completion_callback) {
    std::string buffer(128, '\0');
    size_t actually_read_bytes = 0;
    MojoResult result = test_client_.response_body().ReadData(
        MOJO_READ_DATA_FLAG_NONE, base::as_writable_byte_span(buffer),
        actually_read_bytes);

    bool got_all_data = false;
    switch (result) {
      case MOJO_RESULT_OK:
        if (actually_read_bytes != 0) {
          body_ += buffer.substr(0, actually_read_bytes);
          got_all_data = false;
        } else {
          got_all_data = true;
        }
        break;
      case MOJO_RESULT_SHOULD_WAIT:
        // There is no data to be read or discarded (and the producer is still
        // open).
        got_all_data = false;
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        // The data pipe producer handle has been closed.
        got_all_data = true;
        break;
      default:
        CHECK(false) << "Unexpected mojo error: " << result;
        got_all_data = true;
        break;
    }

    if (!got_all_data) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&RequestInterceptor::ReadBody, base::Unretained(this),
                         std::move(completion_callback)));
    } else {
      std::move(completion_callback).Run();
    }
  }

  bool InterceptorCallback(URLLoaderInterceptor::RequestParams* params) {
    DCHECK(params);

    if (url_to_intercept_ != params->url_request.url) {
      return false;
    }

    // Prevent more than one intercept.
    if (request_intercepted_) {
      return false;
    }
    request_intercepted_ = true;
    interceptor_task_runner_ =
        base::SingleThreadTaskRunner::GetCurrentDefault();

    // Modify |params| if requested.
    if (request_initiator_to_inject_.has_value()) {
      params->url_request.request_initiator = request_initiator_to_inject_;
    }
    if (request_mode_to_inject_.has_value()) {
      params->url_request.mode = request_mode_to_inject_.value();
    }

    // Inject |test_client_| into the request.
    DCHECK(!original_client_);
    original_client_ = std::move(params->client);
    test_client_remote_.Bind(std::move(pending_test_client_remote_));
    test_client_receiver_ =
        std::make_unique<mojo::Receiver<network::mojom::URLLoaderClient>>(
            test_client_remote_.get(),
            params->client.BindNewPipeAndPassReceiver());

    // Forward the request to the original URLLoaderFactory.
    return false;
  }

  void WaitForCleanUpOnInterceptorThread(
      network::mojom::URLResponseHeadPtr response_head,
      std::string response_body,
      network::URLLoaderCompletionStatus status) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    if (cleanup_done_) {
      return;
    }

    if (interceptor_task_runner_) {
      base::RunLoop run_loop;
      interceptor_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&RequestInterceptor::CleanUpOnInterceptorThread,
                         base::Unretained(this), std::move(response_head),
                         response_body, status),
          run_loop.QuitClosure());
      run_loop.Run();
    }

    cleanup_done_ = true;
  }

  void CleanUpOnInterceptorThread(
      network::mojom::URLResponseHeadPtr response_head,
      std::string response_body,
      network::URLLoaderCompletionStatus status) {
    if (!request_intercepted_) {
      return;
    }

    // Tell the |original_client_| that the request has completed (and that it
    // can release its URLLoaderClient.
    if (status.error_code == net::OK) {
      mojo::ScopedDataPipeProducerHandle producer_handle;
      mojo::ScopedDataPipeConsumerHandle consumer_handle;
      ASSERT_EQ(mojo::CreateDataPipe(response_body.size() + 1, producer_handle,
                                     consumer_handle),
                MOJO_RESULT_OK);
      original_client_->OnReceiveResponse(
          std::move(response_head), std::move(consumer_handle), std::nullopt);

      EXPECT_EQ(MOJO_RESULT_OK, producer_handle->WriteAllData(
                                    base::as_byte_span(response_body)));
    }
    original_client_->OnComplete(status);

    // Reset all temporary mojo bindings.
    original_client_.reset();
    test_client_receiver_.reset();
    test_client_remote_.reset();
  }

  const GURL url_to_intercept_;
  URLLoaderInterceptor interceptor_;

  std::optional<url::Origin> request_initiator_to_inject_;
  std::optional<network::mojom::RequestMode> request_mode_to_inject_;

  // |pending_test_client_remote_| below is used to transition results of
  // |test_client_.CreateRemote()| into IO thread.
  mojo::PendingRemote<network::mojom::URLLoaderClient>
      pending_test_client_remote_;

  // UI thread state:
  network::TestURLLoaderClient test_client_;
  std::string body_;
  bool request_completed_ = false;
  bool cleanup_done_ = false;

  // Interceptor thread state:
  mojo::Remote<network::mojom::URLLoaderClient> original_client_;
  bool request_intercepted_ = false;
  scoped_refptr<base::SingleThreadTaskRunner> interceptor_task_runner_;
  mojo::Remote<network::mojom::URLLoaderClient> test_client_remote_;
  std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderClient>>
      test_client_receiver_;
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

  // No copy constructor or assignment.
  CrossSiteDocumentBlockingTestBase(const CrossSiteDocumentBlockingTestBase&) =
      delete;
  CrossSiteDocumentBlockingTestBase& operator=(
      const CrossSiteDocumentBlockingTestBase&) = delete;

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
};

struct ImgTestParams {
  const char* resource;
  OrbExpectations expectations;
};
class CrossSiteDocumentBlockingImgElementTest
    : public CrossSiteDocumentBlockingTestBase,
      public testing::WithParamInterface<ImgTestParams> {
 public:
  CrossSiteDocumentBlockingImgElementTest() = default;
  ~CrossSiteDocumentBlockingImgElementTest() override = default;

  void VerifyImgRequest(std::string resource, OrbExpectations expectations) {
    // Test from a http: origin.
    VerifyImgRequest(resource, expectations,
                     GURL("http://foo.com/title1.html"));

    // Test from a file: origin.
    VerifyImgRequest(resource, expectations,
                     GetTestUrl(nullptr, "title1.html"));
  }

  void VerifyImgRequest(std::string resource,
                        OrbExpectations expectations,
                        GURL page_url) {
    GURL resource_url(
        std::string("http://cross-origin.com/site_isolation/" + resource));
    SCOPED_TRACE(
        base::StringPrintf("... while testing via <img src='%s'> from %s",
                           resource_url.spec().c_str(),
                           url::Origin::Create(page_url).Serialize().c_str()));

    // Navigate to the test page while request interceptor is active.
    RequestInterceptor interceptor(resource_url);
    EXPECT_TRUE(NavigateToURL(shell(), page_url));

    // Make sure that base::HistogramTester below starts with a clean slate.
    FetchHistogramsFromChildProcesses();

    // Issue the request that will be intercepted.
    base::HistogramTester histograms;
    const char kScriptTemplate[] = R"(
        var img = document.createElement('img');
        img.src = $1;
        document.body.appendChild(img); )";
    EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, resource_url)));
    interceptor.WaitForRequestCompletion();

    // Verify...
    interceptor.Verify(expectations,
                       GetTestFileContents("site_isolation", resource.c_str()));
  }
};

IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingImgElementTest, Test) {
  embedded_test_server()->StartAcceptingConnections();

  std::string resource = GetParam().resource;
  OrbExpectations expectations = GetParam().expectations;

  base::HistogramTester histograms;
  VerifyImgRequest(resource, expectations);
}

// TODO(vogelheim): Consider re-writing these as regular tests, rather than
// as a parameterized test suite.
#define IMG_TEST(tag, resource, expectations)       \
  INSTANTIATE_TEST_SUITE_P(                         \
      tag, CrossSiteDocumentBlockingImgElementTest, \
      ::testing::Values(ImgTestParams{resource, BlockAsError(expectations)}));

// The following are files under content/test/data/site_isolation. All
// should be disallowed for cross site XHR under the document blocking policy.
//   valid.*        - Correctly labeled HTML/XML/JSON files.
//   *.txt          - Plain text that sniffs as HTML, XML, or JSON.
//   htmlN_dtd.*    - Various HTML templates to test.
//   json-prefixed* - parser-breaking prefixes
IMG_TEST(valid_html, "valid.html", kShouldBeSniffedAndBlocked)
IMG_TEST(valid_xml, "valid.xml", kShouldBeSniffedAndBlocked)
IMG_TEST(valid_json, "valid.json", kShouldBeSniffedAndBlocked)
IMG_TEST(html_txt, "html.txt", kShouldBeSniffedAndBlocked)
IMG_TEST(xml_txt, "xml.txt", kShouldBeSniffedAndBlocked)
IMG_TEST(json_txt, "json.txt", kShouldBeSniffedAndBlocked)
IMG_TEST(json_octet_stream, "json.octet-stream", kShouldBeSniffedAndBlocked)
IMG_TEST(comment_valid_html, "comment_valid.html", kShouldBeSniffedAndBlocked)
IMG_TEST(html4_dtd_html, "html4_dtd.html", kShouldBeSniffedAndBlocked)
IMG_TEST(html4_dtd_txt, "html4_dtd.txt", kShouldBeSniffedAndBlocked)
IMG_TEST(html5_dtd_html, "html5_dtd.html", kShouldBeSniffedAndBlocked)
IMG_TEST(html5_dtd_txt, "html5_dtd.txt", kShouldBeSniffedAndBlocked)
IMG_TEST(json_js, "json.js", kShouldBeSniffedAndBlocked)
IMG_TEST(json_prefixed_1_js, "json-prefixed-1.js", kShouldBeSniffedAndBlocked)
IMG_TEST(json_prefixed_2_js, "json-prefixed-2.js", kShouldBeSniffedAndBlocked)
IMG_TEST(json_prefixed_3_js, "json-prefixed-3.js", kShouldBeSniffedAndBlocked)
IMG_TEST(json_prefixed_4_js, "json-prefixed-4.js", kShouldBeSniffedAndBlocked)
IMG_TEST(nosniff_json_js, "nosniff.json.js", kShouldBeSniffedAndBlocked)
IMG_TEST(nosniff_json_prefixed_js,
         "nosniff.json-prefixed.js",
         kShouldBeSniffedAndBlocked)

// ORB blocks responses with non-image/audio/video MIME type that don't
// sniff as Javascript (ORBv0.1 blocks ones that sniff as HTML or XML or JSON).
IMG_TEST(html_octet_stream, "html.octet-stream", kShouldBeSniffedAndBlocked)
IMG_TEST(xml_octet_stream, "xml.octet-stream", kShouldBeSniffedAndBlocked)

// ORB detects audio/video responses before the final Javascript sniffing (or in
// the case of ORBv0.1, confirmation sniffing for HTML/XML/JSON).  This
// sequencing simplifies the ORB algorithm and implementation because it means
// that the final sniffing step doesn't need to take into account media content.
// OTOH, if ORB detects some audio/video responses based on the MIME type of the
// response, then it may allow some cases that ORB would block - such as a JSON
// response incorrectly labeled as "audio/x-wav".
IMG_TEST(json_wav, "json.wav", kShouldBeSniffedAndAllowed)

// These files should be disallowed without sniffing.
//   nosniff.*   - Won't sniff correctly, but blocked because of nosniff.
IMG_TEST(nosniff_html, "nosniff.html", kShouldBeBlockedWithoutSniffing)
IMG_TEST(nosniff_xml, "nosniff.xml", kShouldBeBlockedWithoutSniffing)
IMG_TEST(nosniff_json, "nosniff.json", kShouldBeBlockedWithoutSniffing)
IMG_TEST(nosniff_txt, "nosniff.txt", kShouldBeBlockedWithoutSniffing)
IMG_TEST(fake_pdf, "fake.pdf", kShouldBeBlockedWithoutSniffing)
IMG_TEST(fake_zip, "fake.zip", kShouldBeBlockedWithoutSniffing)

// These files are allowed for XHR under the document blocking policy because
// the sniffing logic determines they are not actually documents.
//   *js.*   - JavaScript mislabeled as a document.
//   jsonp.* - JSONP (i.e., script) mislabeled as a document.
//   img.*   - Contents that won't match the document label.
//   valid.* - Correctly labeled responses of non-document types.
IMG_TEST(html_prefix_txt, "html-prefix.txt", kShouldBeSniffedAndAllowed)
IMG_TEST(js_html, "js.html", kShouldBeSniffedAndAllowed)
IMG_TEST(comment_js_html, "comment_js.html", kShouldBeSniffedAndAllowed)
IMG_TEST(js_xml, "js.xml", kShouldBeSniffedAndAllowed)
IMG_TEST(js_json, "js.json", kShouldBeSniffedAndAllowed)
IMG_TEST(js_txt, "js.txt", kShouldBeSniffedAndAllowed)
IMG_TEST(jsonp_html, "jsonp.html", kShouldBeSniffedAndAllowed)
IMG_TEST(jsonp_xml, "jsonp.xml", kShouldBeSniffedAndAllowed)
IMG_TEST(jsonp_json, "jsonp.json", kShouldBeSniffedAndAllowed)
IMG_TEST(jsonp_txt, "jsonp.txt", kShouldBeSniffedAndAllowed)
IMG_TEST(img_html, "img.html", kShouldBeSniffedAndAllowed)
IMG_TEST(img_xml, "img.xml", kShouldBeSniffedAndAllowed)
IMG_TEST(img_json, "img.json", kShouldBeSniffedAndAllowed)
IMG_TEST(img_txt, "img.txt", kShouldBeSniffedAndAllowed)
IMG_TEST(valid_js, "valid.js", kShouldBeSniffedAndAllowed)
IMG_TEST(json_list_js, "json-list.js", kShouldBeSniffedAndAllowed)
IMG_TEST(nosniff_json_list_js,
         "nosniff.json-list.js",
         kShouldBeSniffedAndAllowed)
IMG_TEST(js_html_polyglot_html,
         "js-html-polyglot.html",
         kShouldBeSniffedAndAllowed)
IMG_TEST(js_html_polyglot2_html,
         "js-html-polyglot2.html",
         kShouldBeSniffedAndAllowed)

// ORB allows, because even with 'XCTO: nosniff' ORB will sniff that this is an
// image.
IMG_TEST(nosniff_png, "nosniff.png.octet-stream", kShouldBeSniffedAndAllowed)
// Like nosniff_png above, except that ORB v0.1 allows because HLS/m3u8 doesn't
// sniff as HTML/XML/JSON.
IMG_TEST(m3u8_octet_stream, "m3u8.octet-stream", kShouldBeSniffedAndAllowed)

// This test covers an aspect of Cross-Origin-Resource-Policy (CORP, different
// from ORB) that cannot be covered by wpt/fetch/cross-origin-resource-policy:
// whether blocking occurs *before* the response reaches the renderer process.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       CrossOriginResourcePolicy) {
  embedded_test_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  GURL resource_url("http://cross-origin.com/site_isolation/png-corp.png");
  RequestInterceptor interceptor(resource_url);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://foo.com/title1.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = R"(
      var img = document.createElement('img');
      img.src = $1;
      document.body.appendChild(img); )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, resource_url)));
  interceptor.WaitForRequestCompletion();

  // Verify that Cross-Origin-Resource-Policy blocked the response before it
  // reached the renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

// TODO(crbug.com/40269364): Remove support for old header names once API users
// have switched.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       FledgeAuctionOnlySignalsNotReadableFromFetch) {
  embedded_test_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  // Note that even same origin requests are blocked.
  GURL resource_url("http://foo.com/interest_group/auction_only.json");
  RequestInterceptor interceptor(resource_url);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://foo.com/title1.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = "fetch($1)";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, resource_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  interceptor.WaitForRequestCompletion();

  // Verify that X-FLEDGE-Auction-Only blocked the response before it
  // reached the renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       FledgeAuctionOnlySignalsNotReadableFromFetchNewName) {
  embedded_test_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  // Note that even same origin requests are blocked.
  GURL resource_url("http://foo.com/interest_group/auction_only_new_name.json");
  RequestInterceptor interceptor(resource_url);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://foo.com/title1.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = "fetch($1)";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, resource_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  interceptor.WaitForRequestCompletion();

  // Verify that Ad-Auction-Only blocked the response before it reached the
  // renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

IN_PROC_BROWSER_TEST_F(
    CrossSiteDocumentBlockingTestBase,
    FledgeAuctionOnlySignalsNotReadableFromFetchBothNewAndOldNames) {
  embedded_test_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  // Note that even same origin requests are blocked.
  GURL resource_url(
      "http://foo.com/interest_group/auction_only_both_new_and_old_names.json");
  RequestInterceptor interceptor(resource_url);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("http://foo.com/title1.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = "fetch($1)";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, resource_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  interceptor.WaitForRequestCompletion();

  // Verify that Ad-Auction-Only and X-FLEDGE-Auction-Only blocked the response
  // before it reached the renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase, AllowCorsFetches) {
  embedded_test_server()->StartAcceptingConnections();
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // These files should be allowed for XHR under the document blocking policy.
  //   cors.*  - Correctly labeled documents with valid CORS headers.
  const char* allowed_resources[] = {"cors.html", "cors.xml", "cors.json",
                                     "cors.txt"};
  for (const char* resource : allowed_resources) {
    SCOPED_TRACE(base::StringPrintf("... while testing page: %s", resource));

    // Make sure that base::HistogramTester below starts with a clean slate.
    FetchHistogramsFromChildProcesses();

    base::HistogramTester histograms;
    // Fetch and verify results of the fetch.
    EXPECT_EQ(
        false,
        EvalJs(shell(), base::StringPrintf(
                            "sendRequest('http://bar.com/site_isolation/%s');",
                            resource)));
  }
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       AllowSameOriginFetchFromLoadDataWithBaseUrl) {
  embedded_test_server()->StartAcceptingConnections();

  // LoadDataWithBaseURL is never subject to --site-per-process policy today
  // (this API is only used by Android WebView [where OOPIFs have not shipped
  // yet] and GuestView cases [which always hosts guests inside a renderer
  // without an origin lock]).  Therefore, skip the test in --site-per-process
  // mode to avoid renderer kills which won't happen in practice as described
  // above.
  //
  // TODO(crbug.com/40627228): Consider enabling this test once Android
  // Webview or WebView guests support OOPIFs and/or origin locks.
  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Navigate via LoadDataWithBaseURL.
  const GURL base_url("http://foo.com");
  const std::string data = "<html><body>foo</body></html>";
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);
  TestNavigationObserver nav_observer(shell()->web_contents(), 1);
  shell()->LoadDataWithBaseURL(base_url /* history_url */, data, base_url);
  nav_observer.Wait();

  // Fetch a same-origin resource.
  GURL resource_url("http://foo.com/site_isolation/nosniff.html");
  EXPECT_EQ(
      url::Origin::Create(resource_url),
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  FetchHistogramsFromChildProcesses();
  base::HistogramTester histograms;
  std::string fetch_result =
      EvalJs(shell(), JsReplace("fetch($1).then(response => response.text())",
                                resource_url))
          .ExtractString();
  fetch_result = std::string(TrimWhitespaceASCII(fetch_result, base::TRIM_ALL));

  // Verify that the response was not blocked.
  EXPECT_EQ("runMe({ \"name\" : \"chromium\" });", fetch_result);
}

// Regression test for https://crbug.com/958421.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase, BackToAboutBlank) {
  embedded_test_server()->StartAcceptingConnections();

  // Prepare to verify results of a fetch.
  GURL resource_url("http://foo.com/title2.html");
  const char kFetchScriptTemplate[] = R"(
      fetch($1, {mode: 'no-cors'}).then(response => 'ok');
  )";
  std::string fetch_script = JsReplace(kFetchScriptTemplate, resource_url);

  // Navigate to the test page and open a popup via |window.open|.
  GURL initial_url("http://foo.com/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));
  WebContentsAddedObserver popup_observer;
  ASSERT_TRUE(ExecJs(shell(), "var popup = window.open('title1.html')"));
  WebContents* popup = popup_observer.GetWebContents();
  EXPECT_TRUE(WaitForLoadStop(popup));
  EXPECT_EQ(initial_url, popup->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Navigate the popup to about:blank. Note that we didn't directly
  // window.open() to about:blank because otherwise the about:blank page will
  // always get replaced on the next navigation.
  {
    TestNavigationObserver nav_observer(popup);
    EXPECT_TRUE(ExecJs(popup, "location.href = 'about:blank';"));
    nav_observer.Wait();
    EXPECT_EQ(2, popup->GetController().GetEntryCount());
    EXPECT_EQ(GURL(url::kAboutBlankURL),
              popup->GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(shell()
                  ->web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetLastCommittedOrigin(),
              popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }

  // Verify that ORB doesn't block same-origin request from the popup.
  {
    FetchHistogramsFromChildProcesses();
    base::HistogramTester histograms;
    ASSERT_EQ("ok", EvalJs(popup, fetch_script));
  }

  // Navigate the popup and then go back to the 'about:blank' URL.
  TestNavigationObserver nav_observer(popup);
  ASSERT_TRUE(ExecJs(shell(), "popup.location.href = '/title3.html'"));
  nav_observer.WaitForNavigationFinished();
  TestNavigationObserver back_observer(popup);
  ASSERT_TRUE(ExecJs(shell(), "popup.history.back()"));
  back_observer.WaitForNavigationFinished();
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            popup->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(
      shell()->web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
      popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(url::Origin::Create(resource_url),
            popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Verify that ORB doesn't block same-origin request from the popup.
  {
    FetchHistogramsFromChildProcesses();
    base::HistogramTester histograms;
    ASSERT_EQ("ok", EvalJs(popup, fetch_script));
  }
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       BlockForVariousTargets) {
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

// Checks to see that ORB blocking applies to processes hosting error pages.
// Regression test for https://crbug.com/814913.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
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

  // Add a <script> tag whose src is a ORB-protected resource. Expect no
  // window.onerror to result, because no syntax error is generated by the empty
  // response.
  std::string script = R"((subresource_url => {
    return new Promise(resolve => {
      window.onerror = () => resolve("ORB BYPASSED");
      var script = document.createElement('script');
      script.src = subresource_url;
      script.onerror = () => resolve("ORB WORKED");
      document.body.appendChild(script);
    });
    }))";

  EXPECT_EQ("ORB WORKED",
            EvalJs(shell(), script + "('" + subresource_url.spec() + "')"));
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       HeadersBlockedInResponseBlockedByCorb) {
  embedded_test_server()->StartAcceptingConnections();

  // Prepare to intercept the network request at the IPC layer.
  // This has to be done before the RenderFrameHostImpl is created.
  //
  // Note: we want to verify that the blocking prevents the data from being sent
  // over IPC.  Testing later (e.g. via Response/Headers Web APIs) might give a
  // false sense of security, since some sanitization happens inside the
  // renderer (e.g. via FetchResponseData::CreateCorsFilteredResponse).
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
  interceptor.Verify(BlockAsError(kShouldBeBlockedWithoutSniffing),
                     "no resource body needed for blocking verification");

  // Verify that the error response has no head and no body.
  EXPECT_FALSE(interceptor.response_head());
  EXPECT_EQ("", interceptor.response_body());
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       HeadersSanitizedInCrossOriginResponseAllowedByCorb) {
  embedded_test_server()->StartAcceptingConnections();

  // Prepare to intercept the network request at the IPC layer.
  // This has to be done before the RenderFrameHostImpl is created.
  //
  // Note: we want to verify that the blocking prevents the data from being sent
  // over IPC.  Testing later (e.g. via Response/Headers Web APIs) might give a
  // false sense of security, since some sanitization happens inside the
  // renderer (e.g. via FetchResponseData::CreateCorsFilteredResponse).
  GURL bar_url("http://bar.com/cross_site_document_blocking/headers-test.png");
  RequestInterceptor interceptor(bar_url);
  std::string png_body =
      GetTestFileContents("cross_site_document_blocking", "headers-test.png");

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

  // Verify that the response completed successfully, was *not* blocked and
  // returned the full `png_body`.
  interceptor.Verify(kShouldBeSniffedAndAllowed, png_body);

  // Verify that most response headers have been allowed by ORB.
  const std::string& headers =
      interceptor.response_head()->headers->raw_headers();
  EXPECT_THAT(headers, HasSubstr("Cache-Control"));
  EXPECT_THAT(headers, HasSubstr("Content-Length"));
  EXPECT_THAT(headers, HasSubstr("Content-Type"));
  EXPECT_THAT(headers, HasSubstr("Expires"));
  EXPECT_THAT(headers, HasSubstr("Last-Modified"));
  EXPECT_THAT(headers, HasSubstr("Pragma"));
  EXPECT_THAT(headers, HasSubstr("X-Content-Type-Options"));
  EXPECT_THAT(headers, HasSubstr("X-My-Secret-Header"));

  // Verify that the body has been allowed by ORB.
  EXPECT_EQ(png_body, interceptor.response_body());
  EXPECT_EQ(static_cast<int64_t>(png_body.size()),
            interceptor.completion_status().decoded_body_length);
  EXPECT_EQ(static_cast<int64_t>(png_body.size()),
            interceptor.response_head()->content_length);

  // MAIN VERIFICATION: Verify that despite allowing the response in ORB, we
  // stripped out the cookies (i.e. the cookies present in
  // cross_site_document_blocking/headers-test.png.mock-http-headers).
  //
  // This verification helps ensure that no cross-origin secrets are disclosed
  // in no-cors responses.
  EXPECT_THAT(headers, Not(HasSubstr("MySecretPlainCookieKey")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretCookieValue1")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretHttpOnlyCookieKey")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretCookieValue2")));
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       HeadersSanitizedInSameOriginResponseAllowedByCorb) {
  embedded_test_server()->StartAcceptingConnections();

  // Prepare to intercept the network request at the IPC layer.
  // This has to be done before the RenderFrameHostImpl is created.
  //
  // Note: we want to verify that the blocking prevents the data from being sent
  // over IPC.  Testing later (e.g. via Response/Headers Web APIs) might give a
  // false sense of security, since some sanitization happens inside the
  // renderer (e.g. via FetchResponseData::CreateCorsFilteredResponse).
  GURL foo_resource_url(
      "http://foo.com/cross_site_document_blocking/headers-test.png");
  RequestInterceptor interceptor(foo_resource_url);
  std::string png_body =
      GetTestFileContents("cross_site_document_blocking", "headers-test.png");

  // Navigate to the test page.
  GURL foo_url("http://foo.com/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = R"(
      var img = document.createElement('img');
      img.src = $1;
      document.body.appendChild(img); )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, foo_resource_url)));
  interceptor.WaitForRequestCompletion();

  // Verify that the response completed successfully, was blocked and was logged
  // as having initially a non-empty body.
  interceptor.Verify(kShouldBeSniffedAndAllowed, png_body);

  // Verify that most response headers have been allowed by ORB.
  const std::string& headers =
      interceptor.response_head()->headers->raw_headers();
  EXPECT_THAT(headers, HasSubstr("Cache-Control"));
  EXPECT_THAT(headers, HasSubstr("Content-Length"));
  EXPECT_THAT(headers, HasSubstr("Content-Type"));
  EXPECT_THAT(headers, HasSubstr("Expires"));
  EXPECT_THAT(headers, HasSubstr("Last-Modified"));
  EXPECT_THAT(headers, HasSubstr("Pragma"));
  EXPECT_THAT(headers, HasSubstr("X-Content-Type-Options"));
  EXPECT_THAT(headers, HasSubstr("X-My-Secret-Header"));

  // Verify that the body has been allowed by ORB.
  EXPECT_EQ(png_body, interceptor.response_body());
  EXPECT_EQ(static_cast<int64_t>(png_body.size()),
            interceptor.completion_status().decoded_body_length);
  EXPECT_EQ(static_cast<int64_t>(png_body.size()),
            interceptor.response_head()->content_length);

  // MAIN VERIFICATION: Verify that despite allowing the response in ORB, we
  // stripped out the cookies (i.e. the cookies present in
  // cross_site_document_blocking/headers-test.png.mock-http-headers).
  //
  // No security boundary is crossed in this test case (since this is a
  // same-origin response), but for consistency we want to ensure that cookies
  // are stripped in all IPCs.
  EXPECT_THAT(headers, Not(HasSubstr("MySecretPlainCookieKey")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretCookieValue1")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretHttpOnlyCookieKey")));
  EXPECT_THAT(headers, Not(HasSubstr("MySecretCookieValue2")));
}

// TODO(lukasza): https://crbug.com/154571: Enable this test on Android once
// SharedWorkers are also enabled on Android.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase, SharedWorker) {
  embedded_test_server()->StartAcceptingConnections();

  // Prepare to intercept the network request at the IPC layer.
  // This has to be done before the SharedWorkerHost is created.
  GURL bar_url("http://bar.com/site_isolation/nosniff.json");
  RequestInterceptor interceptor(bar_url);

  // Navigate to the test page.
  GURL foo_url("http://foo.com/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Start a shared worker and wait until it says that it is ready.
  const char kWorkerScriptTemplate[] = R"(
      onconnect = function(e) {
        const port = e.ports[0];

        port.addEventListener('message', function(e) {
          url = e.data;
          fetch(url, {mode: 'no-cors'})
              .then(_ => port.postMessage('FETCH SUCCEEDED'))
              .catch(e => port.postMessage('FETCH ERROR: ' + e));
        });

        port.start();
        port.postMessage('WORKER READY');
      };
  )";
  std::string worker_script =
      base::Base64Encode(JsReplace(kWorkerScriptTemplate, bar_url));
  const char kWorkerStartTemplate[] = R"(
      new Promise(function (resolve, reject) {
          const worker_url = 'data:application/javascript;base64,' + $1;
          window.myWorker = new SharedWorker(worker_url);
          window.myWorkerMessageHandler = resolve;
          window.myWorker.port.onmessage = function(e) {
              window.myWorkerMessageHandler(e.data);
          };
      });
  )";
  EXPECT_EQ("WORKER READY",
            EvalJs(shell(), JsReplace(kWorkerStartTemplate, worker_script)));

  // Make sure that base::HistogramTester below starts with a clean slate.
  FetchHistogramsFromChildProcesses();
  base::HistogramTester histograms;

  // Ask the shared worker to perform a cross-origin fetch.
  const char kFetchStartTemplate[] = R"(
      const fetch_url = $1;
      window.myWorkerMessageHandler = function(data) {
          window.myWorkerResult = data;
      }
      window.myWorker.port.postMessage(fetch_url);
  )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kFetchStartTemplate, bar_url)));

  interceptor.WaitForRequestCompletion();
  interceptor.Verify(kShouldBeBlockedWithoutSniffing,
                     "no resource body needed for blocking verification");

  // Wait for fetch result (really needed only without NetworkService, if no
  // interceptor.WaitForRequestCompletion was called above).
  const char kFetchWait[] = R"(
      new Promise(function (resolve, reject) {
          if (window.myWorkerResult) {
            resolve(window.myWorkerResult);
            return;
          }
          window.myWorkerMessageHandler = resolve;
      });
  )";
  EXPECT_EQ("FETCH SUCCEEDED", EvalJs(shell(), kFetchWait));
}
#endif  // !BUILDFLAG(IS_ANDROID)

// https://crbug.com/1218723 this is broken by SplitCacheByNetworkIsolationKey.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingTestBase,
                       DISABLED_PrefetchIsNotImpacted) {
  // Prepare for intercepting the resource request for testing prefetching.
  const char* kPrefetchResourcePath = "/prefetch-test";
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      kPrefetchResourcePath);

  // Navigate to a webpage containing a cross-origin frame.
  embedded_test_server()->StartAcceptingConnections();
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Make sure that base::HistogramTester below starts with a clean slate.
  FetchHistogramsFromChildProcesses();

  // Inject a cross-origin <link rel="prefetch" ...> into the main frame.
  FetchHistogramsFromChildProcesses();
  base::HistogramTester histograms;
  static constexpr char kPrefetchInjectionScriptTemplate[] = R"(
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
      kPrefetchInjectionScriptTemplate, kPrefetchResourcePath);
  EXPECT_TRUE(ExecJs(shell()->web_contents(), prefetch_injection_script));

  // Respond to the prefetch request in a way that:
  // 1) will enable caching
  // 2) won't finish until after ORB has blocked the response.
  std::string response_bytes =
      "HTTP/1.1 200 OK\r\n"
      "Cache-Control: public, max-age=10\r\n"
      "Content-Type: text/html\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "\r\n"
      "<p>contents of the response</p>";
  response.WaitForRequest();
  response.Send(response_bytes);

  // Verify that ORB blocked the response.
  std::string wait_script = R"(
      new Promise(resolve => {
        function notify_prefetch_is_done() { resolve(123); }

        if (window.is_prefetch_done) {
          // Can notify immediately if |window.is_prefetch_done| has already
          // been set by |prefetch_injection_script|.
          notify_prefetch_is_done();
        } else {
          // Otherwise wait for ORB's empty response to reach the renderer.
          link = document.getElementsByTagName('link')[0];
          link.onerror = notify_prefetch_is_done;
        }
      });
  )";
  EXPECT_EQ(123, EvalJs(shell()->web_contents(), wait_script));

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
  // ORB blocking).
  static constexpr char kFetchScriptTemplate[] = R"(
      fetch('%s')
          .then(response => response.text())
          .catch(error => {
              var errorMessage = 'error: ' + error;
              console.log(errorMessage);
              return errorMessage;
          }); )";
  std::string fetch_script =
      base::StringPrintf(kFetchScriptTemplate, kPrefetchResourcePath);
  EXPECT_EQ("<p>contents of the response</p>",
            EvalJs(ChildFrameAt(shell()->web_contents(), 0), fetch_script));
}

// This test class sets up a service worker that can be used to try to respond
// to same-origin requests with cross-origin responses.
class CrossSiteDocumentBlockingServiceWorkerTest : public ContentBrowserTest {
 public:
  CrossSiteDocumentBlockingServiceWorkerTest()
      : service_worker_https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        cross_origin_https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~CrossSiteDocumentBlockingServiceWorkerTest() override {}

  // No copy constructor or assignment operator.
  CrossSiteDocumentBlockingServiceWorkerTest(
      const CrossSiteDocumentBlockingServiceWorkerTest&) = delete;
  CrossSiteDocumentBlockingServiceWorkerTest& operator=(
      const CrossSiteDocumentBlockingServiceWorkerTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    SetupCrossSiteRedirector(embedded_test_server());

    service_worker_https_server_.ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    ASSERT_TRUE(service_worker_https_server_.Start());

    cross_origin_https_server_.ServeFilesFromSourceDirectory(
        GetTestDataFilePath());
    cross_origin_https_server_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ASSERT_TRUE(cross_origin_https_server_.Start());

    // Sanity check of test setup - the 2 https servers should be cross-site
    // (the second server should have a different hostname because of the call
    // to SetSSLConfig with CERT_COMMON_NAME_IS_DOMAIN argument).
    ASSERT_FALSE(net::registry_controlled_domains::SameDomainOrHost(
        GetURLOnServiceWorkerServer("/"), GetURLOnCrossOriginServer("/"),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
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
    std::string script = R"(
        navigator.serviceWorker
            .register('/cross_site_document_blocking/service_worker.js')
            .then(registration => navigator.serviceWorker.ready)
            .then(function(r) { return true; })
            .catch(function(e) {
                console.log('error: ' + e);
                return false;
            }); )";
    ASSERT_EQ(true, EvalJs(shell(), script));

    // Navigate again to the same URL - the service worker should be 1) active
    // at this time (because of waiting for |navigator.serviceWorker.ready|
    // above) and 2) controlling the current page (because of the reload).
    ASSERT_TRUE(NavigateToURL(shell(), url));
    ASSERT_EQ(true, EvalJs(shell(), "!!navigator.serviceWorker.controller"));
  }

 private:
  // The test requires 2 https servers, because:
  // 1. Service workers are only supported on secure origins.
  // 2. One of tests requires fetching cross-origin resources from the
  //    original page and/or service worker - the target of the fetch needs to
  //    be a https server to avoid hitting the mixed content error.
  net::EmbeddedTestServer service_worker_https_server_;
  net::EmbeddedTestServer cross_origin_https_server_;
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingServiceWorkerTest,
                       NetworkToServiceWorkerResponse) {
  SetUpServiceWorker();

  // Make sure that the histograms generated by a service worker registration
  // have been recorded.
  FetchHistogramsFromChildProcesses();

  // Build a script for XHR-ing a cross-origin, nosniff HTML document.
  GURL cross_origin_url =
      GetURLOnCrossOriginServer("/site_isolation/nosniff.txt");
  static constexpr char kScriptTemplate[] = R"(
      fetch('%s', { mode: 'no-cors' })
          .then(response => response.text())
          .catch(error => {
              var errorMessage = 'error: ' + error;
              return errorMessage;
          }); )";
  std::string script =
      base::StringPrintf(kScriptTemplate, cross_origin_url.spec().c_str());

  // Make sure that base::HistogramTester below starts with a clean slate.
  FetchHistogramsFromChildProcesses();

  // The service worker will forward the request to the network, but a response
  // will be intercepted by the service worker and replaced with a new,
  // artificial error.
  base::HistogramTester histograms;
  std::string response = EvalJs(shell(), script).ExtractString();

  // Verify that the service worker replied with an expected error.
  // Replying with an error means that ORB is only active once (for the
  // initial, real network request) and therefore the test doesn't get
  // confused (second successful response would have added noise to the
  // histograms captured by the test).
  EXPECT_EQ("error: TypeError: Failed to fetch", response);
}

// Test class to verify that --disable-web-security turns off ORB.
class CrossSiteDocumentBlockingDisableWebSecurityTest
    : public CrossSiteDocumentBlockingTestBase {
 public:
  CrossSiteDocumentBlockingDisableWebSecurityTest() = default;
  ~CrossSiteDocumentBlockingDisableWebSecurityTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    CrossSiteDocumentBlockingTestBase::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingDisableWebSecurityTest,
                       DisableBlocking) {
  // Load a page that issues illegal cross-site document requests.
  embedded_test_server()->StartAcceptingConnections();
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  ASSERT_EQ(
      false,
      EvalJs(shell(),
             "sendRequest(\"http://bar.com/site_isolation/valid.html\");"));
}

// Test class to verify that documents are blocked for sandboxed iframes (opaque
// origins) as well.
class CrossSiteDocumentBlockingSandboxedIframeTest
    : public CrossSiteDocumentBlockingTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  CrossSiteDocumentBlockingSandboxedIframeTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(
          blink::features::kIsolateSandboxedIframes);
    } else {
      feature_list_.InitAndDisableFeature(
          blink::features::kIsolateSandboxedIframes);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that documents are blocked from an opaque origin in a sandboxed iframe
// whose precursor is the same as the parent frame's origin.
IN_PROC_BROWSER_TEST_P(CrossSiteDocumentBlockingSandboxedIframeTest,
                       BlockDocumentsFromOpaqueOriginInSameOriginEmbedder) {
  embedded_test_server()->StartAcceptingConnections();
  // Load frame at foo.com, then use it to load a same-origin sandboxed iframe
  // with request.html.
  GURL foo_url("http://foo.com/title1.html");
  GURL sandboxed_frame_url(
      "http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  TestNavigationObserver observer(shell()->web_contents());
  const char kScriptTemplate[] = R"(
      const frm = document.createElement('iframe');
      frm.sandbox='allow-scripts';
      frm.src = $1;
      document.body.append(frm);
  )";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, sandboxed_frame_url)));
  observer.Wait();
  EXPECT_EQ(sandboxed_frame_url, observer.last_navigation_url());

  RenderFrameHost* child = ChildFrameAt(shell(), 0);
  ASSERT_TRUE(child);
  ASSERT_TRUE(child->GetLastCommittedOrigin().opaque());

  ASSERT_EQ(
      true,
      EvalJs(child,
             "sendRequest(\"http://foo.com/site_isolation/valid.html\");"));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CrossSiteDocumentBlockingSandboxedIframeTest,
                         ::testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param
                                      ? "kIsolateSandboxedIframesEnabled"
                                      : "kIsolateSandboxedIframesDisabled";
                         });

// Test class to verify that documents are blocked for isolated origins as well.
class CrossSiteDocumentBlockingIsolatedOriginTest
    : public CrossSiteDocumentBlockingTestBase {
 public:
  CrossSiteDocumentBlockingIsolatedOriginTest() = default;
  ~CrossSiteDocumentBlockingIsolatedOriginTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    "http://bar.com");
    CrossSiteDocumentBlockingTestBase::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingIsolatedOriginTest,
                       BlockDocumentsFromIsolatedOrigin) {
  embedded_test_server()->StartAcceptingConnections();

  if (AreAllSitesIsolatedForTesting()) {
    return;
  }

  // Load a page that issues illegal cross-site document requests to the
  // isolated origin.
  GURL foo_url("http://foo.com/cross_site_document_blocking/request.html");
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  ASSERT_EQ(
      true,
      EvalJs(shell(),
             "sendRequest(\"http://bar.com/site_isolation/valid.html\");"));
}

IN_PROC_BROWSER_TEST_F(ContentBrowserTest, CorpVsBrowserInitiatedRequest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url =
      embedded_test_server()->GetURL("/site_isolation/png-corp.png");

  BrowserContext* browser_context =
      shell()->web_contents()->GetBrowserContext();
  StoragePartition* partition = browser_context->GetDefaultStoragePartition();
  ASSERT_EQ(net::OK,
            LoadBasicRequest(partition->GetNetworkContext(), test_url));
}

// This test class sets up a script element for webbundle subresource loading.
// e.g. <script type=webbundle>...</script>
class CrossSiteDocumentBlockingWebBundleTest : public ContentBrowserTest {
 public:
  CrossSiteDocumentBlockingWebBundleTest() = default;
  ~CrossSiteDocumentBlockingWebBundleTest() override = default;

  CrossSiteDocumentBlockingWebBundleTest(
      const CrossSiteDocumentBlockingWebBundleTest&) = delete;
  CrossSiteDocumentBlockingWebBundleTest& operator=(
      const CrossSiteDocumentBlockingWebBundleTest&) = delete;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 protected:
  void SetupScriptWebBundleElementAndImgElement(const GURL& bundle_url,
                                                const GURL subresource_url) {
    // Navigate to the test page.
    ASSERT_TRUE(
        NavigateToURL(shell(), GURL("https://same-origin.test/title1.html")));

    const char kScriptTemplate[] = R"(
      const script = document.createElement('script');
      script.type = 'webbundle';
      script.textContent = JSON.stringify({
        source: $1,
        resources: [$2],
      });
      document.body.appendChild(script);

      const img = document.createElement('img');
      img.src = $2;
      document.body.appendChild(img);
)";
    // Insert a <script> element for webbundle subresoruce loading, and insert
    // an <img> element which loads a resource from the webbundle.
    ASSERT_TRUE(ExecJs(
        shell(), JsReplace(kScriptTemplate, bundle_url, subresource_url)));
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
    https_server_.ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server_.InitializeAndListen());
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP * " + https_server_.host_port_pair().ToString() +
            ",EXCLUDE localhost");
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;
  net::EmbeddedTestServer https_server_{
      net::EmbeddedTestServer::Type::TYPE_HTTPS};
};

// CrossSiteDocumentBlockingWebBundleTest has 4 tests; a cartesian product of
// 1) cross-origin bundle, 2) same-origin bundle
// X
// A). ORB-protected MIME type (e.g. text/json), B) other type (e.g. image/png)
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingWebBundleTest,
                       CrossOriginWebBundleSubresoruceJson) {
  https_server()->StartAcceptingConnections();
  GURL bundle_url("https://cross-origin.test/web_bundle/cross_origin_b2.wbn");
  GURL subresource_url("https://cross-origin.test/web_bundle/resource.json");
  RequestInterceptor interceptor(subresource_url);
  SetupScriptWebBundleElementAndImgElement(bundle_url, subresource_url);
  interceptor.WaitForRequestCompletion();

  EXPECT_EQ(0, interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body())
      << "JSON in a cross-origin webbundle should be blocked by ORB";
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingWebBundleTest,
                       CrossOriginWebBundleSubresorucePng) {
  https_server()->StartAcceptingConnections();
  GURL bundle_url("https://cross-origin.test/web_bundle/cross_origin_b2.wbn");
  GURL subresource_url("https://cross-origin.test/web_bundle/resource.png");
  RequestInterceptor interceptor(subresource_url);
  SetupScriptWebBundleElementAndImgElement(bundle_url, subresource_url);
  interceptor.WaitForRequestCompletion();

  EXPECT_EQ(0, interceptor.completion_status().error_code);
  EXPECT_EQ("broken png", interceptor.response_body())
      << "PNG in a cross-origin webbundle should not be blocked";
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingWebBundleTest,
                       SameOriginWebBundleSubresoruceJson) {
  https_server()->StartAcceptingConnections();
  GURL bundle_url("https://same-origin.test/web_bundle/same_origin_b2.wbn");
  GURL subresource_url("https://same-origin.test/web_bundle/resource.json");
  RequestInterceptor interceptor(subresource_url);
  SetupScriptWebBundleElementAndImgElement(bundle_url, subresource_url);
  interceptor.WaitForRequestCompletion();

  EXPECT_EQ(0, interceptor.completion_status().error_code);
  EXPECT_EQ("{ secret: 1 }", interceptor.response_body())
      << "JSON in a same-origin webbundle should not be blocked";
}

IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingWebBundleTest,
                       SameOriginWebBundleSubresorucePng) {
  https_server()->StartAcceptingConnections();
  GURL bundle_url("https://same-origin.test/web_bundle/same_origin_b2.wbn");
  GURL subresource_url("https://same-origin.test/web_bundle/resource.png");
  RequestInterceptor interceptor(subresource_url);
  SetupScriptWebBundleElementAndImgElement(bundle_url, subresource_url);
  interceptor.WaitForRequestCompletion();

  EXPECT_EQ(0, interceptor.completion_status().error_code);
  EXPECT_EQ("broken png", interceptor.response_body())
      << "PNG in a same-origin webbundle should not be blocked";
}

// TODO(crbug.com/40269364): Remove support for old header names once API users
// have switched.
IN_PROC_BROWSER_TEST_F(CrossSiteDocumentBlockingWebBundleTest,
                       FledgeAuctionOnlySignalsNotReadableFromFetchWebBundle) {
  https_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  // Note that even same origin requests are blocked.
  GURL subresource_url(
      "https://foo.com/interest_group/auction_only_in_bundle.json");
  RequestInterceptor interceptor(subresource_url);
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL("https://foo.com/interest_group/auction_only.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = "fetch($1)";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, subresource_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  interceptor.WaitForRequestCompletion();

  // Verify that X-FLEDGE-Auction-Only blocked the response before it
  // reached the renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

IN_PROC_BROWSER_TEST_F(
    CrossSiteDocumentBlockingWebBundleTest,
    FledgeAuctionOnlySignalsNotReadableFromFetchWebBundleNewName) {
  https_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  // Note that even same origin requests are blocked.
  GURL subresource_url(
      "https://foo.com/interest_group/auction_only_in_bundle.json");
  RequestInterceptor interceptor(subresource_url);
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("https://foo.com/interest_group/auction_only_new_name.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = "fetch($1)";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, subresource_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  interceptor.WaitForRequestCompletion();

  // Verify that Ad-Auction-Only blocked the response before it
  // reached the renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

IN_PROC_BROWSER_TEST_F(
    CrossSiteDocumentBlockingWebBundleTest,
    FledgeAuctionOnlySignalsNotReadableFromFetchWebBundleBothNewAndOldNames) {
  https_server()->StartAcceptingConnections();

  // Navigate to the test page while request interceptor is active.
  // Note that even same origin requests are blocked.
  GURL subresource_url(
      "https://foo.com/interest_group/auction_only_in_bundle.json");
  RequestInterceptor interceptor(subresource_url);
  EXPECT_TRUE(
      NavigateToURL(shell(), GURL("https://foo.com/interest_group/"
                                  "auction_only_both_new_and_old_names.html")));

  // Issue the request that will be intercepted.
  const char kScriptTemplate[] = "fetch($1)";
  EXPECT_TRUE(ExecJs(shell(), JsReplace(kScriptTemplate, subresource_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  interceptor.WaitForRequestCompletion();

  // Verify that both Ad-Auction-Only and X-FLEDGE-Auction-Only blocked the
  // response before it reached the renderer process.
  EXPECT_EQ(net::ERR_BLOCKED_BY_RESPONSE,
            interceptor.completion_status().error_code);
  EXPECT_EQ("", interceptor.response_body());
}

}  // namespace content

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/loader/document_loader.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/rand_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/storage_access_api/status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_encoding_data.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/visited_link_state.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {
namespace {

// Forwards calls from BodyDataReceived() to DecodedBodyDataReceived().
class DecodedBodyLoader : public StaticDataNavigationBodyLoader {
 public:
  void StartLoadingBody(Client* client) override {
    client_ = std::make_unique<DecodedDataPassthroughClient>(client);
    StaticDataNavigationBodyLoader::StartLoadingBody(client_.get());
  }

 private:
  class DecodedDataPassthroughClient : public WebNavigationBodyLoader::Client {
   public:
    explicit DecodedDataPassthroughClient(Client* client) : client_(client) {}

    void BodyDataReceived(base::span<const char> data) override {
      client_->DecodedBodyDataReceived(
          String(data.data(), data.size()).UpperASCII(),
          WebEncodingData{.encoding = "utf-8"}, base::SpanOrSize(data));
    }

    void DecodedBodyDataReceived(
        const WebString& data,
        const WebEncodingData& encoding_data,
        base::SpanOrSize<const char> encoded_data) override {
      client_->DecodedBodyDataReceived(data, encoding_data, encoded_data);
    }

    void BodyLoadingFinished(base::TimeTicks completion_time,
                             int64_t total_encoded_data_length,
                             int64_t total_encoded_body_length,
                             int64_t total_decoded_body_length,
                             const std::optional<WebURLError>& error) override {
      client_->BodyLoadingFinished(completion_time, total_encoded_data_length,
                                   total_encoded_body_length,
                                   total_decoded_body_length, error);
    }

   private:
    Client* client_;
  };

  std::unique_ptr<DecodedDataPassthroughClient> client_;
};

class BodyLoaderTestDelegate : public URLLoaderTestDelegate {
 public:
  explicit BodyLoaderTestDelegate(
      std::unique_ptr<StaticDataNavigationBodyLoader> body_loader)
      : body_loader_(std::move(body_loader)),
        body_loader_raw_(body_loader_.get()) {}

  // URLLoaderTestDelegate overrides:
  bool FillNavigationParamsResponse(WebNavigationParams* params) override {
    params->response = WebURLResponse(params->url);
    params->response.SetMimeType("text/html");
    params->response.SetHttpStatusCode(200);
    params->body_loader = std::move(body_loader_);
    return true;
  }

  void Write(const char* data) {
    body_loader_raw_->Write(base::make_span(data, strlen(data)));
  }

  void Finish() { body_loader_raw_->Finish(); }

 private:
  std::unique_ptr<StaticDataNavigationBodyLoader> body_loader_;
  StaticDataNavigationBodyLoader* body_loader_raw_;
};

// This struct contains the three elements of the :visited links
// triple-parititon key for storage and comparison in this test.
struct TestVisitedLink {
  GURL link_url;
  net::SchemefulSite top_level_site;
  url::Origin frame_origin;

  friend bool operator<(const TestVisitedLink& lhs,
                        const TestVisitedLink& rhs) {
    return std::tie(lhs.link_url, lhs.frame_origin, lhs.top_level_site) <
           std::tie(rhs.link_url, rhs.frame_origin, rhs.top_level_site);
  }
};

// To test (1) the abiltity to obtain and store the per-origin salt used in
// partitioning visited links and (2) the ability of VisitedLinkState to query
// for partitioned visited links using those salts, we need to override the
// Platform::Current() used in this test. Our platform will obtain and store the
// per-origin salt values locally in `salts_` and mock out calls to the
// partitioned hashtable stored in VisitedLinkReader via
// `partitioned_hashtable_`.
class VisitedLinkPlatform : public TestingPlatformSupport {
 public:
  // An override which stores our per-origin salts locally.
  void AddOrUpdateVisitedLinkSalt(const url::Origin& origin,
                                  uint64_t salt) override {
    salts_[origin] = salt;
  }

  // An override which returns the mock-fingerprint associated with the provided
  // unpartitioned link. In our mock code, we convert to an origin for ease of
  // comparison in a limited test environment, but in the production code,
  // comparison is still made via URL. If an entry is not found in the
  // mock-hashtable, 0, or the null fingerprint is returned.
  uint64_t VisitedLinkHash(std::string_view canonical_url) override {
    // Then we check whether our mock-hashtable has an entry for the provided
    // visited link.
    const url::Origin origin = url::Origin::Create(GURL(canonical_url));
    auto it = unpartitioned_hashtable_.find(origin);
    if (it != unpartitioned_hashtable_.end()) {
      return it->second;
    }
    // We do not have a corresponding entry in mock_hashtable_.
    return 0;
  }

  // An override which returns the mock-fingerprint associated with the provided
  // partitioned visited link. If an entry is not found in the mock-hashtable,
  // 0, the null fingerprint value is returned.
  uint64_t PartitionedVisitedLinkFingerprint(
      std::string_view canonical_link_url,
      const net::SchemefulSite& top_level_site,
      const WebSecurityOrigin& frame_origin) override {
    // First we mock a salt check, as VisitedLinkReader will return the null
    // fingerprint if we have not obtained a corresponding per-origin salt.
    if (!GetVisitedLinkSaltForOrigin(frame_origin).has_value()) {
      return 0;
    }

    // Then we check whether our mock-hashtable has an entry for the provided
    // visited link.
    const TestVisitedLink link = {GURL(canonical_link_url), top_level_site,
                                  url::Origin(frame_origin)};
    auto it = partitioned_hashtable_.find(link);
    if (it != partitioned_hashtable_.end()) {
      return it->second;
    }
    // We do not have a corresponding entry in mock_hashtable_.
    return 0;
  }

  // Override which returns true as long as a non-null fingerprint is provided.
  bool IsLinkVisited(uint64_t link_hash) override { return link_hash != 0; }

  // Test cases can query whether we obtained a salt for a specific origin.
  std::optional<uint64_t> GetVisitedLinkSaltForOrigin(
      const url::Origin& origin) {
    auto it = salts_.find(origin);
    if (it != salts_.end()) {
      return it->second;
    }
    // We do not have a corresponding salt for this origin.
    return std::nullopt;
  }

  void AddPartitionedVisitedLinkToMockHashtable(const KURL& link_url,
                                                const KURL& top_level_url,
                                                const KURL& frame_url) {
    uint64_t mock_fingerprint = base::RandUint64();
    // Zero represents the null fingerprint in our production code, and when we
    // actually generate hashed fingerprints, producing a 0 is not possible.
    // However, in the mocked environment, we could generate a random 0, so we
    // should re-generate the random fingerprint if that occurs.
    while (mock_fingerprint == 0) {
      mock_fingerprint = base::RandUint64();
    }
    const TestVisitedLink link = {GURL(link_url),
                                  net::SchemefulSite(GURL(top_level_url)),
                                  url::Origin::Create(GURL(frame_url))};
    partitioned_hashtable_.insert({link, mock_fingerprint});
  }

  void AddUnpartitionedVisitedLinkToMockHashtable(const KURL& url) {
    uint64_t mock_fingerprint = base::RandUint64();
    // Zero represents the null fingerprint in our production code, and when we
    // actually generate hashed fingerprints, producing a 0 is not possible.
    // However, in the mocked environment, we could generate a random 0, so we
    // should re-generate the random fingerprint if that occurs.
    while (mock_fingerprint == 0) {
      mock_fingerprint = base::RandUint64();
    }
    unpartitioned_hashtable_.insert(
        {url::Origin::Create(GURL(url)), mock_fingerprint});
  }

 private:
  std::map<url::Origin, uint64_t> salts_;
  std::map<TestVisitedLink, uint64_t> partitioned_hashtable_;
  std::map<url::Origin, uint64_t> unpartitioned_hashtable_;
};

enum TestMode {
  kUnpartitionedStorageAndLinks,
  kUnpartitionedStoragePartitionedNoSelfLinks,
  kUnpartitionedStorageParttionedWithSelfLinks,
  kUnpartitionedStoragePartitionedLinksBothEnabled,
  kPartitionedStorageUnpartitionedLinks,
  kPartitionedStorageAndLinksNoSelfLinks,
  kPartitionedStorageAndLinksWithSelfLinks,
  kPartitionedAllEnabled
};

class DocumentLoaderTest : public testing::Test,
                           public ::testing::WithParamInterface<TestMode> {
 protected:
  void SetUp() override {
    switch (GetParam()) {
      case TestMode::kUnpartitionedStorageAndLinks:
        scoped_feature_list_.InitWithFeatures(
            {}, {net::features::kThirdPartyStoragePartitioning,
                 blink::features::kPartitionVisitedLinkDatabase,
                 blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks});
        break;
      case TestMode::kUnpartitionedStoragePartitionedNoSelfLinks:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kPartitionVisitedLinkDatabase},
            {net::features::kThirdPartyStoragePartitioning,
             blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks});
        break;
      case TestMode::kUnpartitionedStorageParttionedWithSelfLinks:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks},
            {net::features::kThirdPartyStoragePartitioning,
             blink::features::kPartitionVisitedLinkDatabase});
        break;
      case TestMode::kUnpartitionedStoragePartitionedLinksBothEnabled:
        scoped_feature_list_.InitWithFeatures(
            {blink::features::kPartitionVisitedLinkDatabase,
             blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks},
            {net::features::kThirdPartyStoragePartitioning});
        break;
      case TestMode::kPartitionedStorageUnpartitionedLinks:
        scoped_feature_list_.InitWithFeatures(
            {net::features::kThirdPartyStoragePartitioning},
            {blink::features::kPartitionVisitedLinkDatabase,
             blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks});
        break;
      case TestMode::kPartitionedStorageAndLinksNoSelfLinks:
        scoped_feature_list_.InitWithFeatures(
            {net::features::kThirdPartyStoragePartitioning,
             blink::features::kPartitionVisitedLinkDatabase},
            {blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks});
        break;
      case TestMode::kPartitionedStorageAndLinksWithSelfLinks:
        scoped_feature_list_.InitWithFeatures(
            {net::features::kThirdPartyStoragePartitioning,
             blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks},
            {blink::features::kPartitionVisitedLinkDatabase});
        break;
      case TestMode::kPartitionedAllEnabled:
        scoped_feature_list_.InitWithFeatures(
            {net::features::kThirdPartyStoragePartitioning,
             blink::features::kPartitionVisitedLinkDatabase,
             blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks},
            {});
        break;
    }

    web_view_helper_.Initialize();
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("http://example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("http://user:@example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("http://:pass@example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("http://user:pass@example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com:8000/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("http://192.168.1.1/foo.html"),
        test::CoreTestDataPath("foo.html"), WebString::FromUTF8("text/html"),
        URLLoaderMockFactory::GetSingletonInstance(),
        network::mojom::IPAddressSpace::kPrivate);
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://192.168.1.1/foo.html"),
        test::CoreTestDataPath("foo.html"), WebString::FromUTF8("text/html"),
        URLLoaderMockFactory::GetSingletonInstance(),
        network::mojom::IPAddressSpace::kPrivate);
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("http://somethinglocal/foo.html"),
        test::CoreTestDataPath("foo.html"), WebString::FromUTF8("text/html"),
        URLLoaderMockFactory::GetSingletonInstance(),
        network::mojom::IPAddressSpace::kLocal);
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  bool are_visited_links_partitioned() {
    return GetParam() == kUnpartitionedStoragePartitionedNoSelfLinks ||
           (GetParam() == kUnpartitionedStorageParttionedWithSelfLinks) ||
           (GetParam() == kUnpartitionedStoragePartitionedLinksBothEnabled) ||
           (GetParam() == kPartitionedStorageAndLinksNoSelfLinks) ||
           (GetParam() == kPartitionedStorageAndLinksWithSelfLinks) ||
           (GetParam() == kPartitionedAllEnabled);
  }

  class ScopedLoaderDelegate {
   public:
    explicit ScopedLoaderDelegate(URLLoaderTestDelegate* delegate) {
      url_test_helpers::SetLoaderDelegate(delegate);
    }
    ~ScopedLoaderDelegate() { url_test_helpers::SetLoaderDelegate(nullptr); }
  };

  WebLocalFrameImpl* MainFrame() { return web_view_helper_.LocalMainFrame(); }

  ScopedTestingPlatformSupport<VisitedLinkPlatform> platform_;
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    DocumentLoaderTest,
    DocumentLoaderTest,
    testing::Values(TestMode::kUnpartitionedStorageAndLinks,
                    TestMode::kUnpartitionedStoragePartitionedNoSelfLinks,
                    TestMode::kUnpartitionedStorageParttionedWithSelfLinks,
                    TestMode::kUnpartitionedStoragePartitionedLinksBothEnabled,
                    TestMode::kPartitionedStorageUnpartitionedLinks,
                    TestMode::kPartitionedStorageAndLinksNoSelfLinks,
                    TestMode::kPartitionedStorageAndLinksWithSelfLinks,
                    TestMode::kPartitionedAllEnabled));

TEST_P(DocumentLoaderTest, SingleChunk) {
  class TestDelegate : public URLLoaderTestDelegate {
   public:
    void DidReceiveData(URLLoaderClient* original_client,
                        base::span<const char> data) override {
      EXPECT_EQ(34u, data.size())
          << "foo.html was not served in a single chunk";
      original_client->DidReceiveDataForTesting(data);
    }
  } delegate;

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");

  // TODO(dcheng): How should the test verify that the original callback is
  // invoked? The test currently still passes even if the test delegate
  // forgets to invoke the callback.
}

// Test normal case of DocumentLoader::dataReceived(): data in multiple chunks,
// with no reentrancy.
TEST_P(DocumentLoaderTest, MultiChunkNoReentrancy) {
  class TestDelegate : public URLLoaderTestDelegate {
   public:
    void DidReceiveData(URLLoaderClient* original_client,
                        base::span<const char> data) override {
      EXPECT_EQ(34u, data.size())
          << "foo.html was not served in a single chunk";
      // Chunk the reply into one byte chunks.
      for (size_t i = 0; i < data.size(); ++i) {
        original_client->DidReceiveDataForTesting(data.subspan(i, 1));
      }
    }
  } delegate;

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");
}

// Finally, test reentrant callbacks to DocumentLoader::BodyDataReceived().
TEST_P(DocumentLoaderTest, MultiChunkWithReentrancy) {
  // This test delegate chunks the response stage into three distinct stages:
  // 1. The first BodyDataReceived() callback, which triggers frame detach
  //    due to committing a provisional load.
  // 2. The middle part of the response, which is dispatched to
  //    BodyDataReceived() reentrantly.
  // 3. The final chunk, which is dispatched normally at the top-level.
  class MainFrameClient : public URLLoaderTestDelegate,
                          public frame_test_helpers::TestWebFrameClient {
   public:
    // URLLoaderTestDelegate overrides:
    bool FillNavigationParamsResponse(WebNavigationParams* params) override {
      params->response = WebURLResponse(params->url);
      params->response.SetMimeType("application/x-webkit-test-webplugin");
      params->response.SetHttpStatusCode(200);

      String data("<html><body>foo</body></html>");
      for (wtf_size_t i = 0; i < data.length(); i++)
        data_.push_back(data[i]);

      auto body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
      body_loader_ = body_loader.get();
      params->body_loader = std::move(body_loader);
      return true;
    }

    void Serve() {
      {
        // Serve the first byte to the real URLLoaderClient, which should
        // trigger frameDetach() due to committing a provisional load.
        base::AutoReset<bool> dispatching(&dispatching_did_receive_data_, true);
        DispatchOneByte();
      }

      // Serve the remaining bytes to complete the load.
      EXPECT_FALSE(data_.empty());
      while (!data_.empty())
        DispatchOneByte();

      body_loader_->Finish();
      body_loader_ = nullptr;
    }

    // WebLocalFrameClient overrides:
    void RunScriptsAtDocumentElementAvailable() override {
      if (dispatching_did_receive_data_) {
        // This should be called by the first BodyDataReceived() call, since
        // it should create a plugin document structure and trigger this.
        EXPECT_GT(data_.size(), 10u);
        // Dispatch BodyDataReceived() callbacks for part of the remaining
        // data, saving the rest to be dispatched at the top-level as
        // normal.
        while (data_.size() > 10)
          DispatchOneByte();
        served_reentrantly_ = true;
      }
      TestWebFrameClient::RunScriptsAtDocumentElementAvailable();
    }

    void DispatchOneByte() {
      char c = data_.TakeFirst();
      body_loader_->Write(base::make_span(&c, static_cast<size_t>(1)));
    }

    bool ServedReentrantly() const { return served_reentrantly_; }

   private:
    Deque<char> data_;
    bool dispatching_did_receive_data_ = false;
    bool served_reentrantly_ = false;
    StaticDataNavigationBodyLoader* body_loader_ = nullptr;
  };

  // We use a plugin document triggered by "application/x-webkit-test-webplugin"
  // mime type, because that gives us reliable way to get a WebLocalFrameClient
  // callback from inside BodyDataReceived() call.
  ScopedFakePluginRegistry fake_plugins;
  MainFrameClient main_frame_client;
  web_view_helper_.Initialize(&main_frame_client);
  web_view_helper_.GetWebView()->GetPage()->GetSettings().SetPluginsEnabled(
      true);

  {
    ScopedLoaderDelegate loader_delegate(&main_frame_client);
    frame_test_helpers::LoadFrameDontWait(
        MainFrame(), url_test_helpers::ToKURL("https://example.com/foo.html"));
    main_frame_client.Serve();
    frame_test_helpers::PumpPendingRequestsForFrameToLoad(MainFrame());
  }

  // Sanity check that we did actually test reeentrancy.
  EXPECT_TRUE(main_frame_client.ServedReentrantly());

  // MainFrameClient is stack-allocated, so manually Reset to avoid UAF.
  web_view_helper_.Reset();
}

TEST_P(DocumentLoaderTest, isCommittedButEmpty) {
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("about:blank");
  EXPECT_TRUE(To<LocalFrame>(web_view_impl->GetPage()->MainFrame())
                  ->Loader()
                  .GetDocumentLoader()
                  ->IsCommittedButEmpty());
}

class DocumentLoaderSimTest : public SimTest {};

TEST_F(DocumentLoaderSimTest, DocumentOpenUpdatesUrl) {
  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Write("<iframe src='javascript:42;'></iframe>");

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_document = child_frame->GetFrame()->GetDocument();
  EXPECT_TRUE(child_document->HasPendingJavaScriptUrlsForTest());

  main_resource.Write(
      "<script>"
      "window[0].document.open();"
      "window[0].document.write('hello');"
      "window[0].document.close();"
      "</script>");

  main_resource.Finish();

  // document.open() should have cancelled the pending JavaScript URLs.
  EXPECT_FALSE(child_document->HasPendingJavaScriptUrlsForTest());

  // Per https://whatwg.org/C/dynamic-markup-insertion.html#document-open-steps,
  // the URL associated with the Document should match the URL of the entry
  // Document.
  EXPECT_EQ(KURL("https://example.com"), child_document->Url());
  // Similarly, the URL of the DocumentLoader should also match.
  EXPECT_EQ(KURL("https://example.com"), child_document->Loader()->Url());
}

TEST_F(DocumentLoaderSimTest, FramePolicyIntegrityOnNavigationCommit) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://example.com/foo.html", "text/html");
  LoadURL("https://example.com");

  main_resource.Write(R"(
    <iframe id='frame1'></iframe>
    <script>
      const iframe = document.getElementById('frame1');
      iframe.src = 'https://example.com/foo.html'; // navigation triggered
      iframe.allow = "payment 'none'"; // should not take effect until the
                                       // next navigation on iframe
    </script>
  )");

  main_resource.Finish();
  iframe_resource.Finish();

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_window = child_frame->GetFrame()->DomWindow();

  EXPECT_TRUE(child_window->IsFeatureEnabled(
      mojom::blink::PermissionsPolicyFeature::kPayment));
}

TEST_P(DocumentLoaderTest, CommitsDeferredOnSameOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& same_origin_url =
      KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(same_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest,
       CommitsNotDeferredOnDifferentOriginNavigationWithCrossOriginDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPaintHoldingCrossOrigin);

  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest,
       CommitsDeferredOnDifferentOriginNavigationWithCrossOriginEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPaintHoldingCrossOrigin);

  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest,
       CommitsNotDeferredOnDifferentPortNavigationWithCrossOriginDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kPaintHoldingCrossOrigin);

  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com:8000/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com:8000/foo.html");

  const KURL& different_port_url =
      KURL(NullURL(), "https://www.example.com:8080/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(different_port_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest,
       CommitsDeferredOnDifferentPortNavigationWithCrossOriginEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPaintHoldingCrossOrigin);

  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com:8000/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com:8000/foo.html");

  const KURL& different_port_url =
      KURL(NullURL(), "https://www.example.com:8080/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(different_port_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest, CommitsNotDeferredOnDataURLNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& data_url = KURL(NullURL(), "data:,Hello%2C%20World!");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(data_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest,
       CommitsNotDeferredOnDataURLNavigationWithCrossOriginEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPaintHoldingCrossOrigin);

  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& data_url = KURL(NullURL(), "data:,Hello%2C%20World!");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(data_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_P(DocumentLoaderTest, NavigationToAboutBlank) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://subdomain.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& about_blank_url = KURL(NullURL(), "about:blank");
  std::unique_ptr<WebNavigationParams> params =
      std::make_unique<WebNavigationParams>();
  params->url = about_blank_url;
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  params->storage_key = local_frame->DomWindow()->GetStorageKey();
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(
      BlinkStorageKey::CreateFirstParty(SecurityOrigin::Create(requestor_url)),
      local_frame->DomWindow()->GetStorageKey());
}

TEST_P(DocumentLoaderTest, SameOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& same_origin_url =
      KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(same_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  params->storage_key = BlinkStorageKey::CreateFirstParty(
      SecurityOrigin::Create(same_origin_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(BlinkStorageKey::CreateFirstParty(
                SecurityOrigin::Create(same_origin_url)),
            local_frame->DomWindow()->GetStorageKey());

  EXPECT_EQ(local_frame->DomWindow()->GetStorageAccessApiStatus(),
            net::StorageAccessApiStatus::kNone);

  EXPECT_TRUE(local_frame->Loader()
                  .GetDocumentLoader()
                  ->LastNavigationHadTrustedInitiator());
}

TEST_P(DocumentLoaderTest, SameOriginNavigation_WithStorageAccess) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& same_origin_url =
      KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(same_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  params->load_with_storage_access = net::StorageAccessApiStatus::kAccessViaAPI;
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  base::HistogramTester histogram_tester;
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(local_frame->DomWindow()->GetStorageAccessApiStatus(),
            net::StorageAccessApiStatus::kAccessViaAPI);

  EXPECT_TRUE(local_frame->Loader()
                  .GetDocumentLoader()
                  ->LastNavigationHadTrustedInitiator());

  histogram_tester.ExpectUniqueSample(
      "API.StorageAccess.DocumentLoadedWithStorageAccess", /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccess.DocumentInheritedStorageAccess", /*sample=*/true,
      /*expected_bucket_count=*/1);
}

TEST_P(DocumentLoaderTest, CrossOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  params->storage_key = BlinkStorageKey::CreateFirstParty(
      SecurityOrigin::Create(other_origin_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  base::HistogramTester histogram_tester;
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(BlinkStorageKey::CreateFirstParty(
                SecurityOrigin::Create(other_origin_url)),
            local_frame->DomWindow()->GetStorageKey());

  EXPECT_FALSE(local_frame->Loader()
                   .GetDocumentLoader()
                   ->LastNavigationHadTrustedInitiator());

  histogram_tester.ExpectUniqueSample(
      "API.StorageAccess.DocumentLoadedWithStorageAccess", /*sample=*/false,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "API.StorageAccess.DocumentInheritedStorageAccess", /*sample=*/false,
      /*expected_bucket_count=*/1);
}

TEST_P(DocumentLoaderTest, StorageKeyFromNavigationParams) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));

  url::Origin origin;
  auto nonce = base::UnguessableToken::Create();
  StorageKey storage_key_to_commit = StorageKey::CreateWithNonce(origin, nonce);
  params->storage_key = storage_key_to_commit;

  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(
      BlinkStorageKey::CreateWithNonce(SecurityOrigin::Create(other_origin_url),
                                       storage_key_to_commit.nonce().value()),
      local_frame->DomWindow()->GetStorageKey());
}

TEST_P(DocumentLoaderTest, StorageKeyCrossSiteFromNavigationParams) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));

  net::SchemefulSite top_level_site =
      net::SchemefulSite(url::Origin::Create(GURL("https://foo.com")));
  StorageKey storage_key_to_commit =
      StorageKey::Create(url::Origin::Create(GURL(other_origin_url)),
                         top_level_site, mojom::AncestorChainBit::kCrossSite);
  params->storage_key = storage_key_to_commit;

  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_EQ(BlinkStorageKey::Create(SecurityOrigin::Create(other_origin_url),
                                    BlinkSchemefulSite(top_level_site),
                                    mojom::AncestorChainBit::kCrossSite),
            local_frame->DomWindow()->GetStorageKey());
}

// Tests that committing a Javascript URL keeps the storage key's nonce of the
// previous document, ensuring that
// `DocumentLoader::CreateWebNavigationParamsToCloneDocument` works correctly
// w.r.t. storage key.
TEST_P(DocumentLoaderTest, JavascriptURLKeepsStorageKeyNonce) {
  WebViewImpl* web_view_impl = web_view_helper_.Initialize();

  BlinkStorageKey storage_key = BlinkStorageKey::CreateWithNonce(
      SecurityOrigin::CreateUniqueOpaque(), base::UnguessableToken::Create());

  LocalFrame* frame = To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  frame->DomWindow()->SetStorageKey(storage_key);

  frame->LoadJavaScriptURL(
      url_test_helpers::ToKURL("javascript:'<p>hello world</p>'"));

  EXPECT_EQ(storage_key.GetNonce(),
            frame->DomWindow()->GetStorageKey().GetNonce());
}

// Tests that discarding the frame keeps the storage key's nonce of the previous
// document, ensuring that
// `DocumentLoader::CreateWebNavigationParamsToCloneDocument` works correctly
// w.r.t. storage key.
TEST_P(DocumentLoaderTest, DiscardingFrameKeepsStorageKeyNonce) {
  WebViewImpl* web_view_impl = web_view_helper_.Initialize();

  BlinkStorageKey storage_key = BlinkStorageKey::CreateWithNonce(
      SecurityOrigin::CreateUniqueOpaque(), base::UnguessableToken::Create());

  LocalFrame* frame = To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  frame->DomWindow()->SetStorageKey(storage_key);

  frame->Discard();

  EXPECT_EQ(storage_key.GetNonce(),
            frame->DomWindow()->GetStorageKey().GetNonce());
}

TEST_P(DocumentLoaderTest, PublicSecureNotCounted) {
  // Checking to make sure secure pages served in the public address space
  // aren't counted for WebFeature::kMainFrameNonSecurePrivateAddressSpace
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  EXPECT_FALSE(document->IsUseCounted(
      WebFeature::kMainFrameNonSecurePrivateAddressSpace));
}

TEST_P(DocumentLoaderTest, PublicNonSecureNotCounted) {
  // Checking to make sure non-secure pages served in the public address space
  // aren't counted for WebFeature::kMainFrameNonSecurePrivateAddressSpace
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("http://example.com/foo.html");
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  EXPECT_FALSE(document->IsUseCounted(
      WebFeature::kMainFrameNonSecurePrivateAddressSpace));
}

TEST_P(DocumentLoaderTest, PrivateSecureNotCounted) {
  // Checking to make sure secure pages served in the private address space
  // aren't counted for WebFeature::kMainFrameNonSecurePrivateAddressSpace
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://192.168.1.1/foo.html");
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  EXPECT_FALSE(document->IsUseCounted(
      WebFeature::kMainFrameNonSecurePrivateAddressSpace));
}

TEST_P(DocumentLoaderTest, PrivateNonSecureIsCounted) {
  // Checking to make sure non-secure pages served in the private address space
  // are counted for WebFeature::kMainFrameNonSecurePrivateAddressSpace
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("http://192.168.1.1/foo.html");
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  EXPECT_TRUE(document->IsUseCounted(
      WebFeature::kMainFrameNonSecurePrivateAddressSpace));
}

TEST_P(DocumentLoaderTest, LocalNonSecureIsCounted) {
  // Checking to make sure non-secure pages served in the local address space
  // are counted for WebFeature::kMainFrameNonSecurePrivateAddressSpace
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("http://somethinglocal/foo.html");
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  EXPECT_TRUE(document->IsUseCounted(
      WebFeature::kMainFrameNonSecurePrivateAddressSpace));
}

TEST_F(DocumentLoaderSimTest, PrivateNonSecureChildFrameNotCounted) {
  // Checking to make sure non-secure iframes served in the private address
  // space are not counted for
  // WebFeature::kMainFrameNonSecurePrivateAddressSpace
  SimRequest main_resource("http://example.com", "text/html");
  SimRequest iframe_resource("http://192.168.1.1/foo.html", "text/html");
  LoadURL("http://example.com");

  main_resource.Write(R"(
    <iframe id='frame1'></iframe>
    <script>
      const iframe = document.getElementById('frame1');
      iframe.src = 'http://192.168.1.1/foo.html'; // navigation triggered
    </script>
  )");

  main_resource.Finish();
  iframe_resource.Finish();

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_document = child_frame->GetFrame()->GetDocument();

  EXPECT_FALSE(child_document->IsUseCounted(
      WebFeature::kMainFrameNonSecurePrivateAddressSpace));
}

TEST_P(DocumentLoaderTest, DecodedBodyData) {
  BodyLoaderTestDelegate delegate(std::make_unique<DecodedBodyLoader>());

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrameDontWait(
      MainFrame(), url_test_helpers::ToKURL("https://example.com/foo.html"));

  delegate.Write("<html>");
  delegate.Write("<body>fo");
  delegate.Write("o</body>");
  delegate.Write("</html>");
  delegate.Finish();

  frame_test_helpers::PumpPendingRequestsForFrameToLoad(MainFrame());

  // DecodedBodyLoader uppercases all data.
  EXPECT_EQ(MainFrame()->GetDocument().Body().TextContent(), "FOO");
}

TEST_P(DocumentLoaderTest, DecodedBodyDataWithBlockedParser) {
  BodyLoaderTestDelegate delegate(std::make_unique<DecodedBodyLoader>());

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrameDontWait(
      MainFrame(), url_test_helpers::ToKURL("https://example.com/foo.html"));

  delegate.Write("<html>");
  // Blocking the parser tests whether we buffer decoded data correctly.
  MainFrame()->GetDocumentLoader()->BlockParser();
  delegate.Write("<body>fo");
  delegate.Write("o</body>");
  MainFrame()->GetDocumentLoader()->ResumeParser();
  delegate.Write("</html>");
  delegate.Finish();

  frame_test_helpers::PumpPendingRequestsForFrameToLoad(MainFrame());

  // DecodedBodyLoader uppercases all data.
  EXPECT_EQ(MainFrame()->GetDocument().Body().TextContent(), "FOO");
}

TEST_P(DocumentLoaderTest, EmbeddedCredentialsNavigation) {
  struct TestCase {
    const char* url;
    const bool useCounted;
  } test_cases[] = {{"http://example.com/foo.html", false},
                    {"http://user:@example.com/foo.html", true},
                    {"http://:pass@example.com/foo.html", true},
                    {"http://user:pass@example.com/foo.html", true}};
  for (const auto& test_case : test_cases) {
    WebViewImpl* web_view_impl =
        web_view_helper_.InitializeAndLoad(test_case.url);
    Document* document =
        To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
    EXPECT_EQ(test_case.useCounted,
              document->IsUseCounted(
                  WebFeature::kTopLevelDocumentWithEmbeddedCredentials));
  }
}

TEST_P(DocumentLoaderTest, VisitedLinkSalt) {
  // Generate the constants.
  const uint64_t kSalt = base::RandUint64();
  const KURL& kUrl = KURL(NullURL(), "https://www.example.com/foo.html");

  // Load a blank slate.
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("about:blank");

  // Create params for the URL we will navigate to next.
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(kUrl);
  params->visited_link_salt = kSalt;

  // Perform the navigation and provide an empty vector for visited link state.
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  // Check if the platform was notified of our salt.
  std::optional<uint64_t> result_salt =
      platform_->GetVisitedLinkSaltForOrigin(url::Origin::Create(GURL(kUrl)));
  ASSERT_EQ(result_salt.has_value(), are_visited_links_partitioned());
  if (result_salt.has_value()) {
    EXPECT_EQ(result_salt.value(), kSalt);
  }
}

TEST_P(DocumentLoaderTest, PartitionedVisitedLinksMainFrame) {
  // Generate the constants.
  const uint64_t kSalt = base::RandUint64();
  const KURL kUrl("https://www.example.com/foo.html");
  const KURL kCrossSiteUrl("https://www.foo.com/bar.html");

  // Mock a previous navigation to the kCrossSiteUrl via kUrl.
  platform_->AddUnpartitionedVisitedLinkToMockHashtable(kCrossSiteUrl);
  platform_->AddPartitionedVisitedLinkToMockHashtable(kCrossSiteUrl, kUrl,
                                                      kUrl);

  // Load a blank slate.
  const WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("about:blank");

  // Create params for the URL we will navigate to next.
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithEmptyHTMLForTesting(kUrl);
  params->visited_link_salt = kSalt;

  // Perform the navigation and provide an empty vector for visited link state.
  const LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  // Obtain the Document we just navigated to.
  Document* document =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame())->GetDocument();
  // Prepare a mock Link Element to check if we have visited.
  Element* visited_link = document->CreateRawElement(html_names::kATag);
  visited_link->setAttribute(html_names::kHrefAttr, kCrossSiteUrl.GetString());

  // Check if our mock Link Element would be styled as visited.
  EInsideLink result =
      document->GetVisitedLinkState().DetermineLinkState(*visited_link);
  EXPECT_EQ(result, EInsideLink::kInsideVisitedLink);

  // Prepare a mock Link Element that we haven't visited.
  Element* unvisited_link = document->CreateRawElement(html_names::kATag);
  unvisited_link->setAttribute(html_names::kHrefAttr,
                               AtomicString("https://bar.com"));

  // Check if our mock Link Element would not be styled as visited.
  result = document->GetVisitedLinkState().DetermineLinkState(*unvisited_link);
  EXPECT_NE(result, EInsideLink::kInsideVisitedLink);
}

}  // namespace
}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/document_loader.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/feature_policy/policy_disposition.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/scoped_fake_plugin_registry.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace blink {

class DocumentLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com:8000/foo.html"),
        test::CoreTestDataPath("foo.html"));
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  class ScopedLoaderDelegate {
   public:
    ScopedLoaderDelegate(WebURLLoaderTestDelegate* delegate) {
      url_test_helpers::SetLoaderDelegate(delegate);
    }
    ~ScopedLoaderDelegate() { url_test_helpers::SetLoaderDelegate(nullptr); }
  };

  WebLocalFrameImpl* MainFrame() { return web_view_helper_.LocalMainFrame(); }

  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(DocumentLoaderTest, SingleChunk) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";
      original_client->DidReceiveData(data, data_length);
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
TEST_F(DocumentLoaderTest, MultiChunkNoReentrancy) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";
      // Chunk the reply into one byte chunks.
      for (int i = 0; i < data_length; ++i)
        original_client->DidReceiveData(&data[i], 1);
    }
  } delegate;

  ScopedLoaderDelegate loader_delegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");
}

// Finally, test reentrant callbacks to DocumentLoader::BodyDataReceived().
TEST_F(DocumentLoaderTest, MultiChunkWithReentrancy) {
  // This test delegate chunks the response stage into three distinct stages:
  // 1. The first BodyDataReceived() callback, which triggers frame detach
  //    due to committing a provisional load.
  // 2. The middle part of the response, which is dispatched to
  //    BodyDataReceived() reentrantly.
  // 3. The final chunk, which is dispatched normally at the top-level.
  class MainFrameClient : public WebURLLoaderTestDelegate,
                          public frame_test_helpers::TestWebFrameClient {
   public:
    // WebURLLoaderTestDelegate overrides:
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
        // Serve the first byte to the real WebURLLoaderCLient, which
        // should trigger frameDetach() due to committing a provisional
        // load.
        base::AutoReset<bool> dispatching(&dispatching_did_receive_data_, true);
        DispatchOneByte();
      }

      // Serve the remaining bytes to complete the load.
      EXPECT_FALSE(data_.IsEmpty());
      while (!data_.IsEmpty())
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
      body_loader_->Write(&c, 1);
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

TEST_F(DocumentLoaderTest, isCommittedButEmpty) {
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
      blink::mojom::blink::FeaturePolicyFeature::kPayment));
}
// When runtime feature DocumentPolicy is not enabled, specifying
// Document-Policy, Require-Document-Policy and policy attribute
// should have no effect, i.e.
// document load should not be blocked even if the required policy and incoming
// policy are incompatible and calling
// |Document::IsFeatureEnabled(DocumentPolicyFeature...)| should always return
// true.
TEST_F(DocumentLoaderSimTest, DocumentPolicyNoEffectWhenFlagNotSet) {
  blink::ScopedDocumentPolicyForTest sdp(false);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(false);

  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Require-Document-Policy", "lossless-images-max-bpp=1.0"}};

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Document-Policy", "lossless-images-max-bpp=1.1"}};

  SimRequest main_resource("https://example.com", "text/html", main_params);
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="lossless-images-max-bpp=1.0">
    </iframe>
  )");

  iframe_resource.Finish();
  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_window = child_frame->GetFrame()->DomWindow();
  auto& console_messages = static_cast<frame_test_helpers::TestWebFrameClient*>(
                               child_frame->Client())
                               ->ConsoleMessages();

  // Should not receive a console error message caused by document policy
  // violation blocking document load.
  EXPECT_TRUE(console_messages.IsEmpty());

  EXPECT_EQ(child_window->Url(), KURL("https://example.com/foo.html"));

  EXPECT_FALSE(child_window->document()->IsUseCounted(
      mojom::WebFeature::kDocumentPolicyCausedPageUnload));

  // Unoptimized-lossless-images should still be allowed in main document.
  EXPECT_TRUE(Window().IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(2.0)));
  EXPECT_TRUE(Window().IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(1.0)));

  // Unoptimized-lossless-images should still be allowed in child document.
  EXPECT_TRUE(child_window->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(2.0)));
  EXPECT_TRUE(child_window->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(1.0)));
}

// When runtime feature DocumentPolicyNegotiation is not enabled, specifying
// Require-Document-Policy HTTP header and  policy attribute on iframe should
// have no effect, i.e. document load should not be blocked even if the required
// policy and incoming policy are incompatible. Document-Policy header should
// function as normal.
TEST_F(DocumentLoaderSimTest, DocumentPolicyNegotiationNoEffectWhenFlagNotSet) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(false);

  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Require-Document-Policy", "lossless-images-max-bpp=1.0"}};

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Document-Policy", "lossless-images-max-bpp=1.1"}};

  SimRequest main_resource("https://example.com", "text/html", main_params);
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="lossless-images-max-bpp=1.0">
    </iframe>
  )");

  iframe_resource.Finish();
  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_window = child_frame->GetFrame()->DomWindow();
  auto& console_messages = static_cast<frame_test_helpers::TestWebFrameClient*>(
                               child_frame->Client())
                               ->ConsoleMessages();

  // Should not receive a console error message caused by document policy
  // violation blocking document load.
  EXPECT_TRUE(console_messages.IsEmpty());

  EXPECT_EQ(child_window->Url(), KURL("https://example.com/foo.html"));

  EXPECT_FALSE(child_window->document()->IsUseCounted(
      mojom::WebFeature::kDocumentPolicyCausedPageUnload));

  // Unoptimized-lossless-images should still be allowed in main document.
  EXPECT_TRUE(Window().IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(2.0)));
  EXPECT_TRUE(Window().IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(1.0)));

  // Unoptimized-lossless-images should NOT be allowed in child document,
  // with the threshold value specified in Document-Policy header.
  EXPECT_FALSE(child_window->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(2.0)));
  EXPECT_TRUE(child_window->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kLosslessImagesMaxBpp,
      PolicyValue::CreateDecDouble(1.0)));
}

TEST_F(DocumentLoaderSimTest, ReportDocumentPolicyHeaderParsingError) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  SimRequest::Params params;
  params.response_http_headers = {{"Document-Policy", "bad-feature-name"}};
  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(
      ConsoleMessages().front().StartsWith("Document-Policy HTTP header:"));
}

TEST_F(DocumentLoaderSimTest, ReportRequireDocumentPolicyHeaderParsingError) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  SimRequest::Params params;
  params.response_http_headers = {
      {"Require-Document-Policy", "bad-feature-name"}};
  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(ConsoleMessages().front().StartsWith(
      "Require-Document-Policy HTTP header:"));
}

TEST_F(DocumentLoaderSimTest, ReportErrorWhenDocumentPolicyIncompatible) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(true);
  SimRequest::Params params;
  params.response_http_headers = {
      {"Document-Policy", "lossless-images-max-bpp=1.1"}};

  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="lossless-images-max-bpp=1.0">
    </iframe>
  )");

  // When blocked by document policy, the document should be filled in with an
  // empty response, with Finish called on |navigation_body_loader| already.
  // If Finish was not called on the loader, because the document was not
  // blocked, this test will fail by crashing here.
  iframe_resource.Finish(true /* body_loader_finished */);

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_document = child_frame->GetFrame()->GetDocument();

  // Should console log a error message.
  auto& console_messages = static_cast<frame_test_helpers::TestWebFrameClient*>(
                               child_frame->Client())
                               ->ConsoleMessages();

  ASSERT_EQ(console_messages.size(), 1u);
  EXPECT_TRUE(console_messages.front().Contains("document policy"));

  // Should replace the document's origin with an opaque origin.
  EXPECT_EQ(child_document->Url(), SecurityOrigin::UrlWithUniqueOpaqueOrigin());

  EXPECT_TRUE(child_document->IsUseCounted(
      mojom::WebFeature::kDocumentPolicyCausedPageUnload));
}

// HTTP header Require-Document-Policy should only take effect on subtree of
// current document, but not on current document.
TEST_F(DocumentLoaderSimTest,
       RequireDocumentPolicyHeaderShouldNotAffectCurrentDocument) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(true);
  SimRequest::Params params;
  params.response_http_headers = {
      {"Require-Document-Policy", "lossless-images-max-bpp=1.0"},
      {"Document-Policy", "lossless-images-max-bpp=1.1"}};

  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  // If document is blocked by document policy because of incompatible document
  // policy, this test will fail by crashing here.
  main_resource.Finish();
}

TEST_F(DocumentLoaderSimTest, DocumentPolicyHeaderHistogramTest) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  HistogramTester histogram_tester;

  SimRequest::Params params;
  params.response_http_headers = {
      {"Document-Policy",
       "font-display-late-swap, lossless-images-max-bpp=1.1"}};

  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  histogram_tester.ExpectTotalCount("Blink.UseCounter.DocumentPolicy.Header",
                                    2);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Header",
                                     1 /* kFontDisplay */, 1);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Header",
                                     2 /* kUnoptimizedLosslessImages */, 1);
}

TEST_F(DocumentLoaderSimTest, DocumentPolicyPolicyAttributeHistogramTest) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(true);
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");

  // Same feature should only be reported once in a document despite its
  // occurrence.
  main_resource.Complete(R"(
    <iframe policy="font-display-late-swap"></iframe>
    <iframe policy="font-display-late-swap=?0"></iframe>
    <iframe
      policy="font-display-late-swap, lossless-images-max-bpp=1.1">
    </iframe>
  )");

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.DocumentPolicy.PolicyAttribute", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.PolicyAttribute", 1 /* kFontDisplay */,
      1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.PolicyAttribute",
      2 /* kUnoptimizedLosslessImages */, 1);
}

TEST_F(DocumentLoaderSimTest, DocumentPolicyEnforcedReportHistogramTest) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Finish();

  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kFontDisplay,
      mojom::blink::PolicyDisposition::kEnforce);

  histogram_tester.ExpectTotalCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                    1);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                     1 /* kFontDisplay */, 1);

  // Multiple reports should be recorded multiple times.
  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kFontDisplay,
      mojom::blink::PolicyDisposition::kEnforce);

  histogram_tester.ExpectTotalCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                    2);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                     1 /* kFontDisplay */, 2);
}

TEST_F(DocumentLoaderSimTest, DocumentPolicyReportOnlyReportHistogramTest) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  HistogramTester histogram_tester;

  SimRequest::Params params;
  params.response_http_headers = {
      {"Document-Policy-Report-Only", "font-display-late-swap"}};
  SimRequest main_resource("https://example.com", "text/html", params);

  LoadURL("https://example.com");
  main_resource.Finish();

  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kFontDisplay,
      mojom::blink::PolicyDisposition::kReport);

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 1 /* kFontDisplay */, 1);

  // Multiple reports should be recorded multiple times.
  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kFontDisplay,
      mojom::blink::PolicyDisposition::kReport);

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 1 /* kFontDisplay */, 2);
}

class DocumentPolicyHeaderUseCounterTest
    : public DocumentLoaderSimTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {};

TEST_P(DocumentPolicyHeaderUseCounterTest, ShouldObserveUseCounterUpdate) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(true);

  bool has_document_policy_header, has_report_only_header, has_require_header;
  std::tie(has_document_policy_header, has_report_only_header,
           has_require_header) = GetParam();

  SimRequest::Params params;
  if (has_document_policy_header) {
    params.response_http_headers.insert("Document-Policy",
                                        "lossless-images-max-bpp=1.0");
  }
  if (has_report_only_header) {
    params.response_http_headers.insert("Document-Policy-Report-Only",
                                        "lossless-images-max-bpp=1.0");
  }
  if (has_require_header) {
    params.response_http_headers.insert("Require-Document-Policy",
                                        "lossless-images-max-bpp=1.0");
  }
  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Complete();

  EXPECT_EQ(
      GetDocument().IsUseCounted(mojom::WebFeature::kDocumentPolicyHeader),
      has_document_policy_header);
  EXPECT_EQ(GetDocument().IsUseCounted(
                mojom::WebFeature::kDocumentPolicyReportOnlyHeader),
            has_report_only_header);
  EXPECT_EQ(GetDocument().IsUseCounted(
                mojom::WebFeature::kRequireDocumentPolicyHeader),
            has_require_header);
}

INSTANTIATE_TEST_SUITE_P(DocumentPolicyHeaderValues,
                         DocumentPolicyHeaderUseCounterTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool(),
                                            ::testing::Bool()));

TEST_F(DocumentLoaderSimTest,
       DocumentPolicyIframePolicyAttributeUseCounterTest) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(true);
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Document-Policy", "lossless-images-max-bpp=1.0"}};
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);
  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="lossless-images-max-bpp=1.0"
    ></iframe>
  )");
  iframe_resource.Finish();

  EXPECT_TRUE(GetDocument().IsUseCounted(
      mojom::WebFeature::kDocumentPolicyIframePolicyAttribute));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(mojom::WebFeature::kRequiredDocumentPolicy));

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_document = child_frame->GetFrame()->GetDocument();

  EXPECT_FALSE(child_document->IsUseCounted(
      mojom::WebFeature::kDocumentPolicyIframePolicyAttribute));
  EXPECT_TRUE(
      child_document->IsUseCounted(mojom::WebFeature::kRequiredDocumentPolicy));
}

TEST_F(DocumentLoaderSimTest, RequiredDocumentPolicyUseCounterTest) {
  blink::ScopedDocumentPolicyForTest sdp(true);
  blink::ScopedDocumentPolicyNegotiationForTest sdpn(true);

  SimRequest::Params main_frame_params;
  main_frame_params.response_http_headers = {
      {"Require-Document-Policy", "lossless-images-max-bpp=1.0"}};
  SimRequest main_resource("https://example.com", "text/html",
                           main_frame_params);

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Document-Policy", "lossless-images-max-bpp=1.0"}};
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe src="https://example.com/foo.html"></iframe>
  )");
  iframe_resource.Finish();

  EXPECT_FALSE(GetDocument().IsUseCounted(
      mojom::WebFeature::kDocumentPolicyIframePolicyAttribute));
  EXPECT_FALSE(
      GetDocument().IsUseCounted(mojom::WebFeature::kRequiredDocumentPolicy));

  auto* child_frame = To<WebLocalFrameImpl>(MainFrame().FirstChild());
  auto* child_document = child_frame->GetFrame()->GetDocument();

  EXPECT_FALSE(child_document->IsUseCounted(
      mojom::WebFeature::kDocumentPolicyIframePolicyAttribute));
  EXPECT_TRUE(
      child_document->IsUseCounted(mojom::WebFeature::kRequiredDocumentPolicy));
}

TEST_F(DocumentLoaderTest, CommitsDeferredOnSameOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& same_origin_url =
      KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                same_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, CommitsNotDeferredOnDifferentOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest,
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
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, CommitsNotDeferredOnDifferentPortNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com:8000/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com:8000/foo.html");

  const KURL& different_port_url =
      KURL(NullURL(), "https://www.example.com:8080/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                different_port_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest,
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
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                different_port_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, CommitsNotDeferredOnDataURLNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& data_url = KURL(NullURL(), "data:,Hello%2C%20World!");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                data_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest,
       CommitsNotDeferredOnDataURLNavigationWithCrossOriginEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kPaintHoldingCrossOrigin);

  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& data_url = KURL(NullURL(), "data:,Hello%2C%20World!");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                data_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(local_frame->GetDocument()->DeferredCompositorCommitIsAllowed());
}

TEST_F(DocumentLoaderTest, SameOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& same_origin_url =
      KURL(NullURL(), "https://www.example.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                same_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_TRUE(
      local_frame->Loader().GetDocumentLoader()->IsSameOriginNavigation());
}

TEST_F(DocumentLoaderTest, CrossOriginNavigation) {
  const KURL& requestor_url =
      KURL(NullURL(), "https://www.example.com/foo.html");
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("https://example.com/foo.html");

  const KURL& other_origin_url =
      KURL(NullURL(), "https://www.another.com/bar.html");
  std::unique_ptr<WebNavigationParams> params =
      WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(),
                                                other_origin_url);
  params->requestor_origin = WebSecurityOrigin::Create(WebURL(requestor_url));
  LocalFrame* local_frame =
      To<LocalFrame>(web_view_impl->GetPage()->MainFrame());
  local_frame->Loader().CommitNavigation(std::move(params), nullptr);

  EXPECT_FALSE(
      local_frame->Loader().GetDocumentLoader()->IsSameOriginNavigation());
}

}  // namespace blink

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_disposition.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/permissions_policy/policy_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class DocumentPolicySimTest : public SimTest {
 public:
  DocumentPolicySimTest() { ResetAvailableDocumentPolicyFeaturesForTest(); }

 private:
  ScopedDocumentPolicyNegotiationForTest scoped_document_policy_negotiation_{
      true};
};

// When runtime feature DocumentPolicyNegotiation is not enabled, specifying
// Require-Document-Policy HTTP header and policy attribute on iframe should
// have no effect, i.e. document load should not be blocked even if the required
// policy and incoming policy are incompatible. Document-Policy header should
// function as normal.
TEST_F(DocumentPolicySimTest, DocumentPolicyNegotiationNoEffectWhenFlagNotSet) {
  ScopedDocumentPolicyNegotiationForTest sdpn(false);
  ResetAvailableDocumentPolicyFeaturesForTest();

  SimRequest::Params main_params;
  main_params.response_http_headers = {
      {"Require-Document-Policy", "force-load-at-top=?0"}};

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {
      {"Document-Policy", "force-load-at-top"}};

  SimRequest main_resource("https://example.com", "text/html", main_params);
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="force-load-at-top=?0">
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
  EXPECT_TRUE(console_messages.empty());

  EXPECT_EQ(child_window->Url(), KURL("https://example.com/foo.html"));

  EXPECT_FALSE(child_window->document()->IsUseCounted(
      mojom::WebFeature::kDocumentPolicyCausedPageUnload));

  // force-load-at-top should be set to false in main document.
  EXPECT_FALSE(Window().IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop));

  // force-load-at-top should be set to true in child document.
  EXPECT_TRUE(child_window->IsFeatureEnabled(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop));
}

TEST_F(DocumentPolicySimTest, ReportDocumentPolicyHeaderParsingError) {
  SimRequest::Params params;
  params.response_http_headers = {{"Document-Policy", "bad-feature-name"}};
  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  EXPECT_EQ(ConsoleMessages().size(), 1u);
  EXPECT_TRUE(
      ConsoleMessages().front().StartsWith("Document-Policy HTTP header:"));
}

TEST_F(DocumentPolicySimTest, ReportRequireDocumentPolicyHeaderParsingError) {
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

TEST_F(DocumentPolicySimTest, ReportErrorWhenDocumentPolicyIncompatible) {
  SimRequest::Params params;
  params.response_http_headers = {{"Document-Policy", "force-load-at-top"}};

  SimRequest main_resource("https://example.com", "text/html");
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             params);

  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="force-load-at-top=?0">
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
TEST_F(DocumentPolicySimTest,
       RequireDocumentPolicyHeaderShouldNotAffectCurrentDocument) {
  SimRequest::Params params;
  params.response_http_headers = {{"Require-Document-Policy", "sync-xhr=?0"},
                                  {"Document-Policy", "force-load-at-top"}};

  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  // If document is blocked by document policy because of incompatible document
  // policy, this test will fail by crashing here.
  main_resource.Finish();
}

TEST_F(DocumentPolicySimTest, DocumentPolicyHeaderHistogramTest) {
  base::HistogramTester histogram_tester;

  SimRequest::Params params;
  params.response_http_headers = {
      {"Document-Policy", "force-load-at-top, sync-xhr"}};

  SimRequest main_resource("https://example.com", "text/html", params);
  LoadURL("https://example.com");
  main_resource.Finish();

  histogram_tester.ExpectTotalCount("Blink.UseCounter.DocumentPolicy.Header",
                                    2);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Header",
                                     3 /* kForceLoadAtTop */, 1);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Header",
                                     12 /* kSyncXHR */, 1);
}

TEST_F(DocumentPolicySimTest, DocumentPolicyPolicyAttributeHistogramTest) {
  base::HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");

  // Same feature should only be reported once in a document despite its
  // occurrence.
  main_resource.Complete(R"(
    <iframe policy="force-load-at-top"></iframe>
    <iframe policy="force-load-at-top=?0"></iframe>
    <iframe
      policy="force-load-at-top,sync-xhr=?0">
    </iframe>
  )");

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.DocumentPolicy.PolicyAttribute", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.PolicyAttribute",
      3 /* kForceLoadAtTop */, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.PolicyAttribute", 12 /* kSyncXHR */, 1);
}

TEST_F(DocumentPolicySimTest, DocumentPolicyEnforcedReportHistogramTest) {
  base::HistogramTester histogram_tester;

  SimRequest main_resource("https://example.com", "text/html");
  LoadURL("https://example.com");
  main_resource.Finish();

  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop,
      mojom::blink::PolicyDisposition::kEnforce,
      "first fragment scroll violation");

  histogram_tester.ExpectTotalCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                    1);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                     3 /* kForceLoadAtTop */, 1);

  // Multiple reports should be recorded multiple times.
  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop,
      mojom::blink::PolicyDisposition::kEnforce,
      "second fragment scroll violation");

  histogram_tester.ExpectTotalCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                    2);
  histogram_tester.ExpectBucketCount("Blink.UseCounter.DocumentPolicy.Enforced",
                                     3 /* kForceLoadAtTop */, 2);
}

TEST_F(DocumentPolicySimTest, DocumentPolicyReportOnlyReportHistogramTest) {
  base::HistogramTester histogram_tester;

  SimRequest::Params params;
  params.response_http_headers = {
      {"Document-Policy-Report-Only", "force-load-at-top"}};
  SimRequest main_resource("https://example.com", "text/html", params);

  LoadURL("https://example.com");
  main_resource.Finish();

  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop,
      mojom::blink::PolicyDisposition::kReport,
      "first fragment scroll violation");

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 3 /* kForceLoadAtTop */, 1);

  // Multiple reports should be recorded multiple times.
  Window().ReportDocumentPolicyViolation(
      mojom::blink::DocumentPolicyFeature::kForceLoadAtTop,
      mojom::blink::PolicyDisposition::kReport,
      "second fragment scroll violation");

  histogram_tester.ExpectTotalCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 2);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.DocumentPolicy.ReportOnly", 3 /* kForceLoadAtTop */, 2);
}

class DocumentPolicyHeaderUseCounterTest
    : public DocumentPolicySimTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {};

TEST_P(DocumentPolicyHeaderUseCounterTest, ShouldObserveUseCounterUpdate) {
  bool has_document_policy_header, has_report_only_header, has_require_header;
  std::tie(has_document_policy_header, has_report_only_header,
           has_require_header) = GetParam();

  SimRequest::Params params;
  if (has_document_policy_header) {
    params.response_http_headers.insert("Document-Policy", "sync-xhr=?0");
  }
  if (has_report_only_header) {
    params.response_http_headers.insert("Document-Policy-Report-Only",
                                        "sync-xhr=?0");
  }
  if (has_require_header) {
    params.response_http_headers.insert("Require-Document-Policy",
                                        "sync-xhr=?0");
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

TEST_F(DocumentPolicySimTest,
       DocumentPolicyIframePolicyAttributeUseCounterTest) {
  SimRequest main_resource("https://example.com", "text/html");
  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {{"Document-Policy", "sync-xhr=?0"}};
  SimRequest iframe_resource("https://example.com/foo.html", "text/html",
                             iframe_params);
  LoadURL("https://example.com");
  main_resource.Complete(R"(
    <iframe
      src="https://example.com/foo.html"
      policy="sync-xhr=?0"
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

TEST_F(DocumentPolicySimTest, RequiredDocumentPolicyUseCounterTest) {
  SimRequest::Params main_frame_params;
  main_frame_params.response_http_headers = {
      {"Require-Document-Policy", "sync-xhr=?0"}};
  SimRequest main_resource("https://example.com", "text/html",
                           main_frame_params);

  SimRequest::Params iframe_params;
  iframe_params.response_http_headers = {{"Document-Policy", "sync-xhr=?0"}};
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

}  // namespace blink

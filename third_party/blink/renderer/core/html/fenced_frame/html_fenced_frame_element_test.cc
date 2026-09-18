// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/fenced_frame/fenced_frame_utils.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/protocol/audits.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class HTMLFencedFrameElementTest : private ScopedFencedFramesForTest,
                                   public RenderingTest {
 public:
  HTMLFencedFrameElementTest()
      : ScopedFencedFramesForTest(true),
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {
    enabled_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}}}, {/* disabled_features */});
  }

 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    SecurityContext& security_context =
        GetDocument().GetFrame()->DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://fencedframedelegate.test"));
    EXPECT_EQ(security_context.GetSecureContextMode(),
              SecureContextMode::kSecureContext);
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList enabled_feature_list_;
};

TEST_F(HTMLFencedFrameElementTest, HistogramTestInsecureContext) {
  Document& doc = GetDocument();

  SecurityContext& security_context =
      doc.GetFrame()->DomWindow()->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString("http://insecure_top_level.test"));

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->setConfig(FencedFrameConfig::Create(
      KURL("https://example.com/"),
      /*urn_uuid=*/KURL("urn:uuid:12345678-1234-5678-1234-567812345678"),
      /*container_size=*/std::nullopt, /*content_size=*/std::nullopt,
      FencedFrameConfig::AttributeVisibility::kTransparent,
      /*freeze_initial_size=*/false));
  doc.body()->AppendChild(fenced_frame);

  histogram_tester_.ExpectUniqueSample(
      kFencedFrameCreationOrNavigationOutcomeHistogram,
      FencedFrameCreationOutcome::kInsecureContext, 1);
}



TEST_F(HTMLFencedFrameElementTest, HistogramTestSandboxFlags) {
  using WebSandboxFlags = network::mojom::WebSandboxFlags;

  Document& doc = GetDocument();

  doc.GetFrame()->DomWindow()->GetSecurityContext().SetSandboxFlags(
      WebSandboxFlags::kAll);

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->SetAttributeWithoutValidation(
      html_names::kSrcAttr, AtomicString("https://test.com/"));
  doc.body()->AppendChild(fenced_frame);
  histogram_tester_.ExpectUniqueSample(
      kFencedFrameCreationOrNavigationOutcomeHistogram,
      FencedFrameCreationOutcome::kSandboxFlagsNotSet, 1);

  // Test that only the offending sandbox flags are being logged.
  for (int32_t i = 1; i <= static_cast<int32_t>(WebSandboxFlags::kMaxValue);
       i = i << 1) {
    WebSandboxFlags current_mask = static_cast<WebSandboxFlags>(i);
    histogram_tester_.ExpectBucketCount(
        kFencedFrameMandatoryUnsandboxedFlagsSandboxed, i,
        (kFencedFrameMandatoryUnsandboxedFlags & current_mask) !=
                WebSandboxFlags::kNone
            ? 1
            : 0);
  }

  // Test that it logged that the fenced frame creation attempt was in the
  // outermost main frame.
  histogram_tester_.ExpectUniqueSample(
      kFencedFrameFailedSandboxLoadInTopLevelFrame, true, 1);
}

TEST_F(HTMLFencedFrameElementTest, HistogramTestSandboxFlagsInIframe) {
  Document& doc = GetDocument();

  // Create iframe and embed it in the main document
  auto* iframe = MakeGarbageCollected<HTMLIFrameElement>(doc);
  iframe->SetAttributeWithoutValidation(html_names::kSrcAttr,
                                        AtomicString("https://test.com/"));
  doc.body()->AppendChild(iframe);
  Document* iframe_doc = iframe->contentDocument();
  iframe_doc->GetFrame()->DomWindow()->GetSecurityContext().SetSandboxFlags(
      network::mojom::blink::WebSandboxFlags::kAll);

  // Create fenced frame and embed it in the main frame
  auto* fenced_frame =
      MakeGarbageCollected<HTMLFencedFrameElement>(*iframe_doc);
  fenced_frame->SetAttributeWithoutValidation(
      html_names::kSrcAttr, AtomicString("https://test.com/"));
  iframe_doc->body()->AppendChild(fenced_frame);

  // Test that it logged that the fenced frame creation attempt was NOT in the
  // outermost main frame.
  histogram_tester_.ExpectUniqueSample(
      kFencedFrameFailedSandboxLoadInTopLevelFrame, false, 1);
}

TEST_F(HTMLFencedFrameElementTest, ReportFencedFrameRemovalOnCreation) {
  Document& doc = GetDocument();
  InspectorIssueStorage& storage = doc.GetPage()->GetInspectorIssueStorage();
  wtf_size_t initial_size = storage.size();

  MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  EXPECT_EQ(initial_size + 1, storage.size());

  auto* issue = storage.at(initial_size);
  EXPECT_EQ(protocol::Audits::InspectorIssueCodeEnum::DeprecationIssue,
            issue->getCode());
  ASSERT_TRUE(issue->getDetails()->hasDeprecationIssueDetails());
  EXPECT_EQ("FencedFrame",
            issue->getDetails()->getDeprecationIssueDetails()->getType());

  // Verify that the deprecation use counter was also bumped.
  EXPECT_TRUE(doc.IsUseCounted(WebFeature::kHTMLFencedFrameElement));
}

}  // namespace blink

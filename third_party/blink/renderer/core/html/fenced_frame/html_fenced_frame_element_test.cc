// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_ad_sizes.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HTMLFencedFrameElementTest
    : private ScopedFencedFramesForTest,
      public testing::WithParamInterface<const char*>,
      public RenderingTest {
 public:
  HTMLFencedFrameElementTest()
      : ScopedFencedFramesForTest(true),
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {
    enabled_feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames,
          {{"implementation_type", "shadow_dom"}}}},
        {/* disabled_features */});
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

INSTANTIATE_TEST_CASE_P(HTMLFencedFrameElementTest,
                        HTMLFencedFrameElementTest,
                        testing::Values("mparch", "shadow_dom"));

TEST_P(HTMLFencedFrameElementTest, FreezeSizePageZoomFactor) {
  Document& doc = GetDocument();
  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  doc.body()->AppendChild(fenced_frame);
  UpdateAllLifecyclePhasesForTest();

  LocalFrame& frame = GetFrame();
  const float zoom_factor = frame.PageZoomFactor();
  const PhysicalSize size(200, 100);
  fenced_frame->FreezeFrameSize(size);
  frame.SetPageZoomFactor(zoom_factor * 2);
  EXPECT_EQ(*fenced_frame->FrozenFrameSize(),
            PhysicalSize(size.width * 2, size.height * 2));

  frame.SetPageZoomFactor(zoom_factor);
}

TEST_P(HTMLFencedFrameElementTest, CoerceFrameSizeTest) {
  Document& doc = GetDocument();
  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->mode_ = mojom::blink::FencedFrameMode::kOpaqueAds;
  doc.body()->AppendChild(fenced_frame);

  // Check that for allowed ad sizes, coercion is a no-op.
  for (const gfx::Size& allowed_size : kAllowedAdSizes) {
    const PhysicalSize requested_size(allowed_size);
    const PhysicalSize coerced_size =
        fenced_frame->CoerceFrameSize(requested_size);
    EXPECT_EQ(requested_size, coerced_size);
  }

  // Check that all of the coercion calls were logged properly.
  histogram_tester_.ExpectBucketCount(
      "Blink.FencedFrame.IsOpaqueFrameSizeCoerced", 0, kAllowedAdSizes.size());

  // Check that for all additional test cases, the coerced size is one of the
  // allowed sizes.
  auto IsAllowedSize = [](const PhysicalSize coerced_size, int screen_width) {
    for (const gfx::Size& allowed_size : kAllowedAdSizes) {
      if (coerced_size == PhysicalSize(allowed_size)) {
        return true;
      }
    }

#if BUILDFLAG(IS_ANDROID)
    for (const int allowed_height : kAllowedAdHeights) {
      if (coerced_size == PhysicalSize(screen_width, allowed_height)) {
        return true;
      }
    }

    for (const gfx::Size& allowed_aspect_ratio : kAllowedAdAspectRatios) {
      if (coerced_size ==
          PhysicalSize(screen_width,
                       (screen_width * allowed_aspect_ratio.height()) /
                           allowed_aspect_ratio.width())) {
        return true;
      }
    }
#endif

    return false;
  };

  int screen_width = GetDocument().domWindow()->screen()->availWidth();

  std::vector<PhysicalSize> test_cases = {
      {-1, -1},
      {0, 0},
      {0, 100},
      {100, 0},
      {100, 100},
      {321, 51},
      {INT_MIN, INT_MIN},
      {INT_MIN / 2, INT_MIN / 2},
      {INT_MAX, INT_MAX},
      {INT_MAX / 2, INT_MAX / 2},
      {screen_width, 0},
      {screen_width, 50},
      {screen_width, 500},
      {screen_width + 10, 0},
      {screen_width + 10, 50},
      {screen_width + 10, 500},
      PhysicalSize(LayoutUnit(320.4), LayoutUnit(50.4)),
      PhysicalSize(LayoutUnit(320.6), LayoutUnit(50.6)),
      PhysicalSize(LayoutUnit(std::numeric_limits<double>::infinity()),
                   LayoutUnit(std::numeric_limits<double>::infinity())),
      PhysicalSize(LayoutUnit(std::numeric_limits<double>::quiet_NaN()),
                   LayoutUnit(std::numeric_limits<double>::quiet_NaN())),
      PhysicalSize(LayoutUnit(std::numeric_limits<double>::signaling_NaN()),
                   LayoutUnit(std::numeric_limits<double>::signaling_NaN())),
      PhysicalSize(LayoutUnit(std::numeric_limits<double>::denorm_min()),
                   LayoutUnit(std::numeric_limits<double>::denorm_min())),
  };

  int expected_coercion_count = 0;

  for (const PhysicalSize& requested_size : test_cases) {
    const PhysicalSize coerced_size =
        fenced_frame->CoerceFrameSize(requested_size);
    EXPECT_TRUE(IsAllowedSize(coerced_size, screen_width));

    // Coercion is not triggered for degenerate sizes
    if (!(coerced_size == requested_size) &&
        requested_size.width.ToDouble() > 0 &&
        requested_size.height.ToDouble() > 0) {
      expected_coercion_count++;
    }
  }

  // Check that all of the coercion calls were logged properly that we expect
  // to be logged.
  histogram_tester_.ExpectBucketCount(
      "Blink.FencedFrame.IsOpaqueFrameSizeCoerced", 1, expected_coercion_count);
}

TEST_P(HTMLFencedFrameElementTest, HistogramTestInsecureContext) {
  Document& doc = GetDocument();

  SecurityContext& security_context =
      doc.GetFrame()->DomWindow()->GetSecurityContext();
  security_context.SetSecurityOriginForTesting(nullptr);
  security_context.SetSecurityOrigin(
      SecurityOrigin::CreateFromString("http://insecure_top_level.test"));

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->setAttribute(html_names::kSrcAttr,
                             String("https://example.com/"),
                             ASSERT_NO_EXCEPTION);
  doc.body()->AppendChild(fenced_frame);

  histogram_tester_.ExpectUniqueSample(
      "Blink.FencedFrame.CreationOrNavigationOutcome",
      HTMLFencedFrameElement::CreationOutcome::kInsecureContext, 1);
}

TEST_P(HTMLFencedFrameElementTest, HistogramTestIncompatibleUrlHTTPDefault) {
  Document& doc = GetDocument();

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->setAttribute(html_names::kModeAttr, String("default"),
                             ASSERT_NO_EXCEPTION);
  fenced_frame->setAttribute(
      html_names::kSrcAttr, String("http://example.com/"), ASSERT_NO_EXCEPTION);
  doc.body()->AppendChild(fenced_frame);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FencedFrame.CreationOrNavigationOutcome",
      HTMLFencedFrameElement::CreationOutcome::kIncompatibleURLDefault, 1);
}

TEST_P(HTMLFencedFrameElementTest, HistogramTestIncompatibleURNDefault) {
  Document& doc = GetDocument();

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->setAttribute(html_names::kModeAttr, String("default"),
                             ASSERT_NO_EXCEPTION);
  fenced_frame->setAttribute(
      html_names::kSrcAttr,
      String("urn:uuid:12345678-1234-5678-1234-567812345678"),
      ASSERT_NO_EXCEPTION);
  doc.body()->AppendChild(fenced_frame);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FencedFrame.CreationOrNavigationOutcome",
      HTMLFencedFrameElement::CreationOutcome::kIncompatibleURLDefault, 1);
}

TEST_P(HTMLFencedFrameElementTest, HistogramTestIncompatibleUrlOpaque) {
  Document& doc = GetDocument();

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->setAttribute(html_names::kModeAttr, String("opaque-ads"),
                             ASSERT_NO_EXCEPTION);
  fenced_frame->setAttribute(
      html_names::kSrcAttr, String("http://example.com/"), ASSERT_NO_EXCEPTION);
  doc.body()->AppendChild(fenced_frame);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FencedFrame.CreationOrNavigationOutcome",
      HTMLFencedFrameElement::CreationOutcome::kIncompatibleURLOpaque, 1);
}

TEST_P(HTMLFencedFrameElementTest, HistogramTestResizeAfterFreeze) {
  Document& doc = GetDocument();

  auto* fenced_frame_opaque = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame_opaque->setAttribute(html_names::kModeAttr, String("opaque-ads"),
                                    ASSERT_NO_EXCEPTION);
  fenced_frame_opaque->setAttribute(html_names::kSrcAttr,
                                    String("https://example.com/"),
                                    ASSERT_NO_EXCEPTION);
  doc.body()->AppendChild(fenced_frame_opaque);

  // This first resize call will freeze the frame size.
  fenced_frame_opaque->OnResize(PhysicalRect(10, 20, 30, 40));

  // This second resize call will cause the resized after frozen
  // histogram to log.
  fenced_frame_opaque->OnResize(PhysicalRect(20, 30, 40, 50));

  histogram_tester_.ExpectTotalCount(
      "Blink.FencedFrame.IsFrameResizedAfterSizeFrozen", 1);
}

TEST_P(HTMLFencedFrameElementTest, HistogramTestSandboxFlags) {
  Document& doc = GetDocument();

  doc.GetFrame()->DomWindow()->GetSecurityContext().SetSandboxFlags(
      network::mojom::blink::WebSandboxFlags::kAll);

  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  fenced_frame->setAttribute(html_names::kSrcAttr, String("https://test.com/"),
                             ASSERT_NO_EXCEPTION);
  doc.body()->AppendChild(fenced_frame);
  histogram_tester_.ExpectUniqueSample(
      "Blink.FencedFrame.CreationOrNavigationOutcome",
      HTMLFencedFrameElement::CreationOutcome::kSandboxFlagsNotSet, 1);
}

}  // namespace blink

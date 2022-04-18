// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

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
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    base::FieldTrialParams params;
    params["implementation_type"] = GetParam();
    enabled_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFencedFrames, params);

    SecurityContext& security_context =
        GetDocument().GetFrame()->DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://fencedframedelegate.test"));
    EXPECT_EQ(security_context.GetSecureContextMode(),
              SecureContextMode::kSecureContext);
  }

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

  for (const PhysicalSize& requested_size : test_cases) {
    EXPECT_TRUE(IsAllowedSize(fenced_frame->CoerceFrameSize(requested_size),
                              screen_width));
  }
}

}  // namespace blink

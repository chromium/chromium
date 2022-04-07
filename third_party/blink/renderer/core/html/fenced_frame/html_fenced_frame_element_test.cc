// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class HTMLFencedFrameElementTest : private ScopedFencedFramesForTest,
                                   public RenderingTest {
 public:
  HTMLFencedFrameElementTest()
      : ScopedFencedFramesForTest(true),
        RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    base::FieldTrialParams params;
    params["implementation_type"] = "mparch";
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

TEST_F(HTMLFencedFrameElementTest, FreezeSizePageZoomFactor) {
  Document& doc = GetDocument();
  auto* fenced_frame = MakeGarbageCollected<HTMLFencedFrameElement>(doc);
  doc.body()->AppendChild(fenced_frame);

  LocalFrame& frame = GetFrame();
  const float zoom_factor = frame.PageZoomFactor();
  const PhysicalSize size(200, 100);
  fenced_frame->FreezeFrameSize(size);
  frame.SetPageZoomFactor(zoom_factor * 2);
  EXPECT_EQ(*fenced_frame->FrozenFrameSize(),
            PhysicalSize(size.width * 2, size.height * 2));

  frame.SetPageZoomFactor(zoom_factor);
}

}  // namespace blink

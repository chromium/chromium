// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"

namespace content {

class FencedFrameTest : public RenderViewHostImplTestHarness {
 public:
  FencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~FencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FencedFrameTest, FencedFrameSanityTest) {
  NavigateAndCommit(GURL("https://google.com"));
  RenderFrameHostImpl* fenced_frame_root = main_test_rfh()->AppendFencedFrame();
  EXPECT_TRUE(fenced_frame_root->IsFencedFrameRoot());
  EXPECT_FALSE(fenced_frame_root->GetPage().IsPrimary());
  EXPECT_EQ(fenced_frame_root->GetParentOrOuterDocument(), main_rfh());
  EXPECT_TRUE(fenced_frame_root->GetRenderWidgetHost()
                  ->GetView()
                  ->IsRenderWidgetHostViewChildFrame());

  // Navigate fenced frame.
  GURL fenced_frame_url = GURL("https://fencedframe.com");
  std::unique_ptr<NavigationSimulator> navigation_simulator =
      NavigationSimulator::CreateForFencedFrame(fenced_frame_url,
                                                fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());
  EXPECT_TRUE(fenced_frame_root->IsFencedFrameRoot());
  EXPECT_EQ(fenced_frame_root->GetLastCommittedURL(), fenced_frame_url);
}

}  // namespace content

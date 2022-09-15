// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"

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
      NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                   fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());
  EXPECT_TRUE(fenced_frame_root->IsFencedFrameRoot());
  EXPECT_EQ(fenced_frame_root->GetLastCommittedURL(), fenced_frame_url);
}

TEST_F(FencedFrameTest, CredentialedSubresourceRequestsAreBlocked) {
  NavigateAndCommit(GURL("https://test.org"));

  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_test_rfh())->AppendFencedFrame();

  // Navigate a fenced frame. This will fail due to credentialed subresources
  // being blocked.
  GURL fenced_frame_url =
      GURL("https://username:password@hostname/path?query#hash");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                            fenced_frame_root);
  navigation_simulator->Commit();
  EXPECT_TRUE(navigation_simulator->HasFailed());
}

TEST_F(FencedFrameTest, EnsureForcedSandboxFlagsSet) {
  NavigateAndCommit(GURL("https://test.org"));

  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(main_test_rfh())->AppendFencedFrame();

  // Ensure it has the forced sandbox flags.
  EXPECT_TRUE(
      fenced_frame_root->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));

  // Navigate fenced frame.
  GURL fenced_frame_url = GURL("https://fencedframe.com");
  std::unique_ptr<content::NavigationSimulator> navigation_simulator =
      content::NavigationSimulator::CreateRendererInitiated(fenced_frame_url,
                                                            fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());
  EXPECT_TRUE(fenced_frame_root->IsFencedFrameRoot());
  EXPECT_EQ(fenced_frame_root->GetLastCommittedURL(), fenced_frame_url);

  // Ensure it still has the forced sandbox flags.
  EXPECT_TRUE(
      fenced_frame_root->IsSandboxed(blink::kFencedFrameForcedSandboxFlags));
}

}  // namespace content

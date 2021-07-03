// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class FencedFrameTreeNodeTest
    : public RenderViewHostImplTestHarness,
      public ::testing::WithParamInterface<
          blink::features::FencedFramesImplementationType> {
 public:
  // Provides meaningful param names instead of /0 and /1.
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param) {
      case blink::features::FencedFramesImplementationType::kShadowDOM:
        return "ShadowDOM";
      case blink::features::FencedFramesImplementationType::kMPArch:
        return "MPArch";
    }
  }

  FencedFrameTreeNodeTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames,
        {{"implementation_type",
          GetParam() ==
                  blink::features::FencedFramesImplementationType::kShadowDOM
              ? "shadow_dom"
              : "mparch"}});
  }

  FrameTreeNode* AddFrame(FrameTree* frame_tree,
                          RenderFrameHostImpl* parent,
                          int process_id,
                          int new_routing_id,
                          const blink::FramePolicy& frame_policy,
                          blink::mojom::FrameOwnerElementType owner_type) {
    return frame_tree->AddFrame(
        parent, process_id, new_routing_id,
        TestRenderFrameHost::CreateStubFrameRemote(),
        TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
        TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
        blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
        false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
        frame_policy, blink::mojom::FrameOwnerProperties(), false, owner_type);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(FencedFrameTreeNodeTest, IsFencedFrame) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  FrameTree* frame_tree = contents()->GetFrameTree();
  FrameTreeNode* root = frame_tree->root();
  int process_id = root->current_frame_host()->GetProcess()->GetID();

  // Simulate attaching an iframe.
  constexpr auto kOwnerType = blink::mojom::FrameOwnerElementType::kIframe;
  AddFrame(frame_tree, root->current_frame_host(), process_id, 14,
           blink::FramePolicy(), kOwnerType);
  EXPECT_FALSE(root->child_at(0)->IsFencedFrame());
  EXPECT_FALSE(root->child_at(0)->IsInFencedFrameTree());

  // Add a fenced frame.
  // main-frame -> fenced-frame.
  constexpr auto kFencedframeOwnerType =
      blink::mojom::FrameOwnerElementType::kFencedframe;
  blink::FramePolicy policy;
  policy.is_fenced = true;
  AddFrame(frame_tree, root->current_frame_host(), process_id, 15, policy,
           kFencedframeOwnerType);
  // TODO(crbug.com/1123606): Simulate the tree to be fenced frame tree until
  // the MPArch supporting code lands.
  if (blink::features::kFencedFramesImplementationTypeParam.Get() ==
      blink::features::FencedFramesImplementationType::kMPArch) {
    root->child_at(1)->frame_tree()->SetFencedFrameTreeForTesting();
  }
  // TODO(crbug.com/1123606): Once the MPArch code lands, the FrameTreeNode that
  // we call these methods on should be either `root->child_at(1)`, or the inner
  // main FrameTreeNode that this node points to if one exists.
  EXPECT_TRUE(root->child_at(1)->IsFencedFrame());
  EXPECT_TRUE(root->child_at(1)->IsInFencedFrameTree());

  // Add a nested iframe in the fenced frame.
  // main-frame -> fenced-frame -> iframe.
  AddFrame(frame_tree, root->child_at(1)->current_frame_host(), process_id, 16,
           blink::FramePolicy(), kOwnerType);
  EXPECT_FALSE(root->child_at(1)->child_at(0)->IsFencedFrame());
  EXPECT_TRUE(root->child_at(1)->child_at(0)->IsInFencedFrameTree());

  // Add a nested fenced frame inside the existing fenced frame.
  // main-frame -> fenced-frame -> fenced-frame.
  AddFrame(frame_tree, root->child_at(1)->current_frame_host(), process_id, 17,
           policy, kFencedframeOwnerType);
  // TODO(crbug.com/1123606): Once the MPArch code lands, the FrameTreeNode that
  // we call these methods on should be either `root->child_at(1)`, or the inner
  // main FrameTreeNode that this node points to if one exists.
  EXPECT_TRUE(root->child_at(1)->child_at(1)->IsFencedFrame());
  EXPECT_TRUE(root->child_at(1)->child_at(1)->IsInFencedFrameTree());

  // Add a nested fenced frame inside the iframe added above.
  // main-frame -> iframe -> fenced-frame.
  AddFrame(frame_tree, root->child_at(0)->current_frame_host(), process_id, 18,
           policy, kFencedframeOwnerType);
  EXPECT_TRUE(root->child_at(0)->child_at(0)->IsFencedFrame());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->IsInFencedFrameTree());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    FencedFrameTreeNodeTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch),
    &FencedFrameTreeNodeTest::DescribeParams);

}  // namespace content

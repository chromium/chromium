// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

namespace content {

class FencedFrameTreeNodeTest : public RenderViewHostImplTestHarness {
 public:
  FencedFrameTreeNodeTest() {
    // Note that we only run these tests for the ShadowDOM implementation of
    // fenced frames, due to how they add subframes in a way that is very
    // specific to the ShadowDOM implementation, and not suitable for the MPArch
    // implementation. We test the MPArch implementation in
    // `FencedFrameBrowserTest`.
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames,
        {{"implementation_type", "shadow_dom"}});
  }

  FrameTreeNode* AddFrame(FrameTree& frame_tree,
                          RenderFrameHostImpl* parent,
                          int process_id,
                          int new_routing_id,
                          const blink::FramePolicy& frame_policy,
                          blink::FrameOwnerElementType owner_type) {
    return frame_tree.AddFrame(
        parent, process_id, new_routing_id,
        TestRenderFrameHost::CreateStubFrameRemote(),
        TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
        TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
        blink::mojom::TreeScopeType::kDocument, std::string(), "uniqueName0",
        false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
        frame_policy, blink::mojom::FrameOwnerProperties(), false, owner_type,
        /*is_dummy_frame_for_inner_tree=*/false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FencedFrameTreeNodeTest, IsFencedFrameHelpers) {
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* root = frame_tree.root();
  int process_id = root->current_frame_host()->GetProcess()->GetID();

  // Simulate attaching an iframe.
  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  AddFrame(frame_tree, root->current_frame_host(), process_id, 14,
           blink::FramePolicy(), kOwnerType);
  EXPECT_FALSE(root->child_at(0)->IsFencedFrameRoot());
  EXPECT_FALSE(root->child_at(0)->IsInFencedFrameTree());

  // Add a fenced frame.
  // main-frame -> fenced-frame.
  constexpr auto kFencedframeOwnerType =
      blink::FrameOwnerElementType::kFencedframe;
  blink::FramePolicy policy;
  policy.is_fenced = true;
  AddFrame(frame_tree, root->current_frame_host(), process_id, 15, policy,
           kFencedframeOwnerType);
  EXPECT_TRUE(root->child_at(1)->IsFencedFrameRoot());
  EXPECT_TRUE(root->child_at(1)->IsInFencedFrameTree());

  // Add a nested iframe in the fenced frame.
  // main-frame -> fenced-frame -> iframe.
  AddFrame(frame_tree, root->child_at(1)->current_frame_host(), process_id, 16,
           blink::FramePolicy(), kOwnerType);
  EXPECT_FALSE(root->child_at(1)->child_at(0)->IsFencedFrameRoot());
  EXPECT_TRUE(root->child_at(1)->child_at(0)->IsInFencedFrameTree());

  // Add a nested fenced frame inside the existing fenced frame.
  // main-frame -> fenced-frame -> fenced-frame.
  AddFrame(frame_tree, root->child_at(1)->current_frame_host(), process_id, 17,
           policy, kFencedframeOwnerType);
  EXPECT_TRUE(root->child_at(1)->child_at(1)->IsFencedFrameRoot());
  EXPECT_TRUE(root->child_at(1)->child_at(1)->IsInFencedFrameTree());

  // Add a nested fenced frame inside the iframe added above.
  // main-frame -> iframe -> fenced-frame.
  AddFrame(frame_tree, root->child_at(0)->current_frame_host(), process_id, 18,
           policy, kFencedframeOwnerType);
  EXPECT_TRUE(root->child_at(0)->child_at(0)->IsFencedFrameRoot());
  EXPECT_TRUE(root->child_at(0)->child_at(0)->IsInFencedFrameTree());
}

}  // namespace content

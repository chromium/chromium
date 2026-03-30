// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/digital_credential_environment.h"

#include "base/memory/raw_ptr.h"
#include "content/browser/digital_credentials/virtual_wallet.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

class DigitalCredentialEnvironmentTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    // Navigate so the main frame reaches RenderFrameState::kCreated.  Without
    // this, OnCreateChildFrame() returns early and child frames are never added
    // to the frame tree.
    NavigateAndCommit(GURL("https://example.com/"));
    env_ = DigitalCredentialEnvironment::GetInstance();
  }

  void TearDown() override {
    env_->Reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  // Returns the root FrameTreeNode for the primary frame tree.
  FrameTreeNode* RootNode() {
    return static_cast<WebContentsImpl*>(web_contents())
        ->GetPrimaryFrameTree()
        .root();
  }

  // Appends a child frame named `name` under `parent_node` and returns the
  // new child's FrameTreeNode.
  FrameTreeNode* AppendChildNode(FrameTreeNode* parent_node,
                                 const std::string& name) {
    size_t index = parent_node->child_count();
    static_cast<TestRenderFrameHost*>(parent_node->current_frame_host())
        ->AppendChild(name);
    return parent_node->child_at(index);
  }

  raw_ptr<DigitalCredentialEnvironment> env_;
};

// GetOrCreateVirtualWallet() is idempotent: successive calls on the same node
// return the same wallet instance without creating a new one.
TEST_F(DigitalCredentialEnvironmentTest, GetOrCreateIsIdempotent) {
  VirtualWallet* first = env_->GetOrCreateVirtualWallet(RootNode());
  VirtualWallet* second = env_->GetOrCreateVirtualWallet(RootNode());
  EXPECT_EQ(first, second);
}

// MaybeGetVirtualWallet() returns the wallet after it has been created.
TEST_F(DigitalCredentialEnvironmentTest, MaybeGetReturnsPresentWallet) {
  VirtualWallet* created = env_->GetOrCreateVirtualWallet(RootNode());
  ASSERT_NE(created, nullptr);
  EXPECT_EQ(env_->MaybeGetVirtualWallet(RootNode()), created);
}

// MaybeGetVirtualWallet() on a child frame returns the parent's wallet when
// the child has no wallet of its own.
TEST_F(DigitalCredentialEnvironmentTest, MaybeGetOnChildInheritsParentWallet) {
  VirtualWallet* parent_wallet = env_->GetOrCreateVirtualWallet(RootNode());
  ASSERT_NE(parent_wallet, nullptr);

  FrameTreeNode* child_node = AppendChildNode(RootNode(), "child");
  ASSERT_NE(child_node, nullptr);

  EXPECT_EQ(env_->MaybeGetVirtualWallet(child_node), parent_wallet);
}

// MaybeGetVirtualWallet() on a child frame returns the child's own wallet when
// one has been explicitly created for it, not the parent's wallet.
TEST_F(DigitalCredentialEnvironmentTest,
       MaybeGetOnChildReturnsChildWalletWhenPresent) {
  env_->GetOrCreateVirtualWallet(RootNode());

  FrameTreeNode* child_node = AppendChildNode(RootNode(), "child");
  ASSERT_NE(child_node, nullptr);
  VirtualWallet* child_wallet = env_->GetOrCreateVirtualWallet(child_node);
  ASSERT_NE(child_wallet, nullptr);

  EXPECT_EQ(env_->MaybeGetVirtualWallet(child_node), child_wallet);
}

// MaybeGetVirtualWallet() on a child frame returns nullptr when neither the
// child nor any ancestor has a wallet.
TEST_F(DigitalCredentialEnvironmentTest,
       MaybeGetOnChildReturnsNullWhenNoAncestorHasWallet) {
  FrameTreeNode* child_node = AppendChildNode(RootNode(), "child");
  ASSERT_NE(child_node, nullptr);
  EXPECT_EQ(env_->MaybeGetVirtualWallet(child_node), nullptr);
}

// Reset() removes all wallets so that MaybeGetVirtualWallet() returns nullptr
// for every previously registered node.
TEST_F(DigitalCredentialEnvironmentTest, ResetClearsAllWallets) {
  env_->GetOrCreateVirtualWallet(RootNode());
  ASSERT_NE(env_->MaybeGetVirtualWallet(RootNode()), nullptr);

  env_->Reset();

  EXPECT_EQ(env_->MaybeGetVirtualWallet(RootNode()), nullptr);
}

// Reset() is a no-op (does not crash) when no wallets have been created.
TEST_F(DigitalCredentialEnvironmentTest, ResetWithNoWalletsIsNoOp) {
  EXPECT_NO_FATAL_FAILURE(env_->Reset());
}

// Destroying a frame removes its wallet. Wallets on other frames are
// unaffected: after child_a is detached, the root's wallet is still
// returned for nodes that walk up through the root.
TEST_F(DigitalCredentialEnvironmentTest, FrameDestructionRemovesWallet) {
  // Give the root its own wallet.
  VirtualWallet* root_wallet = env_->GetOrCreateVirtualWallet(RootNode());
  ASSERT_NE(root_wallet, nullptr);

  FrameTreeNode* child_a = AppendChildNode(RootNode(), "child_a");
  ASSERT_NE(child_a, nullptr);
  VirtualWallet* child_a_wallet = env_->GetOrCreateVirtualWallet(child_a);
  ASSERT_NE(child_a_wallet, nullptr);
  ASSERT_NE(child_a_wallet, root_wallet);

  FrameTreeNode* grandchild = AppendChildNode(child_a, "grandchild");
  ASSERT_NE(grandchild, nullptr);
  ASSERT_EQ(env_->MaybeGetVirtualWallet(grandchild), child_a_wallet);

  RenderFrameHostTester::For(child_a->current_frame_host())->Detach();

  // root's wallet is still present — only child_a's wallet was removed.
  EXPECT_EQ(env_->MaybeGetVirtualWallet(RootNode()), root_wallet);
}

// Wallets created for two sibling frames are independent of each other.
TEST_F(DigitalCredentialEnvironmentTest, SiblingFramesHaveIndependentWallets) {
  FrameTreeNode* node_a = AppendChildNode(RootNode(), "child_a");
  FrameTreeNode* node_b = AppendChildNode(RootNode(), "child_b");

  ASSERT_NE(node_a, nullptr);
  ASSERT_NE(node_b, nullptr);

  VirtualWallet* wallet_a = env_->GetOrCreateVirtualWallet(node_a);
  VirtualWallet* wallet_b = env_->GetOrCreateVirtualWallet(node_b);

  ASSERT_NE(wallet_a, nullptr);
  ASSERT_NE(wallet_b, nullptr);
  EXPECT_NE(wallet_a, wallet_b);
}

}  // namespace
}  // namespace content

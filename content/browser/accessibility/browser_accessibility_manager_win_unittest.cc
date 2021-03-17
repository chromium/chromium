// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/ax_fragment_root_delegate_win.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"

namespace content {

class TestFragmentRootDelegate : public ui::AXFragmentRootDelegateWin {
 public:
  TestFragmentRootDelegate(
      BrowserAccessibilityManager* browser_accessibility_manager)
      : browser_accessibility_manager_(browser_accessibility_manager) {}
  ~TestFragmentRootDelegate() = default;

  gfx::NativeViewAccessible GetChildOfAXFragmentRoot() override {
    return browser_accessibility_manager_->GetRoot()->GetNativeViewAccessible();
  }

  gfx::NativeViewAccessible GetParentOfAXFragmentRoot() override {
    return nullptr;
  }

  bool IsAXFragmentRootAControlElement() override { return true; }

  BrowserAccessibilityManager* browser_accessibility_manager_;
};

class BrowserAccessibilityManagerWinTest : public testing::Test {
 public:
  BrowserAccessibilityManagerWinTest() = default;
  ~BrowserAccessibilityManagerWinTest() override = default;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManagerWinTest);
};

void BrowserAccessibilityManagerWinTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

TEST_F(BrowserAccessibilityManagerWinTest, DynamicallyAddedIFrame) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalUIAutomation);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> root_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root), test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(root_manager.get());

  ui::AXPlatformNode* root_document_root_node =
      ui::AXPlatformNode::FromNativeViewAccessible(
          root_manager->GetRoot()->GetNativeViewAccessible());

  std::unique_ptr<ui::AXPlatformNodeDelegate> fragment_root =
      std::make_unique<ui::AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                              &test_fragment_root_delegate);

  EXPECT_EQ(fragment_root->GetChildCount(), 1);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Simulate the case where an iframe is created but the update to add the
  // element to the root frame's document has not yet come through.
  std::unique_ptr<TestBrowserAccessibilityDelegate> iframe_delegate =
      std::make_unique<TestBrowserAccessibilityDelegate>();
  iframe_delegate->is_root_frame_ = false;
  iframe_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> iframe_manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root),
                                          iframe_delegate.get()));

  // The new frame is not a root frame, so the fragment root's lone child should
  // still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

TEST_F(BrowserAccessibilityManagerWinTest, ChildTree) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalUIAutomation);

  ui::AXNodeData child_tree_root;
  child_tree_root.id = 1;
  child_tree_root.role = ax::mojom::Role::kRootWebArea;
  ui::AXTreeUpdate child_tree_update = MakeAXTreeUpdate(child_tree_root);

  ui::AXNodeData parent_tree_root;
  parent_tree_root.id = 1;
  parent_tree_root.role = ax::mojom::Role::kRootWebArea;
  parent_tree_root.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeId,
      child_tree_update.tree_data.tree_id.ToString());
  ui::AXTreeUpdate parent_tree_update = MakeAXTreeUpdate(parent_tree_root);

  child_tree_update.tree_data.parent_tree_id =
      parent_tree_update.tree_data.tree_id;

  test_browser_accessibility_delegate_->accelerated_widget_ =
      gfx::kMockAcceleratedWidget;

  std::unique_ptr<BrowserAccessibilityManager> parent_manager(
      BrowserAccessibilityManager::Create(
          parent_tree_update, test_browser_accessibility_delegate_.get()));

  TestFragmentRootDelegate test_fragment_root_delegate(parent_manager.get());

  ui::AXPlatformNode* root_document_root_node =
      ui::AXPlatformNode::FromNativeViewAccessible(
          parent_manager->GetRoot()->GetNativeViewAccessible());

  std::unique_ptr<ui::AXPlatformNodeDelegate> fragment_root =
      std::make_unique<ui::AXFragmentRootWin>(gfx::kMockAcceleratedWidget,
                                              &test_fragment_root_delegate);

  EXPECT_EQ(fragment_root->GetChildCount(), 1);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());

  // Add the child tree.
  std::unique_ptr<TestBrowserAccessibilityDelegate> child_tree_delegate =
      std::make_unique<TestBrowserAccessibilityDelegate>();
  child_tree_delegate->is_root_frame_ = true;
  child_tree_delegate->accelerated_widget_ = gfx::kMockAcceleratedWidget;
  std::unique_ptr<BrowserAccessibilityManager> child_manager(
      BrowserAccessibilityManager::Create(child_tree_update,
                                          child_tree_delegate.get()));

  // The fragment root's lone child should still be the same as before.
  EXPECT_EQ(fragment_root->GetChildCount(), 1);
  EXPECT_EQ(fragment_root->ChildAtIndex(0),
            root_document_root_node->GetNativeViewAccessible());
}

}  // namespace content

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

#include <map>
#include <vector>

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/common/render_accessibility.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"

namespace content {
namespace {

class MockAccessibilityBridge : public ui::AccessibilityBridgeFuchsia {
 public:
  MockAccessibilityBridge() = default;
  ~MockAccessibilityBridge() override = default;

  // ui::AccessibilityBridgeFuchsia overrides.
  void UpdateNode(ui::AXNodeUpdateFuchsia node_update) override {
    node_updates_.push_back(std::move(node_update));
  }

  void DeleteNode(ui::AXNodeDescriptorFuchsia node_id) override {
    node_deletions_.push_back(node_id);
  }

  void OnAccessibilityHitTestResult(
      int hit_test_request_id,
      ui::AXNodeDescriptorFuchsia result) override {
    hit_test_results_[hit_test_request_id] = result;
  }

  const std::vector<ui::AXNodeUpdateFuchsia>& node_updates() {
    return node_updates_;
  }
  const std::vector<ui::AXNodeDescriptorFuchsia>& node_deletions() {
    return node_deletions_;
  }
  const std::map<int, ui::AXNodeDescriptorFuchsia> hit_test_results() {
    return hit_test_results_;
  }

 private:
  std::vector<ui::AXNodeUpdateFuchsia> node_updates_;
  std::vector<ui::AXNodeDescriptorFuchsia> node_deletions_;
  std::map<int /* hit test request id */,
           ui::AXNodeDescriptorFuchsia /* hit test result */>
      hit_test_results_;
};

class BrowserAccessibilityManagerFuchsiaTest : public testing::Test {
 public:
  BrowserAccessibilityManagerFuchsiaTest() = default;
  ~BrowserAccessibilityManagerFuchsiaTest() override = default;

  // testing::Test.
  void SetUp() override {
    test_browser_accessibility_delegate_ =
        std::make_unique<TestBrowserAccessibilityDelegate>();
    mock_accessibility_bridge_ = std::make_unique<MockAccessibilityBridge>();
  }

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;
  std::unique_ptr<MockAccessibilityBridge> mock_accessibility_bridge_;
};

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestEmitNodeUpdates) {
  ui::AXTreeUpdate initial_state;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  auto* registry = ui::AccessibilityBridgeFuchsiaRegistry::GetInstance();
  registry->RegisterAccessibilityBridge(tree_id,
                                        mock_accessibility_bridge_.get());
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, test_browser_accessibility_delegate_.get()));
  ASSERT_TRUE(manager);

  {
    const auto& node_updates = mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 1u);
    EXPECT_EQ(node_updates[0].node_id.node_id, 1);

    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    EXPECT_TRUE(node_deletions.empty());
  }

  // Send another update for node 1, and verify that it was passed to the
  // accessibility bridge.
  ui::AXTreeUpdate updated_state;
  updated_state.root_id = 1;
  updated_state.nodes.resize(2);
  updated_state.nodes[0].id = 1;
  updated_state.nodes[0].child_ids.push_back(2);
  updated_state.nodes[1].id = 2;

  manager->ax_tree()->Unserialize(updated_state);

  {
    const auto& node_updates = mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 3u);
    EXPECT_EQ(node_updates[1].node_id.node_id, 1);
    EXPECT_EQ(node_updates[2].node_id.node_id, 2);
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestDeleteNodes) {
  ui::AXTreeUpdate initial_state;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  auto* registry = ui::AccessibilityBridgeFuchsiaRegistry::GetInstance();
  registry->RegisterAccessibilityBridge(tree_id,
                                        mock_accessibility_bridge_.get());
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, test_browser_accessibility_delegate_.get()));
  ASSERT_TRUE(manager);

  // Verify that no deletions were received.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    EXPECT_TRUE(node_deletions.empty());
  }

  // Delete node 2.
  ui::AXTreeUpdate updated_state;
  updated_state.nodes.resize(1);
  updated_state.nodes[0].id = 1;

  manager->ax_tree()->Unserialize(updated_state);

  // Verify that the accessibility bridge received deletions for both nodes.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    ASSERT_EQ(node_deletions.size(), 1u);
    EXPECT_EQ(node_deletions[0].node_id, 2);
  }

  // Destroy manager. Doing so should force the remainder of the tree to be
  // deleted.
  manager.reset();

  // Verify that the accessibility bridge received a deletion for node 1.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    ASSERT_EQ(node_deletions.size(), 2u);
    EXPECT_EQ(node_deletions[0].node_id, 2);
    EXPECT_EQ(node_deletions[1].node_id, 1);
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestLocationChange) {
  ui::AXTreeUpdate initial_state;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  auto* registry = ui::AccessibilityBridgeFuchsiaRegistry::GetInstance();
  registry->RegisterAccessibilityBridge(tree_id,
                                        mock_accessibility_bridge_.get());
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, test_browser_accessibility_delegate_.get()));
  ASSERT_TRUE(manager);

  {
    const auto& node_updates = mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 2u);
  }

  // Send location update for node 2.
  std::vector<mojom::LocationChangesPtr> changes;
  ui::AXRelativeBounds relative_bounds;
  relative_bounds.bounds =
      gfx::RectF(/*x=*/1, /*y=*/2, /*width=*/3, /*height=*/4);
  changes.push_back(mojom::LocationChanges::New(2, relative_bounds));
  manager->OnLocationChanges(std::move(changes));

  {
    const auto& node_updates = mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 3u);
    EXPECT_EQ(node_updates.back().node_id.node_id, 2);
    ASSERT_TRUE(node_updates.back().node_data.has_location());
    const auto& location = node_updates.back().node_data.location();
    EXPECT_EQ(location.min.x, 1);
    EXPECT_EQ(location.min.y, 2);
    EXPECT_EQ(location.max.x, 4);
    EXPECT_EQ(location.max.y, 6);
  }
}

}  // namespace
}  // namespace content

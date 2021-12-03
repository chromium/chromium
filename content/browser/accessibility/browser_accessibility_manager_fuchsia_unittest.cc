// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

#include <map>
#include <vector>

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_fuchsia.h"
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
  void UpdateNode(fuchsia::accessibility::semantics::Node node) override {
    node_updates_.push_back(std::move(node));
  }

  void DeleteNode(uint32_t node_id) override {
    node_deletions_.push_back(node_id);
  }

  void FocusNode(uint32_t new_focus) override {
    new_focus_.emplace(std::move(new_focus));
  }

  void UnfocusNode(uint32_t old_focus) override {
    old_focus_.emplace(std::move(old_focus));
  }

  void OnAccessibilityHitTestResult(int hit_test_request_id,
                                    uint32_t result) override {
    hit_test_results_[hit_test_request_id] = result;
  }

  void SetRootID(uint32_t root_node_id) override {
    root_node_id_ = root_node_id;
  }

  const std::vector<fuchsia::accessibility::semantics::Node>& node_updates() {
    return node_updates_;
  }
  const std::vector<uint32_t>& node_deletions() { return node_deletions_; }
  const std::map<int, uint32_t> hit_test_results() { return hit_test_results_; }

  const absl::optional<uint32_t>& old_focus() { return old_focus_; }

  const absl::optional<uint32_t>& new_focus() { return new_focus_; }

  const absl::optional<uint32_t>& root_node_id() { return root_node_id_; }

  void reset() {
    node_updates_.clear();
    node_deletions_.clear();
    hit_test_results_.clear();
  }

 private:
  std::vector<fuchsia::accessibility::semantics::Node> node_updates_;
  std::vector<uint32_t> node_deletions_;
  std::map<int /* hit test request id */, uint32_t /* hit test result */>
      hit_test_results_;
  absl::optional<uint32_t> old_focus_;
  absl::optional<uint32_t> new_focus_;
  absl::optional<uint32_t> root_node_id_;
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

    BrowserAccessibilityFuchsia* node_1 =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(1));
    ASSERT_TRUE(node_1);

    EXPECT_EQ(node_updates[0].node_id(), node_1->GetFuchsiaNodeID());

    // Verify that the the accessibility bridge root ID was set to node 1's
    // unique ID.
    ASSERT_TRUE(mock_accessibility_bridge_->root_node_id().has_value());
    EXPECT_EQ(*mock_accessibility_bridge_->root_node_id(),
              static_cast<uint32_t>(node_1->GetFuchsiaNodeID()));

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

    BrowserAccessibilityFuchsia* node_1 =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(1));
    ASSERT_TRUE(node_1);
    BrowserAccessibilityFuchsia* node_2 =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
    ASSERT_TRUE(node_2);

    // Node 1 is the root of the root tree, so its fuchsia ID should be 0.
    EXPECT_EQ(node_updates[1].node_id(), node_1->GetFuchsiaNodeID());
    ASSERT_EQ(node_updates[1].child_ids().size(), 1u);
    EXPECT_EQ(node_updates[1].child_ids()[0], node_2->GetFuchsiaNodeID());

    // Node 2 is NOT the root, so its fuchsia ID should be its AXUniqueID.
    EXPECT_EQ(node_updates[2].node_id(), node_2->GetFuchsiaNodeID());
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

  // Get the fuchsia IDs for nodes 1 and 2 before they are deleted.
  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(1));
  ASSERT_TRUE(node_1);
  uint32_t node_1_fuchsia_id = node_1->GetFuchsiaNodeID();
  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
  ASSERT_TRUE(node_2);
  uint32_t node_2_fuchsia_id = node_2->GetFuchsiaNodeID();

  // Delete node 2.
  ui::AXTreeUpdate updated_state;
  updated_state.nodes.resize(1);
  updated_state.nodes[0].id = 1;

  manager->ax_tree()->Unserialize(updated_state);

  // Verify that the accessibility bridge received a deletion for node 2.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    ASSERT_EQ(node_deletions.size(), 1u);
    EXPECT_EQ(node_deletions[0], static_cast<uint32_t>(node_2_fuchsia_id));
  }

  // Destroy manager. Doing so should force the remainder of the tree to be
  // deleted.
  manager.reset();

  // Verify that the accessibility bridge received a deletion for node 1.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    ASSERT_EQ(node_deletions.size(), 2u);
    EXPECT_EQ(node_deletions[1], node_1_fuchsia_id);
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
    const std::vector<fuchsia::accessibility::semantics::Node>& node_updates =
        mock_accessibility_bridge_->node_updates();
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
    BrowserAccessibilityFuchsia* node_2 =
        ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
    ASSERT_TRUE(node_2);

    const std::vector<fuchsia::accessibility::semantics::Node>& node_updates =
        mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 3u);
    const fuchsia::accessibility::semantics::Node& node_update =
        node_updates.back();
    EXPECT_EQ(node_update.node_id(),
              static_cast<uint32_t>(node_2->GetFuchsiaNodeID()));
    ASSERT_TRUE(node_update.has_location());
    const fuchsia::ui::gfx::BoundingBox& location = node_update.location();
    EXPECT_EQ(location.min.x, 1);
    EXPECT_EQ(location.min.y, 2);
    EXPECT_EQ(location.max.x, 4);
    EXPECT_EQ(location.max.y, 6);
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestFocusChange) {
  // We need to specify that this is the root frame; otherwise, no focus events
  // will be fired. Likewise, we need to ensure that events are not suppressed.
  test_browser_accessibility_delegate_->is_root_frame_ = true;
  BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting();

  ui::AXTreeUpdate initial_state;
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.tree_data.parent_tree_id = ui::AXTreeIDUnknown();
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

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(1));
  ASSERT_TRUE(node_1);

  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager->GetFromID(2));
  ASSERT_TRUE(node_2);

  // Set focus to node 1, and check that the focus was updated from null to
  // node 1.
  {
    AXEventNotificationDetails event;
    ui::AXTreeUpdate updated_state;
    updated_state.tree_data.tree_id = tree_id;
    updated_state.has_tree_data = true;
    updated_state.tree_data.focused_tree_id = tree_id;
    updated_state.tree_data.focus_id = 1;
    event.ax_tree_id = tree_id;
    event.updates.push_back(std::move(updated_state));
    EXPECT_TRUE(manager->OnAccessibilityEvents(event));
  }

  ASSERT_FALSE(mock_accessibility_bridge_->old_focus());
  ASSERT_TRUE(mock_accessibility_bridge_->new_focus());
  EXPECT_EQ(*mock_accessibility_bridge_->new_focus(),
            node_1->GetFuchsiaNodeID());

  // Set focus to node 2, and check that focus was updated from node 1 to node
  // 2.
  {
    AXEventNotificationDetails event;
    ui::AXTreeUpdate updated_state;
    updated_state.tree_data.tree_id = tree_id;
    updated_state.has_tree_data = true;
    updated_state.tree_data.focused_tree_id = tree_id;
    updated_state.tree_data.focus_id = 2;
    event.ax_tree_id = tree_id;
    event.updates.push_back(std::move(updated_state));
    EXPECT_TRUE(manager->OnAccessibilityEvents(event));
  }

  ASSERT_TRUE(mock_accessibility_bridge_->old_focus());
  EXPECT_EQ(*mock_accessibility_bridge_->old_focus(),
            node_1->GetFuchsiaNodeID());
  ASSERT_TRUE(mock_accessibility_bridge_->new_focus());
  EXPECT_EQ(*mock_accessibility_bridge_->new_focus(),
            node_2->GetFuchsiaNodeID());
}

}  // namespace
}  // namespace content

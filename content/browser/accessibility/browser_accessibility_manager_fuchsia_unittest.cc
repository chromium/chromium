// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_fuchsia.h"

#include <map>
#include <vector>

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_fuchsia.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/render_accessibility.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"

namespace content {
namespace {

class MockBrowserAccessibilityDelegate
    : public TestBrowserAccessibilityDelegate {
 public:
  void AccessibilityPerformAction(const ui::AXActionData& data) override {
    last_action_data_ = data;
  }

  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(ui::AXPlatformTreeManager* hit_manager,
                              ui::AXNodeID hit_node_id)> opt_callback)
      override {
    last_hit_test_point_ = point_in_frame_pixels;
    last_request_id_ = opt_request_id;
  }

  const absl::optional<ui::AXActionData>& last_action_data() {
    return last_action_data_;
  }

  const absl::optional<int>& last_request_id() { return last_request_id_; }

  const absl::optional<gfx::Point>& last_hit_test_point() {
    return last_hit_test_point_;
  }

 private:
  absl::optional<ui::AXActionData> last_action_data_;
  absl::optional<int> last_request_id_;
  absl::optional<gfx::Point> last_hit_test_point_;
};

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

  void OnAccessibilityHitTestResult(int hit_test_request_id,
                                    absl::optional<uint32_t> result) override {
    hit_test_results_[hit_test_request_id] = result;
  }

  void SetRootID(uint32_t root_node_id) override {
    root_node_id_ = root_node_id;
  }

  inspect::Node GetInspectNode() override { return inspect::Node(); }

  float GetDeviceScaleFactor() override { return device_scale_factor_; }

  void SetDeviceScaleFactor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

  const std::vector<fuchsia::accessibility::semantics::Node>& node_updates() {
    return node_updates_;
  }
  const std::vector<uint32_t>& node_deletions() { return node_deletions_; }
  const std::map<int, absl::optional<uint32_t>>& hit_test_results() {
    return hit_test_results_;
  }

  const absl::optional<uint32_t>& old_focus() { return old_focus_; }

  const absl::optional<uint32_t>& new_focus() { return new_focus_; }

  const absl::optional<uint32_t>& root_node_id() { return root_node_id_; }

  void reset() {
    node_updates_.clear();
    node_deletions_.clear();
    hit_test_results_.clear();
  }

 private:
  float device_scale_factor_ = 1.f;
  std::vector<fuchsia::accessibility::semantics::Node> node_updates_;
  std::vector<uint32_t> node_deletions_;
  std::map<int /* hit test request id */,
           absl::optional<uint32_t> /* hit test result */>
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
    mock_browser_accessibility_delegate_ =
        std::make_unique<MockBrowserAccessibilityDelegate>();
    mock_accessibility_bridge_ = std::make_unique<MockAccessibilityBridge>();
    manager_ = std::unique_ptr<BrowserAccessibilityManager>(
        BrowserAccessibilityManager::Create(
            mock_browser_accessibility_delegate_.get()));
    static_cast<BrowserAccessibilityManagerFuchsia*>(manager_.get())
        ->SetAccessibilityBridgeForTest(mock_accessibility_bridge_.get());
  }

 protected:
  std::unique_ptr<MockBrowserAccessibilityDelegate>
      mock_browser_accessibility_delegate_;
  std::unique_ptr<MockAccessibilityBridge> mock_accessibility_bridge_;
  const content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BrowserAccessibilityManager> manager_;
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

  manager_->ax_tree()->Unserialize(initial_state);

  {
    const auto& node_updates = mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 1u);

    BrowserAccessibilityFuchsia* node_1 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
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

  manager_->ax_tree()->Unserialize(updated_state);

  {
    const auto& node_updates = mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 3u);

    BrowserAccessibilityFuchsia* node_1 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
    ASSERT_TRUE(node_1);
    BrowserAccessibilityFuchsia* node_2 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
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

  manager_->ax_tree()->Unserialize(initial_state);

  // Verify that no deletions were received.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    EXPECT_TRUE(node_deletions.empty());
  }

  // Get the fuchsia IDs for nodes 1 and 2 before they are deleted.
  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);
  uint32_t node_1_fuchsia_id = node_1->GetFuchsiaNodeID();
  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);
  uint32_t node_2_fuchsia_id = node_2->GetFuchsiaNodeID();

  // Delete node 2.
  ui::AXTreeUpdate updated_state;
  updated_state.nodes.resize(1);
  updated_state.nodes[0].id = 1;

  manager_->ax_tree()->Unserialize(updated_state);

  // Verify that the accessibility bridge received a deletion for node 2.
  {
    const auto& node_deletions = mock_accessibility_bridge_->node_deletions();
    ASSERT_EQ(node_deletions.size(), 1u);
    EXPECT_EQ(node_deletions[0], static_cast<uint32_t>(node_2_fuchsia_id));
  }

  // Destroy manager. Doing so should force the remainder of the tree to be
  // deleted.
  manager_.reset();

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

  manager_->ax_tree()->Unserialize(initial_state);

  {
    const std::vector<fuchsia::accessibility::semantics::Node>& node_updates =
        mock_accessibility_bridge_->node_updates();
    ASSERT_EQ(node_updates.size(), 2u);
  }

  // Send location update for node 2.
  std::vector<blink::mojom::LocationChangesPtr> changes;
  ui::AXRelativeBounds relative_bounds;
  relative_bounds.bounds =
      gfx::RectF(/*x=*/1, /*y=*/2, /*width=*/3, /*height=*/4);
  changes.push_back(blink::mojom::LocationChanges::New(2, relative_bounds));
  manager_->OnLocationChanges(std::move(changes));

  {
    BrowserAccessibilityFuchsia* node_2 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
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
  mock_browser_accessibility_delegate_->is_root_frame_ = true;
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

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);

  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
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
    EXPECT_TRUE(manager_->OnAccessibilityEvents(event));
  }

  {
    const std::vector<fuchsia::accessibility::semantics::Node>& node_updates =
        mock_accessibility_bridge_->node_updates();
    ASSERT_FALSE(node_updates.empty());
    EXPECT_EQ(node_updates.back().node_id(), node_1->GetFuchsiaNodeID());
    ASSERT_TRUE(node_updates.back().has_states());
    ASSERT_TRUE(node_updates.back().states().has_has_input_focus());
    EXPECT_TRUE(node_updates.back().states().has_input_focus());
  }

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
    EXPECT_TRUE(manager_->OnAccessibilityEvents(event));
  }

  {
    const std::vector<fuchsia::accessibility::semantics::Node>& node_updates =
        mock_accessibility_bridge_->node_updates();
    ASSERT_GT(node_updates.size(), 2u);
    const fuchsia::accessibility::semantics::Node& old_focus_node =
        node_updates[node_updates.size() - 2];
    EXPECT_EQ(old_focus_node.node_id(), node_1->GetFuchsiaNodeID());
    ASSERT_TRUE(old_focus_node.has_states());
    ASSERT_TRUE(old_focus_node.states().has_has_input_focus());
    EXPECT_FALSE(old_focus_node.states().has_input_focus());
    EXPECT_EQ(node_updates.back().node_id(), node_2->GetFuchsiaNodeID());
    ASSERT_TRUE(node_updates.back().has_states());
    ASSERT_TRUE(node_updates.back().states().has_has_input_focus());
    EXPECT_TRUE(node_updates.back().states().has_input_focus());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, HitTest) {
  mock_browser_accessibility_delegate_->is_root_frame_ = true;

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

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);
  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);

  // Set the hit test action data. Note that we will later hard-code the result
  // of the hit test, so the geometry doesn't matter. We just need to verify
  // that the target point specified here matches the target point received by
  // the delegate.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point.set_x(1);
  action_data.target_point.set_y(2);
  action_data.request_id = 3;

  ui::AXPlatformNodeFuchsia* platform_node =
      static_cast<ui::AXPlatformNodeFuchsia*>(
          ui::AXPlatformNodeBase::GetFromUniqueId(node_1->GetFuchsiaNodeID()));
  ASSERT_TRUE(platform_node);

  platform_node->PerformAction(action_data);

  {
    absl::optional<gfx::Point> last_target =
        mock_browser_accessibility_delegate_->last_hit_test_point();
    ASSERT_TRUE(last_target.has_value());
    EXPECT_EQ(last_target->x(), 1);
    EXPECT_EQ(last_target->y(), 2);

    absl::optional<int> last_request_id =
        mock_browser_accessibility_delegate_->last_request_id();
    ASSERT_TRUE(last_request_id.has_value());
    EXPECT_EQ(*last_request_id, action_data.request_id);
  }

  // Fire blink event to signify the hit test result.
  manager_->FireBlinkEvent(ax::mojom::Event::kHover, node_2,
                           action_data.request_id);

  {
    const std::map<int, absl::optional<uint32_t>>& hit_test_results =
        mock_accessibility_bridge_->hit_test_results();
    // We should have a hit test result for request id = 3, and the result
    // should be the fuchsia ID of node 2, which is our hit result specified
    // above.
    ASSERT_TRUE(hit_test_results.count(3));
    ASSERT_TRUE(hit_test_results.at(3).has_value());
    EXPECT_EQ(*hit_test_results.at(3), node_2->GetFuchsiaNodeID());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, HitTestFails) {
  mock_browser_accessibility_delegate_->is_root_frame_ = true;

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

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point.set_x(1);
  action_data.target_point.set_y(2);
  action_data.request_id = 4;

  ui::AXPlatformNodeFuchsia* platform_node =
      static_cast<ui::AXPlatformNodeFuchsia*>(
          ui::AXPlatformNodeBase::GetFromUniqueId(node_1->GetFuchsiaNodeID()));
  ASSERT_TRUE(platform_node);

  platform_node->PerformAction(action_data);

  {
    absl::optional<gfx::Point> last_target =
        mock_browser_accessibility_delegate_->last_hit_test_point();
    EXPECT_EQ(last_target->x(), 1);
    EXPECT_EQ(last_target->y(), 2);
  }

  // FIre blink event to signify the hit test result.
  manager_->FireBlinkEvent(ax::mojom::Event::kHover, nullptr, 4);

  {
    const std::map<int, absl::optional<uint32_t>>& hit_test_results =
        mock_accessibility_bridge_->hit_test_results();

    ASSERT_FALSE(hit_test_results.empty());
    ASSERT_TRUE(hit_test_results.count(4));
    EXPECT_FALSE(hit_test_results.at(4).has_value());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, PerformAction) {
  mock_browser_accessibility_delegate_->is_root_frame_ = true;

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

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);

  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_node_id = 2;

  ui::AXPlatformNodeFuchsia* platform_node =
      static_cast<ui::AXPlatformNodeFuchsia*>(
          ui::AXPlatformNodeBase::GetFromUniqueId(node_2->GetFuchsiaNodeID()));
  ASSERT_TRUE(platform_node);

  platform_node->PerformAction(action_data);

  {
    const absl::optional<ui::AXActionData> last_action_data =
        mock_browser_accessibility_delegate_->last_action_data();
    ASSERT_TRUE(last_action_data);
    EXPECT_EQ(last_action_data->action,
              ax::mojom::Action::kScrollToMakeVisible);
  }
}

}  // namespace
}  // namespace content

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <zircon/types.h>

#include "content/public/test/browser_test.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/browser/accessibility_bridge.h"
#include "fuchsia/engine/browser/fake_semantics_manager.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

const char kPage1Path[] = "/ax1.html";
const char kPage2Path[] = "/batching.html";
const char kPageIframePath[] = "/iframe.html";
const char kPage1Title[] = "accessibility 1";
const char kPage2Title[] = "lots of nodes!";
const char kPageIframeTitle[] = "iframe title";
const char kButtonName1[] = "a button";
const char kButtonName2[] = "another button";
const char kButtonName3[] = "button 3";
const char kNodeName[] = "last node";
const char kParagraphName[] = "a third paragraph";
const char kOffscreenNodeName[] = "offscreen node";
const size_t kPage1NodeCount = 29;
const size_t kPage2NodeCount = 190;
const size_t kInitialRangeValue = 51;
const size_t kStepSize = 3;

fuchsia::math::PointF GetCenterOfBox(fuchsia::ui::gfx::BoundingBox box) {
  fuchsia::math::PointF center;
  center.x = (box.min.x + box.max.x) / 2;
  center.y = (box.min.y + box.max.y) / 2;
  return center;
}

// Creates an AXEventNotificationDetails that contains an AxTreeUpdate that
// builds a tree from scratch of the form: (1 (2 ... (|tree_size|))).
content::AXEventNotificationDetails CreateTreeAccessibilityEvent(
    size_t tree_size) {
  content::AXEventNotificationDetails event;
  event.ax_tree_id = ui::AXTreeID ::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(tree_size);

  for (int i = 1; i <= static_cast<int>(tree_size); ++i) {
    auto& node = update.nodes[i - 1];  // vector 0-indexed, IDs 1-indexed.
    node.id = i;
    node.child_ids.push_back(i + 1);
  }

  // The deepest node does not have any child.
  update.nodes.back().child_ids.clear();
  event.updates.push_back(std::move(update));
  return event;
}

// Creates an AXEventNotificationDetails that contains |update| for the tree
// referenced by |tree_id|.
content::AXEventNotificationDetails CreateAccessibilityEventWithUpdate(
    ui::AXTreeUpdate update,
    ui::AXTreeID tree_id) {
  content::AXEventNotificationDetails event;
  event.ax_tree_id = tree_id;
  event.updates.push_back(std::move(update));
  return event;
}

// Returns whether or not the given node supports the given action.
bool HasAction(const fuchsia::accessibility::semantics::Node& node,
               fuchsia::accessibility::semantics::Action action) {
  for (const auto& node_action : node.actions()) {
    if (node_action == action)
      return true;
  }
  return false;
}

}  // namespace

class AccessibilityBridgeTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  AccessibilityBridgeTest() {
    cr_fuchsia::WebEngineBrowserTest::set_test_server_root(
        base::FilePath(cr_fuchsia::kTestServerRoot));
  }

  ~AccessibilityBridgeTest() override = default;

  AccessibilityBridgeTest(const AccessibilityBridgeTest&) = delete;
  AccessibilityBridgeTest& operator=(const AccessibilityBridgeTest&) = delete;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchNative(switches::kOzonePlatform,
                                     switches::kHeadless);
    command_line->AppendSwitch(switches::kHeadless);
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    frame_ptr_ =
        cr_fuchsia::WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_ptr_);
    frame_impl_->set_semantics_manager_for_test(&semantics_manager_);
    frame_ptr_->EnableHeadlessRendering();

    semantics_manager_.WaitUntilViewRegistered();
    ASSERT_TRUE(semantics_manager_.is_view_registered());
    ASSERT_TRUE(semantics_manager_.is_listener_valid());

    frame_ptr_->GetNavigationController(navigation_controller_.NewRequest());
    ASSERT_TRUE(embedded_test_server()->Start());

    // Change the accessibility mode on the Fuchsia side and check that it is
    // propagated correctly.
    ASSERT_FALSE(frame_impl_->web_contents_for_test()
                     ->IsWebContentsOnlyAccessibilityModeForTesting());

    semantics_manager_.SetSemanticsModeEnabled(true);
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(frame_impl_->web_contents_for_test()
                    ->IsWebContentsOnlyAccessibilityModeForTesting());
  }

  void LoadPage(base::StringPiece url, base::StringPiece page_title) {
    GURL page_url(embedded_test_server()->GetURL(std::string(url)));
    ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        navigation_controller_.get(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    navigation_listener_.RunUntilUrlAndTitleEquals(page_url, page_title);
  }

  // Helper function that checks if |num_deletes|, |num_updates| and
  // |num_commits| match the ones in the FakeSemanticTree.
  void CheckCallsToFakeSemanticTree(size_t num_deletes,
                                    size_t num_updates,
                                    size_t num_commits) {
    auto* tree = semantics_manager_.semantic_tree();
    EXPECT_EQ(tree->num_delete_calls(), num_deletes);
    EXPECT_EQ(tree->num_update_calls(), num_updates);
    EXPECT_EQ(tree->num_commit_calls(), num_commits);
  }

 protected:
  fuchsia::web::FramePtr frame_ptr_;
  FrameImpl* frame_impl_;
  FakeSemanticsManager semantics_manager_;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, CorrectDataSent) {
  LoadPage(kPage1Path, kPage1Title);

  // Check that the data values are correct in the FakeSemanticTree.
  // TODO(fxb/18796): Test more fields once Chrome to Fuchsia conversions are
  // available.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));
}

// Batching is performed when the number of nodes to send or delete exceeds the
// maximum, as set on the Fuchsia side. Check that all nodes are received by the
// Semantic Tree when batching is performed.
// TODO(1168126): Number of commit calls is not deterministic in general,
// leading to flakiness.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DISABLED_DataSentWithBatching) {
  LoadPage(kPage2Path, kPage2Title);

  // Run until we expect more than a batch's worth of nodes to be present.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_delete_calls(), 0u);

  // Checks if the actual batching happened.
  EXPECT_GE(semantics_manager_.semantic_tree()->num_update_calls(), 18u);
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_commit_calls(), 1u);
}

// Check that semantics information is correctly sent when navigating from page
// to page.
// TODO(1168126): Number of commit calls is not deterministic in general,
// leading to flakiness.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       DISABLED_NavigateFromPageToPage) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_commit_calls(), 1u);

  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));

  LoadPage(kPage2Path, kPage2Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_commit_calls(), 2u);

  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage2Title));
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));

  // Check that data from the first page has been deleted successfully.
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));
}

// Checks that the correct node ID is returned when performing hit testing.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, HitTest) {
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* target_node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName);
  EXPECT_TRUE(target_node);

  fuchsia::math::PointF target_point = GetCenterOfBox(target_node->location());

  float scale_factor = 20.f;
  // Make the bridge use scaling in hit test calculations.
  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  bridge->set_device_scale_factor_for_test(scale_factor);

  // Downscale the target point, since the hit test calculation will scale it
  // back up.
  target_point.x /= scale_factor;
  target_point.y /= scale_factor;

  uint32_t hit_node_id =
      semantics_manager_.HitTestAtPointSync(std::move(target_point));
  fuchsia::accessibility::semantics::Node* hit_node =
      semantics_manager_.semantic_tree()->GetNodeWithId(hit_node_id);

  EXPECT_EQ(hit_node->attributes().label(), kParagraphName);

  // Expect hit testing to return the root when the point given is out of
  // bounds or there is no semantic node at that position.
  target_point.x = -1;
  target_point.y = -1;
  EXPECT_EQ(0u, semantics_manager_.HitTestAtPointSync(std::move(target_point)));
  target_point.x = 1. / scale_factor;
  target_point.y = 1. / scale_factor;
  EXPECT_EQ(0u, semantics_manager_.HitTestAtPointSync(std::move(target_point)));
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, PerformDefaultAction) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* button1 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1);
  EXPECT_TRUE(button1);
  fuchsia::accessibility::semantics::Node* button2 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName2);
  EXPECT_TRUE(button2);
  fuchsia::accessibility::semantics::Node* button3 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName3);
  EXPECT_TRUE(button3);

  EXPECT_TRUE(
      HasAction(*button1, fuchsia::accessibility::semantics::Action::DEFAULT));
  EXPECT_TRUE(
      HasAction(*button2, fuchsia::accessibility::semantics::Action::DEFAULT));
  EXPECT_TRUE(
      HasAction(*button3, fuchsia::accessibility::semantics::Action::DEFAULT));

  // Perform the default action (click) on multiple buttons.
  semantics_manager_.RequestAccessibilityAction(
      button1->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  semantics_manager_.RequestAccessibilityAction(
      button2->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  semantics_manager_.RunUntilNumActionsHandledEquals(2);
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, PerformUnsupportedAction) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* button1 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1);
  EXPECT_TRUE(button1);
  fuchsia::accessibility::semantics::Node* button2 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName2);
  EXPECT_TRUE(button2);

  // Perform one supported action (DEFAULT) and one non-supported action
  // (SET_VALUE);
  semantics_manager_.RequestAccessibilityAction(
      button1->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT);
  semantics_manager_.RequestAccessibilityAction(
      button2->node_id(), fuchsia::accessibility::semantics::Action::SET_VALUE);
  semantics_manager_.RunUntilNumActionsHandledEquals(2);

  EXPECT_EQ(1, semantics_manager_.num_actions_handled());
  EXPECT_EQ(1, semantics_manager_.num_actions_unhandled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, Disconnect) {
  base::RunLoop run_loop;
  frame_ptr_.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(ZX_ERR_INTERNAL, status);
    run_loop.Quit();
  });

  semantics_manager_.semantic_tree()->Disconnect();
  run_loop.Run();
}

// TODO(crbug.com/1122806): Migrate this test to use kSignalEndOfTest and
// re-enable it.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       DISABLED_PerformScrollToMakeVisible) {
  constexpr int kScreenWidth = 720;
  constexpr int kScreenHeight = 640;
  gfx::Rect screen_bounds(kScreenWidth, kScreenHeight);

  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  auto* content_view =
      frame_impl_->web_contents_for_test()->GetContentNativeView();
  content_view->SetBounds(screen_bounds);

  // Get a node that is off the screen.
  fuchsia::accessibility::semantics::Node* node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kOffscreenNodeName);
  ASSERT_TRUE(node);
  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  ui::AXNode* ax_node = bridge->ax_tree_for_test()->GetFromId(node->node_id());
  ASSERT_TRUE(ax_node);
  bool is_offscreen = false;
  bridge->ax_tree_for_test()->GetTreeBounds(ax_node, &is_offscreen);
  EXPECT_TRUE(is_offscreen);

  // Perform SHOW_ON_SCREEN on that node and check that it is on the screen.
  base::RunLoop run_loop;
  bridge->set_event_received_callback_for_test(run_loop.QuitClosure());
  semantics_manager_.RequestAccessibilityAction(
      node->node_id(),
      fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN);
  semantics_manager_.RunUntilNumActionsHandledEquals(1);
  run_loop.Run();

  // Initialize |is_offscreen| to false before calling GetTreeBounds as
  // specified by the API.
  is_offscreen = false;
  bridge->ax_tree_for_test()->GetTreeBounds(ax_node, &is_offscreen);

  EXPECT_FALSE(is_offscreen);
}

// TODO(1168167): Test sets node-updated callback only after triggering node
// update, making it flaky.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DISABLED_Slider) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* node =
      semantics_manager_.semantic_tree()->GetNodeFromRole(
          fuchsia::accessibility::semantics::Role::SLIDER);
  EXPECT_TRUE(node);
  EXPECT_TRUE(node->has_states() && node->states().has_range_value());
  EXPECT_EQ(node->states().range_value(), kInitialRangeValue);

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  base::RunLoop run_loop;
  bridge->set_event_received_callback_for_test(run_loop.QuitClosure());
  semantics_manager_.RequestAccessibilityAction(
      node->node_id(), fuchsia::accessibility::semantics::Action::INCREMENT);
  semantics_manager_.RunUntilNumActionsHandledEquals(1);
  run_loop.Run();

  // Wait for the slider node to be updated, then check the value.
  base::RunLoop run_loop2;
  semantics_manager_.semantic_tree()->SetNodeUpdatedCallback(
      node->node_id(), run_loop2.QuitClosure());
  run_loop2.Run();

  node = semantics_manager_.semantic_tree()->GetNodeWithId(node->node_id());
  EXPECT_TRUE(node->has_states() && node->states().has_range_value());
  EXPECT_EQ(node->states().range_value(), kInitialRangeValue + kStepSize);
}

// This test makes sure that when semantic updates toggle on / off / on, the
// full semantic tree is sent in the first update when back on.
// TODO(1168126): Number of commit calls is not deterministic in general,
// leading to flakiness.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       DISABLED_TogglesSemanticsUpdates) {
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilCommitCountIs(1);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_commit_calls(), 1u);

  semantics_manager_.SetSemanticsModeEnabled(false);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(frame_impl_->web_contents_for_test()
                   ->IsWebContentsOnlyAccessibilityModeForTesting());

  // The tree gets cleared when semantic updates are off.
  EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 0u);
  semantics_manager_.SetSemanticsModeEnabled(true);
  base::RunLoop().RunUntilIdle();
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_commit_calls(), 2u);

  ASSERT_TRUE(frame_impl_->web_contents_for_test()
                  ->IsWebContentsOnlyAccessibilityModeForTesting());
}

// This test performs several tree modifications (insertions, changes, removals
// and reparentings). All operations must leave the tree in a valid state and
// also forward the nodes in a way that leaves the tree in the Fuchsia side in a
// valid state. Note that every time that a new tree is sent to Fuchsia, the
// FakeSemantiTree checks if the tree is valid.
// TODO(1168126): Number of commit calls is not deterministic in general,
// leading to flakiness.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       DISABLED_TreeModificationsAreForwarded) {
  // Loads a page, so a real frame is created for this test. Then, several tree
  // operations are applied on top of it, using the AXTreeID that corresponds to
  // that frame.
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilCommitCountIs(1);

  // Fetch the AXTreeID of the main frame (the page just loaded). This ID will
  // be used in the operations that follow to simulate new data coming in.
  auto tree_id =
      frame_impl_->web_contents_for_test()->GetMainFrame()->GetAXTreeID();

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  size_t tree_size = 5;

  // The tree has the following form: (1 (2 (3 (4 (5)))))
  auto tree_accessibility_event = CreateTreeAccessibilityEvent(tree_size);
  tree_accessibility_event.ax_tree_id = tree_id;

  // The root of this tree needs to be cleared (because it holds the page just
  // loaded, and we are loading something completely new).
  tree_accessibility_event.updates[0].node_id_to_clear =
      bridge->ax_tree_for_test()->root()->id();

  bridge->AccessibilityEventReceived(tree_accessibility_event);

  semantics_manager_.semantic_tree()->RunUntilCommitCountIs(2);
  CheckCallsToFakeSemanticTree(/*num_deletes=*/1, /*num_updates=*/3,
                               /*num_commits=*/2);

  // Adds a new node with ID 6.
  // (1 (2 (3 (4 (5 6)))))
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 4;
    update.nodes[0].child_ids.push_back(5);
    update.nodes[0].child_ids.push_back(6);
    update.nodes[1].id = 6;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(3);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/1, /*num_updates=*/4,
                                 /*num_commits=*/3);
  }

  // Removes the added node 6.
  // (1 (2 (3 (4 (5)))))
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.node_id_to_clear = 4;
    update.nodes.resize(2);
    update.nodes[0].id = 4;
    update.nodes[0].child_ids.push_back(5);

    update.nodes[1].id = 5;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(4);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/2, /*num_updates=*/5,
                                 /*num_commits=*/4);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), tree_size);
  }

  // Reparents node 5 to be a child of node 3.
  // (1 (2 (3 (4 5))))
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.node_id_to_clear = 3;
    update.nodes.resize(3);
    update.nodes[0].id = 3;
    update.nodes[0].child_ids.push_back(4);
    update.nodes[0].child_ids.push_back(5);

    update.nodes[1].id = 4;
    update.nodes[2].id = 5;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(5);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/2, /*num_updates=*/6,
                                 /*num_commits=*/5);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), tree_size);
  }

  // Reparents the subtree rooted at node 3 to be a child of node 1.
  // (1 (2 3 (4 5)))
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.node_id_to_clear = 2;
    update.nodes.resize(5);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids.push_back(2);
    update.nodes[0].child_ids.push_back(3);

    update.nodes[1].id = 2;
    update.nodes[2].id = 3;
    update.nodes[2].child_ids.push_back(4);
    update.nodes[2].child_ids.push_back(5);

    update.nodes[3].id = 4;
    update.nodes[4].id = 5;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(6);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/2, /*num_updates=*/7,
                                 /*num_commits=*/6);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), tree_size);
  }

  // Deletes the subtree rooted at node 3.
  // (1 (2))
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.node_id_to_clear = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 1;
    update.nodes[0].child_ids.push_back(2);

    update.nodes[1].id = 2;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(7);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/3, /*num_updates=*/8,
                                 /*num_commits=*/7);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 2u);
  }

  // Give this tree a new root.
  // (7 (2))
  {
    ui::AXTreeUpdate update;
    update.root_id = 7;
    update.node_id_to_clear = 1;
    update.nodes.resize(2);
    update.nodes[0].id = 7;
    update.nodes[0].child_ids.push_back(2);

    update.nodes[1].id = 2;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(8);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/4, /*num_updates=*/9,
                                 /*num_commits=*/8);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 2u);
  }

  // Delete child and change root ID.
  // (1)
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.node_id_to_clear = 7;
    update.nodes.resize(1);
    update.nodes[0].id = 1;

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilCommitCountIs(9);
    CheckCallsToFakeSemanticTree(/*num_deletes=*/5, /*num_updates=*/10,
                                 /*num_commits=*/9);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 1u);
  }
}

// TODO(crbug.com/1167266): Number of commit calls is not deterministic in
// general, leading to flakiness. Hard-wired wait for 200ms may also lead to
// expectations failures if the system running tests is overloaded.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DISABLED_OutOfProcessIframe) {
  constexpr int64_t kBindingsId = 1234;

  // Start a different embedded test server, and load a page on it. The URL for
  // this page will have a different port and be considered out of process when
  // used as the src for an iframe.
  net::EmbeddedTestServer second_test_server;
  second_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(cr_fuchsia::kTestServerRoot));
  ASSERT_TRUE(second_test_server.Start());
  GURL out_of_process_url = second_test_server.GetURL(kPage1Path);

  // Before loading a page on the default embedded test server, set the iframe
  // src to be |out_of_process_url|.
  frame_ptr_->AddBeforeLoadJavaScript(
      kBindingsId, {"*"},
      cr_fuchsia::MemBufferFromString(
          base::StringPrintf("iframeSrc = '%s'",
                             out_of_process_url.spec().c_str()),
          "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        CHECK(result.is_response());
      });
  LoadPage(kPageIframePath, "iframe loaded");

  semantics_manager_.semantic_tree()->RunUntilCommitCountIs(1);

  // Run message loop for 200ms to ensure that all AX updates from the iframes
  // are processed.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(200));
  run_loop.Run();

  // Two frames should be present.
  int num_frames = frame_impl_->web_contents_for_test()->GetAllFrames().size();
  EXPECT_EQ(num_frames, 2);

  // Check that the iframe node has been loaded.
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPageIframeTitle));

  // Data that is part of the iframe should be in the semantic tree.
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));

  // Makes the iframe navigate to a different page.
  GURL out_of_process_url_2 = second_test_server.GetURL(kPage2Path);
  const auto script =
      base::StringPrintf("document.getElementById(\"iframeId\").src = '%s'",
                         out_of_process_url_2.spec().c_str());

  frame_ptr_->ExecuteJavaScript(
      {"*"}, cr_fuchsia::MemBufferFromString(script, "test2"),
      [](fuchsia::web::Frame_ExecuteJavaScript_Result result) {
        CHECK(result.is_response());
      });

  semantics_manager_.semantic_tree()->RunUntilCommitCountIs(4);

  // check that the iframe navigated to a different page.
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));

  // Old iframe data should be gone.
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));

  // Makes the main page navigate to a different page, causing the iframe to go
  // away.
  LoadPage(kPage2Path, kPage2Title);

  semantics_manager_.semantic_tree()->RunUntilCommitCountIs(5);

  // We've navigated to a different page that has no iframes. Only one frame
  // should be present.
  num_frames = frame_impl_->web_contents_for_test()->GetAllFrames().size();
  EXPECT_EQ(num_frames, 1);
}

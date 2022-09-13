// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <zircon/types.h>

#include "base/command_line.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/accessibility_bridge.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/fake_semantics_manager.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/test/frame_for_test.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_observer.h"
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
const char kUpdate1Name[] = "update1";
const char kUpdate2Name[] = "update2";
const char kUpdate3Name[] = "update3";
const char kUpdate4Name[] = "update4";
const char kUpdate5Name[] = "update5";
const char kUpdate6Name[] = "update6";
const char kUpdate7Name[] = "update7";
const char kUpdate8Name[] = "update8";
const size_t kPage1NodeCount = 29;
const size_t kPage2NodeCount = 190;
const size_t kInitialRangeValue = 51;
const size_t kStepSize = 3;

// Simulated screen bounds to use when testing the SemanticsManager.
constexpr gfx::Size kTestWindowSize = {720, 640};

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

class AccessibilityBridgeTest : public WebEngineBrowserTest {
 public:
  AccessibilityBridgeTest() {
    WebEngineBrowserTest::set_test_server_root(base::FilePath(kTestServerRoot));
  }

  ~AccessibilityBridgeTest() override = default;

  AccessibilityBridgeTest(const AccessibilityBridgeTest&) = delete;
  AccessibilityBridgeTest& operator=(const AccessibilityBridgeTest&) = delete;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchNative(switches::kOzonePlatform,
                                     switches::kHeadless);
    command_line->AppendSwitch(switches::kHeadless);
    WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    frame_ = FrameForTest::Create(context(), {});
    base::RunLoop().RunUntilIdle();

    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_.ptr());
    frame_impl_->set_semantics_manager_for_test(&semantics_manager_);
    frame_impl_->set_window_size_for_test(kTestWindowSize);

    // TODO(crbug.com/1291330): Remove uses of
    // set_use_v2_accessibility_bridge().
    frame_impl_->set_use_v2_accessibility_bridge(false);
    frame_->EnableHeadlessRendering();

    semantics_manager_.WaitUntilViewRegistered();
    ASSERT_TRUE(semantics_manager_.is_view_registered());
    ASSERT_TRUE(semantics_manager_.is_listener_valid());

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
    ASSERT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame_.navigation_listener().RunUntilUrlAndTitleEquals(page_url,
                                                           page_title);
  }

 protected:
  FrameForTest frame_;
  FrameImpl* frame_impl_;
  FakeSemanticsManager semantics_manager_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, CorrectDataSent) {
  LoadPage(kPage1Path, kPage1Title);

  // Check that the data values are correct in the FakeSemanticTree.
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
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, DataSentWithBatching) {
  LoadPage(kPage2Path, kPage2Title);

  // Run until we expect more than a batch's worth of nodes to be present.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));
  EXPECT_EQ(semantics_manager_.semantic_tree()->num_delete_calls(), 0u);

  // Checks if the actual batching happened.
  EXPECT_GE(semantics_manager_.semantic_tree()->num_update_calls(), 18u);

  // Checks if one or more commit calls were made to send the data.
  EXPECT_GE(semantics_manager_.semantic_tree()->num_commit_calls(), 1u);
}

// Check that semantics information is correctly sent when navigating from page
// to page.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, NavigateFromPageToPage) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kPage1Title));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_TRUE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));

  LoadPage(kPage2Path, kPage2Title);

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kPage2Title);

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
  frame_.ptr().set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(ZX_ERR_INTERNAL, status);
    run_loop.Quit();
  });

  semantics_manager_.semantic_tree()->Disconnect();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, PerformScrollToMakeVisible) {
  // Set the screen height to be small so that we can detect if we've
  // scrolled past our target, even if the max scroll is bounded.
  constexpr int kScreenWidth = 720;
  constexpr int kScreenHeight = 20;
  gfx::Rect screen_bounds(kScreenWidth, kScreenHeight);

  LoadPage(kPage1Path, kPage1Title);

  auto* semantic_tree = semantics_manager_.semantic_tree();
  ASSERT_TRUE(semantic_tree);

  semantic_tree->RunUntilNodeCountAtLeast(kPage1NodeCount);

  auto* content_view =
      frame_impl_->web_contents_for_test()->GetContentNativeView();
  content_view->SetBounds(screen_bounds);

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();

  // Get a node that is off the screen, and verify that it is off the screen.
  fuchsia::accessibility::semantics::Node* fuchsia_node =
      semantic_tree->GetNodeFromLabel(kOffscreenNodeName);
  ASSERT_TRUE(fuchsia_node);

  // Get the corresponding AXNode.
  auto ax_node_id = bridge->node_id_mapper_for_test()
                        ->ToAXNodeID(fuchsia_node->node_id())
                        ->second;
  ui::AXNode* ax_node = bridge->ax_tree_for_test()->GetFromId(ax_node_id);
  ASSERT_TRUE(ax_node);
  bool is_offscreen = false;
  bridge->ax_tree_for_test()->GetTreeBounds(ax_node, &is_offscreen);
  EXPECT_TRUE(is_offscreen);

  // Perform SHOW_ON_SCREEN on that node.
  semantics_manager_.RequestAccessibilityAction(
      fuchsia_node->node_id(),
      fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN);
  semantics_manager_.RunUntilNumActionsHandledEquals(1);

  semantic_tree->RunUntilConditionIsTrue(
      base::BindLambdaForTesting([semantic_tree]() {
        auto* root = semantic_tree->GetNodeWithId(0u);
        if (!root)
          return false;

        // Once the scroll action has been handled, the root should have a
        // non-zero y-scroll offset.
        return root->has_states() && root->states().has_viewport_offset() &&
               root->states().viewport_offset().y > 0;
      }));

  // Verify that the AXNode we tried to make visible is now onscreen.
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
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, TogglesSemanticsUpdates) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  semantics_manager_.SetSemanticsModeEnabled(false);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(frame_impl_->web_contents_for_test()
                   ->IsWebContentsOnlyAccessibilityModeForTesting());

  // The tree gets cleared when semantic updates are off.
  EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 0u);
  semantics_manager_.SetSemanticsModeEnabled(true);
  base::RunLoop().RunUntilIdle();
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  ASSERT_TRUE(frame_impl_->web_contents_for_test()
                  ->IsWebContentsOnlyAccessibilityModeForTesting());
}

// This test performs several tree modifications (insertions, changes, removals
// and reparentings). All operations must leave the tree in a valid state and
// also forward the nodes in a way that leaves the tree in the Fuchsia side in a
// valid state. Note that every time that a new tree is sent to Fuchsia, the
// FakeSemantiTree checks if the tree is valid.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, TreeModificationsAreForwarded) {
  // Loads a page, so a real frame is created for this test. Then, several tree
  // operations are applied on top of it, using the AXTreeID that corresponds to
  // that frame.
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Fetch the AXTreeID of the main frame (the page just loaded). This ID will
  // be used in the operations that follow to simulate new data coming in.
  auto tree_id = frame_impl_->web_contents_for_test()
                     ->GetPrimaryMainFrame()
                     ->GetAXTreeID();

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  size_t tree_size = 5;

  // The tree has the following form: (1 (2 (3 (4 (5)))))
  auto tree_accessibility_event = CreateTreeAccessibilityEvent(tree_size);
  tree_accessibility_event.ax_tree_id = tree_id;

  // The root of this tree needs to be cleared (because it holds the page just
  // loaded, and we are loading something completely new).
  tree_accessibility_event.updates[0].node_id_to_clear =
      bridge->ax_tree_for_test()->root()->id();

  // Set a name in a node so we can wait for this node to appear. This pattern
  // is used throughout this test to ensure that the new data we are waiting for
  // arrived.
  tree_accessibility_event.updates[0].nodes[0].role =
      ax::mojom::Role::kStaticText;
  tree_accessibility_event.updates[0].nodes[0].SetName(kUpdate1Name);

  bridge->AccessibilityEventReceived(tree_accessibility_event);

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate1Name);

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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate2Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate2Name);
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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate3Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate3Name);
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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate4Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate4Name);
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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate5Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate5Name);
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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate6Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate6Name);
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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate7Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate7Name);
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
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate8Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));

    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate8Name);
    EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 1u);
  }
}

// Verifies that offset container bookkeeping is updated correctly.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       OffsetContainerBookkeepingIsUpdated) {
  // Loads a page, so a real frame is created for this test. Then, several tree
  // operations are applied on top of it, using the AXTreeID that corresponds to
  // that frame.
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Fetch the AXTreeID of the main frame (the page just loaded). This ID will
  // be used in the operations that follow to simulate new data coming in.
  auto tree_id = frame_impl_->web_contents_for_test()
                     ->GetPrimaryMainFrame()
                     ->GetAXTreeID();

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  size_t tree_size = 5;

  // The tree has the following form: (1 (2 (3 (4 (5)))))
  auto tree_accessibility_event = CreateTreeAccessibilityEvent(tree_size);
  tree_accessibility_event.ax_tree_id = tree_id;

  // The root of this tree needs to be cleared (because it holds the page just
  // loaded, and we are loading something completely new).
  tree_accessibility_event.updates[0].node_id_to_clear =
      bridge->ax_tree_for_test()->root()->id();

  // Set a name in a node so we can wait for this node to appear. This pattern
  // is used throughout this test to ensure that the new data we are waiting for
  // arrived.
  tree_accessibility_event.updates[0].nodes[0].role =
      ax::mojom::Role::kStaticText;
  tree_accessibility_event.updates[0].nodes[0].SetName(kUpdate1Name);

  bridge->AccessibilityEventReceived(tree_accessibility_event);

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate1Name);

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
    update.nodes[1].relative_bounds.offset_container_id = 3;
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(kUpdate2Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate2Name);
  }

  // Then, verify that offset container bookkeeping was updated.
  {
    auto* tree = bridge->ax_tree_for_test();
    auto offset_container_children_it = bridge->offset_container_children_.find(
        std::make_pair(tree->GetAXTreeID(), 3));
    EXPECT_NE(offset_container_children_it,
              bridge->offset_container_children_.end());
    const auto& offset_children = offset_container_children_it->second;
    EXPECT_EQ(offset_children.size(), 1u);
    EXPECT_TRUE(offset_children.count(std::make_pair(tree->GetAXTreeID(), 6)));
  }

  // Now, change node 6's offset container to be node 4.
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.nodes.resize(1);
    update.nodes[0].id = 6;
    update.nodes[0].relative_bounds.offset_container_id = 4;
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate3Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate3Name);
  }

  // Then, verify that offset container bookkeeping was updated.
  {
    // Check that node 6 was deleted from node 3's offset children.
    auto* tree = bridge->ax_tree_for_test();
    auto offset_container_children_it = bridge->offset_container_children_.find(
        std::make_pair(tree->GetAXTreeID(), 3));
    EXPECT_NE(offset_container_children_it,
              bridge->offset_container_children_.end());
    const auto& offset_children = offset_container_children_it->second;
    EXPECT_TRUE(offset_children.empty());

    // Check that node 6 was added to node 4's offset children.
    auto new_offset_container_children_it =
        bridge->offset_container_children_.find(
            std::make_pair(tree->GetAXTreeID(), 4));
    EXPECT_NE(offset_container_children_it,
              bridge->offset_container_children_.end());
    const auto& new_offset_children = new_offset_container_children_it->second;
    EXPECT_EQ(new_offset_children.size(), 1u);
    EXPECT_TRUE(
        new_offset_children.count(std::make_pair(tree->GetAXTreeID(), 6)));
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
    update.nodes[1].role = ax::mojom::Role::kStaticText;
    update.nodes[1].SetName(kUpdate4Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate4Name);
  }

  // Verify that node 6 was removed as an offset child of node 4.
  {
    auto* tree = bridge->ax_tree_for_test();
    auto offset_container_children_it = bridge->offset_container_children_.find(
        std::make_pair(tree->GetAXTreeID(), 4));
    EXPECT_NE(offset_container_children_it,
              bridge->offset_container_children_.end());
    const auto& offset_children = offset_container_children_it->second;
    EXPECT_TRUE(offset_children.empty());
  }

  // Removes node 4.
  // (1 (2 (3 )))
  {
    ui::AXTreeUpdate update;
    update.root_id = 1;
    update.node_id_to_clear = 3;
    update.nodes.resize(1);
    update.nodes[0].id = 3;
    update.nodes[0].role = ax::mojom::Role::kStaticText;
    update.nodes[0].SetName(kUpdate5Name);

    bridge->AccessibilityEventReceived(
        CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
    semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
        kUpdate5Name);
  }

  // Verify that node 4 was cleared from the offset children map.
  {
    auto* tree = bridge->ax_tree_for_test();
    EXPECT_FALSE(bridge->offset_container_children_.count(
        std::make_pair(tree->GetAXTreeID(), 4)));
  }
}

// This test verifies that a node's transform includes a translation for its
// offset container's bounds.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       TransformAccountsForOffsetContainerBounds) {
  // Loads a page, so a real frame is created for this test. Then, several tree
  // operations are applied on top of it, using the AXTreeID that corresponds to
  // that frame.
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Fetch the AXTreeID of the main frame (the page just loaded). This ID will
  // be used in the operations that follow to simulate new data coming in.
  auto tree_id = frame_impl_->web_contents_for_test()
                     ->GetPrimaryMainFrame()
                     ->GetAXTreeID();

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  size_t tree_size = 5;

  // The tree has the following form: (1 (2 (3 (4 (5)))))
  auto tree_accessibility_event = CreateTreeAccessibilityEvent(tree_size);
  tree_accessibility_event.ax_tree_id = tree_id;

  // The root of this tree needs to be cleared (because it holds the page just
  // loaded, and we are loading something completely new).
  tree_accessibility_event.updates[0].node_id_to_clear =
      bridge->ax_tree_for_test()->root()->id();

  // Set a name in a node so we can wait for this node to appear. This pattern
  // is used throughout this test to ensure that the new data we are waiting for
  // arrived.
  tree_accessibility_event.updates[0].nodes[0].role =
      ax::mojom::Role::kStaticText;
  tree_accessibility_event.updates[0].nodes[0].SetName(kUpdate1Name);

  bridge->AccessibilityEventReceived(tree_accessibility_event);

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate1Name);

  const char kUpdateNodeName[] = "transfrom should update";
  // Changes the bounds of node 1.
  // (1 (2 (3 (4 (5)))))
  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(2);
  update.nodes[0].id = 1;
  // Update the relative bounds of node 1, which is node 2's offset container.
  update.nodes[0].relative_bounds.bounds = gfx::RectF(2, 3, 4, 5);
  update.nodes[0].child_ids = {2};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(kUpdate2Name);
  update.nodes[1].id = 2;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[1].SetName(kUpdateNodeName);
  update.nodes[1].relative_bounds.offset_container_id = 1;
  // Node 2 should have non-trivial relative bounds to ensure that the
  // accessibility bridge correctly composes node 2's transform and the
  // translation for node 1's bounds.
  update.nodes[1].relative_bounds.bounds = gfx::RectF(10, 11, 10, 11);
  update.nodes[1].relative_bounds.transform = std::make_unique<gfx::Transform>(
      5, 0, 0, 100, 0, 5, 0, 200, 0, 0, 5, 0, 0, 0, 0, 1);
  bridge->AccessibilityEventReceived(
      CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate2Name);

  auto* tree = bridge->ax_tree_for_test();
  auto* updated_node = tree->GetFromId(2);
  ASSERT_TRUE(updated_node);

  // Verify that the transform for the Fuchsia semantic node corresponding to
  // node 2 reflects the new bounds of node 1.
  fuchsia::accessibility::semantics::Node* fuchsia_node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kUpdateNodeName);
  ASSERT_TRUE(fuchsia_node);
  // A Fuchsia node's semantic transform should include an offset for its parent
  // node as a post-translation on top of its existing transform. Therefore, the
  // x, y, and z scale (indices 0, 5, and 10, respectively) should remain
  // unchanged, and the x and y bounds of the offset container should be added
  // to the node's existing translation entries (indices 12 and 13).
  EXPECT_EQ(fuchsia_node->transform().matrix[0], 5);
  EXPECT_EQ(fuchsia_node->transform().matrix[5], 5);
  EXPECT_EQ(fuchsia_node->transform().matrix[10], 5);
  EXPECT_EQ(fuchsia_node->transform().matrix[12], 102);
  EXPECT_EQ(fuchsia_node->transform().matrix[13], 203);
}

// This test verifies that a node's transform is updated correctly when its
// container's relative bounds change.
// NOTE: This test is distinct from the above test case.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest,
                       UpdateTransformWhenContainerBoundsChange) {
  // Loads a page, so a real frame is created for this test. Then, several tree
  // operations are applied on top of it, using the AXTreeID that corresponds to
  // that frame.
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Fetch the AXTreeID of the main frame (the page just loaded). This ID will
  // be used in the operations that follow to simulate new data coming in.
  auto tree_id = frame_impl_->web_contents_for_test()
                     ->GetPrimaryMainFrame()
                     ->GetAXTreeID();

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  size_t tree_size = 5;

  // The tree has the following form: (1 (2 (3 (4 (5)))))
  auto tree_accessibility_event = CreateTreeAccessibilityEvent(tree_size);
  tree_accessibility_event.ax_tree_id = tree_id;

  // The root of this tree needs to be cleared (because it holds the page just
  // loaded, and we are loading something completely new).
  tree_accessibility_event.updates[0].node_id_to_clear =
      bridge->ax_tree_for_test()->root()->id();

  // Set a name in a node so we can wait for this node to appear. This pattern
  // is used throughout this test to ensure that the new data we are waiting for
  // arrived.
  tree_accessibility_event.updates[0].nodes[0].role =
      ax::mojom::Role::kStaticText;
  tree_accessibility_event.updates[0].nodes[0].SetName(kUpdate1Name);

  bridge->AccessibilityEventReceived(tree_accessibility_event);

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate1Name);

  // Ensure that the accessibility bridge's offset container bookkeeping is up
  // to date.
  bridge->offset_container_children_[std::make_pair(tree_id, 1)].insert(
      std::make_pair(tree_id, 2));

  const char kUpdateNodeName[] = "transfrom should update";
  // Changes the bounds of node 1.
  // (1 (2 (3 (4 (5)))))
  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(2);
  update.nodes[0].id = 1;
  // Update the relative bounds of node 1, which is node 2's offset container.
  update.nodes[0].relative_bounds.bounds = gfx::RectF(2, 3, 4, 5);
  update.nodes[0].child_ids = {2};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(kUpdate2Name);
  update.nodes[1].id = 2;
  update.nodes[1].role = ax::mojom::Role::kStaticText;
  update.nodes[1].SetName(kUpdateNodeName);
  bridge->AccessibilityEventReceived(
      CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate2Name);

  // Verify that the transform for the Fuchsia semantic node corresponding to
  // node 2 reflects the new bounds of node 1.
  fuchsia::accessibility::semantics::Node* fuchsia_node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kUpdateNodeName);

  // A Fuchsia node's semantic transform should include an offset for its parent
  // node as a post-translation on top of its existing transform. Therefore, the
  // x, y, and z scale (indices 0, 5, and 10, respectively) should remain
  // unchanged, and the x and y bounds of the offset container should be added
  // to the node's existing translation entries (indices 12 and 13).
  EXPECT_EQ(fuchsia_node->transform().matrix[12], 2);
  EXPECT_EQ(fuchsia_node->transform().matrix[13], 3);
}

// This test ensures that fuchsia only receives one update per node.
IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, OneUpdatePerNode) {
  // Loads a page, so a real frame is created for this test. Then, several tree
  // operations are applied on top of it, using the AXTreeID that corresponds to
  // that frame.
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Fetch the AXTreeID of the main frame (the page just loaded). This ID will
  // be used in the operations that follow to simulate new data coming in.
  auto tree_id = frame_impl_->web_contents_for_test()
                     ->GetPrimaryMainFrame()
                     ->GetAXTreeID();

  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  size_t tree_size = 5;

  // The tree has the following form: (1 (2 (3 (4 (5)))))
  auto tree_accessibility_event = CreateTreeAccessibilityEvent(tree_size);
  tree_accessibility_event.ax_tree_id = tree_id;

  // The root of this tree needs to be cleared (because it holds the page just
  // loaded, and we are loading something completely new).
  tree_accessibility_event.updates[0].node_id_to_clear =
      bridge->ax_tree_for_test()->root()->id();

  // Set a name in a node so we can wait for this node to appear. This pattern
  // is used throughout this test to ensure that the new data we are waiting for
  // arrived.
  tree_accessibility_event.updates[0].nodes[0].role =
      ax::mojom::Role::kStaticText;
  tree_accessibility_event.updates[0].nodes[0].SetName(kUpdate1Name);

  bridge->AccessibilityEventReceived(tree_accessibility_event);

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdate1Name);

  // Mark node 2 as node 3's offset container. Below, we will send an update to
  // change node 3's offset container to node 1, so that we can verify that the
  // fuchsia node produced reflects that update.
  bridge->offset_container_children_[std::make_pair(tree_id, 2)].insert(
      std::make_pair(tree_id, 3));

  // Send three updates:
  // 1. Change bounds for node 1.
  // 2. Change bounds for node 2. Since node 3 is marked as node 2's offset
  // child (above), OnNodeDataChanged() should produce an update for node 3 that
  // includes a transform accounting for node 2's new bounds.
  // 3. Change offset container for node 3 from node 2 to node 1.
  // OnAtomicUpdateFinished() should replace the now-incorrect update from step
  // (2) with a new update that includes a transform accounting for node 1's
  // bounds.
  const char kUpdateNodeName[] = "transform should update";
  // Changes the bounds of node 1.
  // (1 (2 (3 (4 (5)))))
  ui::AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  // Update the relative bounds of node 1, which is node 2's offset container.
  auto new_root_bounds = gfx::RectF(2, 3, 4, 5);
  update.nodes[0].relative_bounds.bounds = new_root_bounds;
  update.nodes[0].child_ids = {2};
  update.nodes[0].role = ax::mojom::Role::kStaticText;
  update.nodes[0].SetName(kUpdate2Name);
  update.nodes[1].id = 2;
  update.nodes[1].relative_bounds.bounds = gfx::RectF(20, 30, 40, 50);
  update.nodes[1].child_ids = {3};
  update.nodes[2].id = 3;
  update.nodes[2].relative_bounds.offset_container_id = 1u;
  update.nodes[2].role = ax::mojom::Role::kStaticText;
  update.nodes[2].SetName(kUpdateNodeName);

  bridge->AccessibilityEventReceived(
      CreateAccessibilityEventWithUpdate(std::move(update), tree_id));
  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kUpdateNodeName);

  // Verify that the transform for the Fuchsia semantic node corresponding to
  // node 3 reflects the new bounds of node 1.
  fuchsia::accessibility::semantics::Node* fuchsia_node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kUpdateNodeName);

  // A Fuchsia node's semantic transform should include an offset for its parent
  // node as a post-translation on top of its existing transform. Therefore, the
  // x, y, and z scale (indices 0, 5, and 10, respectively) should remain
  // unchanged, and the x and y bounds of the offset container should be added
  // to the node's existing translation entries (indices 12 and 13).
  EXPECT_EQ(fuchsia_node->transform().matrix[12], new_root_bounds.x());
  EXPECT_EQ(fuchsia_node->transform().matrix[13], new_root_bounds.y());
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, OutOfProcessIframe) {
  constexpr int64_t kBindingsId = 1234;

  // Start a different embedded test server, and load a page on it. The URL for
  // this page will have a different port and be considered out of process when
  // used as the src for an iframe.
  net::EmbeddedTestServer second_test_server;
  second_test_server.ServeFilesFromSourceDirectory(
      base::FilePath(kTestServerRoot));
  ASSERT_TRUE(second_test_server.Start());
  GURL out_of_process_url = second_test_server.GetURL(kPage1Path);

  // Before loading a page on the default embedded test server, set the iframe
  // src to be |out_of_process_url|.
  frame_->AddBeforeLoadJavaScript(
      kBindingsId, {"*"},
      base::MemBufferFromString(
          base::StringPrintf("iframeSrc = '%s'",
                             out_of_process_url.spec().c_str()),
          "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });
  LoadPage(kPageIframePath, "iframe loaded");

  // Run until the title of the iframe page is in the semantic tree. Because
  // the iframe's semantic tree is only sent when it is connected to the parent
  // tree, it is guaranteed that both trees will be present.
  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kPage1Title);

  // Two frames should be present.
  int num_frames = CollectAllRenderFrameHosts(
                       frame_impl_->web_contents_for_test()->GetPrimaryPage())
                       .size();

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

  frame_->ExecuteJavaScript(
      {"*"}, base::MemBufferFromString(script, "test2"),
      [](fuchsia::web::Frame_ExecuteJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(
      kPage2Title);

  // check that the iframe navigated to a different page.
  EXPECT_TRUE(semantics_manager_.semantic_tree()->GetNodeFromLabel(kNodeName));

  // Old iframe data should be gone.
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));

  // Makes the main page navigate to a different page, causing the iframe to go
  // away.
  LoadPage(kPage2Path, kPage2Title);

  // Wait for the root to be updated, which means that we navigated to a new
  // page.
  base::RunLoop run_loop;
  semantics_manager_.semantic_tree()->SetNodeUpdatedCallback(
      0u, run_loop.QuitClosure());
  run_loop.Run();

  // We've navigated to a different page that has no iframes. Only one frame
  // should be present.
  num_frames = CollectAllRenderFrameHosts(
                   frame_impl_->web_contents_for_test()->GetPrimaryPage())
                   .size();

  EXPECT_EQ(num_frames, 1);
}

IN_PROC_BROWSER_TEST_F(AccessibilityBridgeTest, UpdatesFocusInformation) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  ASSERT_FALSE(semantics_manager_.semantic_tree()
                   ->GetNodeWithId(0u)
                   ->states()
                   .has_has_input_focus());

  // Focus the root node.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  AccessibilityBridge* bridge = frame_impl_->accessibility_bridge_for_test();
  action_data.target_tree_id = bridge->ax_tree_for_test()->GetAXTreeID();
  action_data.target_node_id = bridge->ax_tree_for_test()->root()->id();

  frame_impl_->web_contents_for_test()
      ->GetPrimaryMainFrame()
      ->AccessibilityPerformAction(action_data);

  FakeSemanticTree* semantic_tree = semantics_manager_.semantic_tree();

  semantic_tree->RunUntilConditionIsTrue(
      base::BindLambdaForTesting([semantic_tree]() {
        auto* node = semantic_tree->GetNodeWithId(0u);
        if (!node)
          return false;

        return node->has_states() && node->states().has_has_input_focus() &&
               node->states().has_input_focus();
      }));

  ASSERT_TRUE(semantics_manager_.semantic_tree()
                  ->GetNodeWithId(0u)
                  ->states()
                  .has_input_focus());

  // Changes the focus to a different node and checks that the old value is
  // cleared.
  auto new_focus_id = semantics_manager_.semantic_tree()
                          ->GetNodeFromLabel(kButtonName1)
                          ->node_id();
  action_data.target_node_id =
      bridge->node_id_mapper_for_test()->ToAXNodeID(new_focus_id)->second;

  frame_impl_->web_contents_for_test()
      ->GetPrimaryMainFrame()
      ->AccessibilityPerformAction(action_data);

  semantic_tree->RunUntilConditionIsTrue(
      base::BindLambdaForTesting([semantic_tree, new_focus_id]() {
        auto* root = semantic_tree->GetNodeWithId(0u);
        auto* node = semantic_tree->GetNodeWithId(new_focus_id);

        if (!node || !root)
          return false;

        // Node has the focus, root does not.
        return (node->has_states() && node->states().has_has_input_focus() &&
                node->states().has_input_focus()) &&
               (root->has_states() && root->states().has_has_input_focus() &&
                !root->states().has_input_focus());
      }));

  ASSERT_FALSE(semantics_manager_.semantic_tree()
                   ->GetNodeWithId(0u)
                   ->states()
                   .has_input_focus());
  ASSERT_TRUE(semantics_manager_.semantic_tree()
                  ->GetNodeWithId(new_focus_id)
                  ->states()
                  .has_input_focus());
}

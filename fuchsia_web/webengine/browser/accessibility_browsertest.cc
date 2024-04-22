// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <zircon/types.h>

#include <string_view>

#include "base/command_line.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/fake_semantics_manager.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"
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

// Simulated screen bounds to use.
constexpr gfx::Size kTestWindowSize = {720, 640};

fuchsia::math::PointF GetCenterOfBox(fuchsia::ui::gfx::BoundingBox box) {
  fuchsia::math::PointF center;
  center.x = (box.min.x + box.max.x) / 2;
  center.y = (box.min.y + box.max.y) / 2;
  return center;
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

class FuchsiaFrameAccessibilityTest : public WebEngineBrowserTest {
 public:
  FuchsiaFrameAccessibilityTest() {
    WebEngineBrowserTest::set_test_server_root(base::FilePath(kTestServerRoot));
  }

  ~FuchsiaFrameAccessibilityTest() override = default;

  FuchsiaFrameAccessibilityTest(const FuchsiaFrameAccessibilityTest&) = delete;
  FuchsiaFrameAccessibilityTest& operator=(
      const FuchsiaFrameAccessibilityTest&) = delete;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchNative(switches::kOzonePlatform,
                                     switches::kHeadless);
    command_line->AppendSwitch(switches::kHeadless);
    WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    test_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    WebEngineBrowserTest::SetUpOnMainThread();

    // Remove the injected a11y manager from /svc; otherwise, we won't be able
    // to replace it with the fake owned by the test fixture.
    test_context_->additional_services()
        ->RemovePublicService<
            fuchsia::accessibility::semantics::SemanticsManager>();
    semantics_manager_binding_.emplace(test_context_->additional_services(),
                                       &semantics_manager_);

    frame_ = FrameForTest::Create(context(), {});
    base::RunLoop().RunUntilIdle();

    frame_impl_ = context_impl()->GetFrameImplForTest(&frame_.ptr());
    frame_impl_->set_window_size_for_test(kTestWindowSize);
    frame_->EnableHeadlessRendering();

    semantics_manager_.WaitUntilViewRegistered();
    ASSERT_TRUE(semantics_manager_.is_view_registered());
    ASSERT_TRUE(semantics_manager_.is_listener_valid());

    ASSERT_TRUE(embedded_test_server()->Start());

    // Change the accessibility mode on the Fuchsia side and check that it is
    // propagated correctly.
    ASSERT_FALSE(frame_impl_->web_contents_for_test()
                     ->IsFullAccessibilityModeForTesting());

    semantics_manager_.SetSemanticsModeEnabled(true);
    base::RunLoop().RunUntilIdle();

    ASSERT_TRUE(frame_impl_->web_contents_for_test()
                    ->IsFullAccessibilityModeForTesting());
  }

  void TearDownOnMainThread() override {
    frame_ = {};
    WebEngineBrowserTest::TearDownOnMainThread();
  }

  void LoadPage(std::string_view url, std::string_view page_title) {
    GURL page_url(embedded_test_server()->GetURL(std::string(url)));
    ASSERT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         page_url.spec()));
    frame_.navigation_listener().RunUntilUrlAndTitleEquals(page_url,
                                                           page_title);
  }

 protected:
  // TODO(crbug.com/42050058): Maybe move to WebEngineBrowserTest.
  std::optional<base::TestComponentContextForProcess> test_context_;

  FrameForTest frame_;
  FrameImpl* frame_impl_;
  FakeSemanticsManager semantics_manager_;

  // Binding to the fake semantics manager.
  // Optional so that it can be instantiated outside the constructor.
  std::optional<base::ScopedServiceBinding<
      fuchsia::accessibility::semantics::SemanticsManager>>
      semantics_manager_binding_;
};

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, CorrectDataSent) {
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
IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, DataSentWithBatching) {
  LoadPage(kPage2Path, kPage2Title);

  // Run until we expect more than a batch's worth of nodes to be present.
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage2NodeCount);
  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(kNodeName);

  // Checks if the actual batching happened.
  EXPECT_GE(semantics_manager_.semantic_tree()->num_update_calls(), 18u);

  // Checks if one or more commit calls were made to send the data.
  EXPECT_GE(semantics_manager_.semantic_tree()->num_commit_calls(), 1u);
}

// Check that semantics information is correctly sent when navigating from page
// to page.
IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, NavigateFromPageToPage) {
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

  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(kNodeName);

  // Check that data from the first page has been deleted successfully.
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1));
  EXPECT_FALSE(
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName));
}

// Checks that the correct node ID is returned when performing hit testing.
IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, HitTest) {
  LoadPage(kPage1Path, kPage1Title);
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* target_node =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kParagraphName);
  EXPECT_TRUE(target_node);

  fuchsia::math::PointF target_point = GetCenterOfBox(target_node->location());

  float scale_factor = 20.f;
  // Make the bridge use scaling in hit test calculations.
  frame_impl_->OnPixelScaleUpdate(scale_factor);

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

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, PerformDefaultAction) {
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

  EXPECT_TRUE(semantics_manager_.RequestAccessibilityActionSync(
      button1->node_id(), fuchsia::accessibility::semantics::Action::DEFAULT));
}

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest,
                       PerformUnsupportedAction) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* button1 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName1);
  EXPECT_TRUE(button1);
  fuchsia::accessibility::semantics::Node* button2 =
      semantics_manager_.semantic_tree()->GetNodeFromLabel(kButtonName2);
  EXPECT_TRUE(button2);

  // Attempt to perform unsupported action.
  EXPECT_FALSE(semantics_manager_.RequestAccessibilityActionSync(
      button2->node_id(),
      fuchsia::accessibility::semantics::Action::SECONDARY));
}

// This test times out frequently, presumably due to a race condition.
// TODO(crbug.com/40896150): Re-enable this test when it is no longer flaky.
IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, DISABLED_Disconnect) {
  base::RunLoop run_loop;
  frame_.ptr().set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(ZX_ERR_INTERNAL, status);
    run_loop.Quit();
  });

  semantics_manager_.semantic_tree()->Disconnect();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest,
                       PerformScrollToMakeVisible) {
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

  // Get a node that is off the screen, and verify that it is off the screen.
  fuchsia::accessibility::semantics::Node* fuchsia_node =
      semantic_tree->GetNodeFromLabel(kOffscreenNodeName);
  ASSERT_TRUE(fuchsia_node);

  // Get the corresponding AXPlatformNode.
  auto* fuchsia_platform_node = static_cast<ui::AXPlatformNodeFuchsia*>(
      ui::AXPlatformNodeBase::GetFromUniqueId(fuchsia_node->node_id()));
  ASSERT_TRUE(fuchsia_platform_node);
  auto* delegate = fuchsia_platform_node->GetDelegate();

  ui::AXOffscreenResult offscreen_result;
  delegate->GetClippedScreenBoundsRect(&offscreen_result);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOffscreen);

  // Perform SHOW_ON_SCREEN on that node.
  EXPECT_TRUE(semantics_manager_.RequestAccessibilityActionSync(
      fuchsia_node->node_id(),
      fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN));

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
  delegate->GetClippedScreenBoundsRect(&offscreen_result);
  EXPECT_EQ(offscreen_result, ui::AXOffscreenResult::kOnscreen);
}

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, Slider) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  fuchsia::accessibility::semantics::Node* node =
      semantics_manager_.semantic_tree()->GetNodeFromRole(
          fuchsia::accessibility::semantics::Role::SLIDER);
  EXPECT_TRUE(node);
  EXPECT_TRUE(node->has_states() && node->states().has_range_value());
  EXPECT_EQ(node->states().range_value(), kInitialRangeValue);

  base::RunLoop run_loop;
  semantics_manager_.semantic_tree()->SetNodeUpdatedCallback(
      node->node_id(), run_loop.QuitClosure());

  semantics_manager_.RequestAccessibilityActionSync(
      node->node_id(), fuchsia::accessibility::semantics::Action::INCREMENT);
  run_loop.Run();

  node = semantics_manager_.semantic_tree()->GetNodeWithId(node->node_id());
  EXPECT_TRUE(node->has_states() && node->states().has_range_value());
  EXPECT_EQ(node->states().range_value(), kInitialRangeValue + kStepSize);
}

// This test makes sure that when semantic updates toggle on / off / on, the
// full semantic tree is sent in the first update when back on.
IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, TogglesSemanticsUpdates) {
  LoadPage(kPage1Path, kPage1Title);

  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  semantics_manager_.SetSemanticsModeEnabled(false);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(frame_impl_->web_contents_for_test()
                   ->IsFullAccessibilityModeForTesting());

  // The tree gets cleared when semantic updates are off.
  EXPECT_EQ(semantics_manager_.semantic_tree()->tree_size(), 0u);
  semantics_manager_.SetSemanticsModeEnabled(true);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(frame_impl_->web_contents_for_test()
                  ->IsFullAccessibilityModeForTesting());
}

// This test performs several tree modifications (insertions, changes, and
// removals). All operations must leave the tree in a valid state and
// also forward the nodes in a way that leaves the tree in the Fuchsia side in a
// valid state. Note that every time that a new tree is sent to Fuchsia, the
// FakeSemantiTree checks if the tree is valid.
IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest,
                       TreeModificationsAreForwarded) {
  LoadPage(kPage1Path, kPage1Title);

  auto* semantic_tree = semantics_manager_.semantic_tree();
  semantics_manager_.semantic_tree()->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Create a new HTML element.
  {
    const auto script = base::StringPrintf(
        "var p = document.createElement(\"p\"); var text = "
        "document.createTextNode(\"new_label\"); p.appendChild(text); "
        "document.body.appendChild(p);");

    frame_->ExecuteJavaScript(
        {"*"}, base::MemBufferFromString(script, "add node"),
        [](fuchsia::web::Frame_ExecuteJavaScript_Result result) {
          EXPECT_TRUE(result.is_response());
        });

    semantic_tree->RunUntilNodeWithLabelIsInTree("new_label");
  }

  // Remove an HTML element.
  {
    // Verify that slider is present initially.
    EXPECT_TRUE(semantic_tree->GetNodeFromRole(
        fuchsia::accessibility::semantics::Role::SLIDER));

    const auto script = base::StringPrintf(
        "var slider = document.getElementById(\"myRange\"); slider.remove();");

    frame_->ExecuteJavaScript(
        {"*"}, base::MemBufferFromString(script, "reparent nodes"),
        [](fuchsia::web::Frame_ExecuteJavaScript_Result result) {
          EXPECT_TRUE(result.is_response());
        });

    semantic_tree->RunUntilConditionIsTrue(
        base::BindLambdaForTesting([semantic_tree]() {
          return !semantic_tree->GetNodeFromRole(
              fuchsia::accessibility::semantics::Role::SLIDER);
        }));
  }
}

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, OutOfProcessIframe) {
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
  semantics_manager_.semantic_tree()->RunUntilNodeWithLabelIsInTree(kNodeName);

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

IN_PROC_BROWSER_TEST_F(FuchsiaFrameAccessibilityTest, UpdatesFocusInformation) {
  LoadPage(kPage1Path, kPage1Title);

  auto* semantic_tree = semantics_manager_.semantic_tree();
  semantic_tree->RunUntilNodeCountAtLeast(kPage1NodeCount);

  // Get a node that is off the screen, and verify that it is off the screen.
  fuchsia::accessibility::semantics::Node* fuchsia_node =
      semantic_tree->GetNodeFromLabel(kButtonName1);
  ASSERT_TRUE(fuchsia_node);
  EXPECT_FALSE(fuchsia_node->states().has_input_focus());

  // Get the corresponding AXPlatformNode.
  auto* fuchsia_platform_node = static_cast<ui::AXPlatformNodeFuchsia*>(
      ui::AXPlatformNodeBase::GetFromUniqueId(fuchsia_node->node_id()));
  ASSERT_TRUE(fuchsia_platform_node);

  // Focus the node.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  fuchsia_platform_node->PerformAction(action_data);

  semantic_tree->RunUntilConditionIsTrue(
      base::BindLambdaForTesting([semantic_tree, fuchsia_node]() {
        auto* node = semantic_tree->GetNodeWithId(fuchsia_node->node_id());
        if (!node)
          return false;

        return node->has_states() && node->states().has_has_input_focus() &&
               node->states().has_input_focus();
      }));

  // Changes the focus to a different node and checks that the old value is
  // cleared.
  fuchsia::accessibility::semantics::Node* new_focus_node =
      semantic_tree->GetNodeFromLabel(kButtonName2);
  ASSERT_TRUE(new_focus_node);

  // Get the corresponding AXPlatformNode.
  auto* new_focus_platform_node = static_cast<ui::AXPlatformNodeFuchsia*>(
      ui::AXPlatformNodeBase::GetFromUniqueId(new_focus_node->node_id()));
  ASSERT_TRUE(new_focus_platform_node);

  // Focus the new node. We can reuse the original action data.
  new_focus_platform_node->PerformAction(action_data);

  semantic_tree->RunUntilConditionIsTrue(base::BindLambdaForTesting(
      [semantic_tree, new_focus_id = new_focus_node->node_id(),
       old_focus_id = fuchsia_node->node_id()]() {
        auto* old_focus = semantic_tree->GetNodeWithId(old_focus_id);
        auto* node = semantic_tree->GetNodeWithId(new_focus_id);

        if (!node || !old_focus)
          return false;

        // Node has the focus, root does not.
        return (node->has_states() && node->states().has_has_input_focus() &&
                node->states().has_input_focus()) &&
               (old_focus->has_states() &&
                old_focus->states().has_has_input_focus() &&
                !old_focus->states().has_input_focus());
      }));
}

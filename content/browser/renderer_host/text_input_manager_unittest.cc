// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_factory.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

class TextInputManagerTest : public RenderViewHostTestHarness {
 public:
  TextInputManagerTest() {
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }
  ~TextInputManagerTest() override = default;
};

#if BUILDFLAG(IS_WIN)
class TestTextInputManagerObserver : public TextInputManager::Observer {
 public:
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_update_state) override {
    last_did_update_state_ = did_update_state;
    ++update_call_count_;
  }

  bool last_did_update_state_ = false;
  int update_call_count_ = 0;
};

TEST_F(TextInputManagerTest, CustomPasswordFlagDoesNotRefocusNativePassword) {
  auto* view =
      static_cast<RenderWidgetHostViewBase*>(rvh()->GetWidget()->GetView());
  TextInputManager* manager = view->GetTextInputManager();
  ASSERT_TRUE(manager);

  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_PASSWORD;
  state.flags = ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD;
  state.node_id = 1;
  manager->UpdateTextInputState(view, state);

  TestTextInputManagerObserver observer;
  manager->AddObserver(&observer);

  // Adding only the redundant custom password flag is not a state update.
  state.flags |= ui::TEXT_INPUT_FLAG_HAS_BEEN_CUSTOM_PASSWORD;
  manager->UpdateTextInputState(view, state);
  EXPECT_EQ(observer.update_call_count_, 1);
  EXPECT_FALSE(observer.last_did_update_state_);
  EXPECT_EQ(manager->GetTextInputState()->flags, state.flags);

  // Removing only the redundant custom password flag is also not an update.
  state.flags = ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD;
  manager->UpdateTextInputState(view, state);
  EXPECT_EQ(observer.update_call_count_, 2);
  EXPECT_FALSE(observer.last_did_update_state_);

  // Changing another flag at the same time remains a state update.
  state.flags |= ui::TEXT_INPUT_FLAG_HAS_BEEN_CUSTOM_PASSWORD |
                 ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF;
  manager->UpdateTextInputState(view, state);
  EXPECT_EQ(observer.update_call_count_, 3);
  EXPECT_TRUE(observer.last_did_update_state_);

  // A different node represents a genuine focus change that must update TSF.
  state.node_id = 2;
  manager->UpdateTextInputState(view, state);
  EXPECT_EQ(observer.update_call_count_, 4);
  EXPECT_TRUE(observer.last_did_update_state_);

  manager->RemoveObserver(&observer);
}
#endif  // BUILDFLAG(IS_WIN)

// Test that ImeCompositionRangeChanged clamps out-of-bounds character bounds.
TEST_F(TextInputManagerTest, ImeCompositionRangeChanged_Clamped) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(rvh()->GetWidget()->GetView());

  TextInputManager* manager = view->GetTextInputManager();
  ASSERT_TRUE(manager);

  // Make the view active in TextInputManager.
  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;
  manager->UpdateTextInputState(view, state);

  view->SetBounds(gfx::Rect(0, 0, 800, 600));

  // Simulate an IPC to set character bounds that are out of bounds (negative
  // origin). Rect: x=-10, y=-10, w=50, h=50
  manager->ImeCompositionRangeChanged(view, gfx::Range(0, 1),
                                      {{gfx::Rect(-10, -10, 50, 50)}});

  const TextInputManager::CompositionRangeInfo* info =
      manager->GetCompositionRangeInfo();

  ASSERT_TRUE(info);
  ASSERT_EQ(info->character_bounds.size(), 1u);

  // Should be clamped to fit in (0, 0, 800, 600) -> (0, 0, 50, 50)
  gfx::Rect expected_bounds(0, 0, 50, 50);
  EXPECT_EQ(info->character_bounds[0], expected_bounds);
}

// Test that ImeCompositionRangeChanged does not clamp in-bounds character
// bounds.
TEST_F(TextInputManagerTest, ImeCompositionRangeChanged_InBounds) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(rvh()->GetWidget()->GetView());

  TextInputManager* manager = view->GetTextInputManager();
  ASSERT_TRUE(manager);

  // Make the view active in TextInputManager.
  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;
  manager->UpdateTextInputState(view, state);

  view->SetBounds(gfx::Rect(0, 0, 800, 600));

  // In bounds rect: x=10, y=10, w=50, h=50
  manager->ImeCompositionRangeChanged(view, gfx::Range(0, 1),
                                      {{gfx::Rect(10, 10, 50, 50)}});

  const TextInputManager::CompositionRangeInfo* info =
      manager->GetCompositionRangeInfo();

  ASSERT_TRUE(info);
  ASSERT_EQ(info->character_bounds.size(), 1u);

  gfx::Rect expected_bounds(10, 10, 50, 50);
  EXPECT_EQ(info->character_bounds[0], expected_bounds);
}

// Test that SelectionBoundsChanged clamps out-of-bounds selection bounds.
TEST_F(TextInputManagerTest, SelectionBoundsChanged_Clamped) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(rvh()->GetWidget()->GetView());

  TextInputManager* manager = view->GetTextInputManager();
  ASSERT_TRUE(manager);

  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;
  manager->UpdateTextInputState(view, state);

  view->SetBounds(gfx::Rect(0, 0, 800, 600));

  // Simulate SelectionBoundsChanged with out-of-bounds values.
  gfx::Rect out_of_bounds_rect(-10, -10, 10, 20);
  gfx::Rect out_of_bounds_box(-20, -20, 50, 50);

  manager->SelectionBoundsChanged(view, out_of_bounds_rect,
                                  base::i18n::LEFT_TO_RIGHT, out_of_bounds_rect,
                                  base::i18n::LEFT_TO_RIGHT, out_of_bounds_box,
                                  /*is_anchor_first=*/true);

  const TextInputManager::SelectionRegion* region =
      manager->GetSelectionRegion(view);

  ASSERT_TRUE(region);

  // Expected clamped bounds:
  EXPECT_EQ(region->anchor.edge_start(), gfx::PointF(0, 0));
  EXPECT_EQ(region->anchor.edge_end(), gfx::PointF(0, 20));
  EXPECT_EQ(region->focus.edge_start(), gfx::PointF(0, 0));
  EXPECT_EQ(region->focus.edge_end(), gfx::PointF(0, 20));

  EXPECT_EQ(region->caret_rect, gfx::Rect(0, 0, 10, 20));
  EXPECT_EQ(region->first_selection_rect, gfx::Rect(0, 0, 10, 20));
  EXPECT_EQ(region->bounding_box, out_of_bounds_box);
}

// Test that SelectionBoundsChanged clamps out-of-bounds selection bounds
// when anchor and focus are different.
TEST_F(TextInputManagerTest, SelectionBoundsChanged_Clamped_DifferentBounds) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(rvh()->GetWidget()->GetView());

  TextInputManager* manager = view->GetTextInputManager();
  ASSERT_TRUE(manager);

  ui::mojom::TextInputState state;
  state.type = ui::TEXT_INPUT_TYPE_TEXT;
  manager->UpdateTextInputState(view, state);

  view->SetBounds(gfx::Rect(0, 0, 800, 600));

  // Anchor is out-of-bounds top-left: (-20, -10, 10, 20) -> clamped to (0, 0,
  // 10, 20) Focus is out-of-bounds bottom-right: (810, 590, 10, 20) -> clamped
  // to (790, 580, 10, 20)
  gfx::Rect anchor_rect(-20, -10, 10, 20);
  gfx::Rect focus_rect(810, 590, 10, 20);
  gfx::Rect bounding_box(-20, -10, 840, 620);

  manager->SelectionBoundsChanged(view, anchor_rect, base::i18n::LEFT_TO_RIGHT,
                                  focus_rect, base::i18n::LEFT_TO_RIGHT,
                                  bounding_box,
                                  /*is_anchor_first=*/true);

  const TextInputManager::SelectionRegion* region =
      manager->GetSelectionRegion(view);

  ASSERT_TRUE(region);

  // Expected clamped bounds:
  EXPECT_EQ(region->anchor.edge_start(), gfx::PointF(0, 0));
  EXPECT_EQ(region->anchor.edge_end(), gfx::PointF(0, 20));

  EXPECT_EQ(region->focus.edge_start(), gfx::PointF(790, 580));
  EXPECT_EQ(region->focus.edge_end(), gfx::PointF(790, 600));

  EXPECT_EQ(region->first_selection_rect, gfx::Rect(0, 0, 10, 20));
  EXPECT_EQ(region->bounding_box, bounding_box);
}

// Test that an unfocused child frame cannot become the active view.
TEST_F(TextInputManagerTest, UnfocusedChildFrameCannotBecomeActive) {
  GURL parent_url("https://a.test/");
  NavigationSimulator::CreateRendererInitiated(parent_url, main_rfh())
      ->Commit();

  auto* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  GURL child_url("https://b.test/");
  child_rfh =
      NavigationSimulator::NavigateAndCommitFromDocument(child_url, child_rfh);

  auto* main_view = static_cast<RenderWidgetHostViewBase*>(
      main_rfh()->GetRenderWidgetHost()->GetView());
  auto* child_view = static_cast<RenderWidgetHostViewBase*>(
      child_rfh->GetRenderWidgetHost()->GetView());

  TextInputManager* manager = main_view->GetTextInputManager();
  EXPECT_TRUE(manager);

  // Main frame is focused. Activate text input on main frame.
  ui::mojom::TextInputState main_state;
  main_state.type = ui::TEXT_INPUT_TYPE_PASSWORD;
  manager->UpdateTextInputState(main_view, main_state);

  EXPECT_EQ(main_view, manager->active_view_for_testing());
  EXPECT_EQ(main_rfh()->GetRenderWidgetHost(), manager->GetActiveWidget());

  // Attempt to activate text input from unfocused child frame.
  ui::mojom::TextInputState child_state;
  child_state.type = ui::TEXT_INPUT_TYPE_TEXT;
  manager->UpdateTextInputState(child_view, child_state);

  // Active view and widget must remain unchanged (main frame).
  EXPECT_EQ(main_view, manager->active_view_for_testing());
  EXPECT_EQ(main_rfh()->GetRenderWidgetHost(), manager->GetActiveWidget());
}

// Test that a popup widget's ability to become active depends on whether
// its creator frame is focused.
TEST_F(TextInputManagerTest, PopupActiveStateDependsOnCreatorFrameFocus) {
  GURL parent_url("https://a.test/");
  NavigationSimulator::CreateRendererInitiated(parent_url, main_rfh())
      ->Commit();

  auto* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");
  GURL child_url("https://b.test/");
  child_rfh =
      NavigationSimulator::NavigateAndCommitFromDocument(child_url, child_rfh);

  auto* main_view = static_cast<RenderWidgetHostViewBase*>(
      main_rfh()->GetRenderWidgetHost()->GetView());
  TextInputManager* manager = main_view->GetTextInputManager();
  EXPECT_TRUE(manager);

  RenderFrameHostImpl* main_rfh_impl =
      static_cast<RenderFrameHostImpl*>(main_rfh());
  RenderFrameHostImpl* child_rfh_impl =
      static_cast<RenderFrameHostImpl*>(child_rfh);

  // 1. Create a popup widget view and associate its creator frame with the main
  // frame (which currently has focus).
  auto popup_widget = RenderWidgetHostFactory::Create(
      /*frame_tree=*/nullptr, main_rfh_impl->GetRenderWidgetHost()->delegate(),
      viz::FrameSinkId(1, 1),
      main_rfh_impl->GetSiteInstance()->group()->GetSafeRef(),
      main_rfh()->GetProcess()->GetNextRoutingID(),
      /*hidden=*/false,
      /*renderer_initiated_creation=*/false);
  auto popup_view =
      std::make_unique<TestRenderWidgetHostView>(popup_widget.get());
  popup_view->SetWidgetType(WidgetType::kPopup);
  EXPECT_EQ(manager, popup_view->GetTextInputManager());
  popup_widget->set_popup_creator_frame_id_for_testing(
      main_rfh()->GetGlobalId());

  // Since the creator frame (main frame) has focus, popup can become active.
  ui::mojom::TextInputState popup_state;
  popup_state.type = ui::TEXT_INPUT_TYPE_TEXT;
  manager->UpdateTextInputState(popup_view.get(), popup_state);

  EXPECT_EQ(popup_view.get(), manager->active_view_for_testing());
  EXPECT_EQ(popup_widget.get(), manager->GetActiveWidget());

  // 2. Now create a popup whose creator frame is the unfocused child frame.
  auto unfocused_popup_widget = RenderWidgetHostFactory::Create(
      /*frame_tree=*/nullptr, child_rfh_impl->GetRenderWidgetHost()->delegate(),
      viz::FrameSinkId(2, 2),
      child_rfh_impl->GetSiteInstance()->group()->GetSafeRef(),
      child_rfh->GetProcess()->GetNextRoutingID(),
      /*hidden=*/false,
      /*renderer_initiated_creation=*/false);
  auto unfocused_popup_view =
      std::make_unique<TestRenderWidgetHostView>(unfocused_popup_widget.get());
  unfocused_popup_view->SetWidgetType(WidgetType::kPopup);
  EXPECT_EQ(manager, unfocused_popup_view->GetTextInputManager());
  unfocused_popup_widget->set_popup_creator_frame_id_for_testing(
      child_rfh->GetGlobalId());

  // Attempt to activate text input from popup of unfocused frame.
  manager->UpdateTextInputState(unfocused_popup_view.get(), popup_state);

  // Active view and widget must remain the focused popup, not the unfocused
  // popup.
  EXPECT_EQ(popup_view.get(), manager->active_view_for_testing());
  EXPECT_EQ(popup_widget.get(), manager->GetActiveWidget());

  // 3. Also verify a popup with no creator frame cannot become active.
  auto orphan_popup_widget = RenderWidgetHostFactory::Create(
      /*frame_tree=*/nullptr, main_rfh_impl->GetRenderWidgetHost()->delegate(),
      viz::FrameSinkId(3, 3),
      main_rfh_impl->GetSiteInstance()->group()->GetSafeRef(),
      main_rfh()->GetProcess()->GetNextRoutingID(),
      /*hidden=*/false,
      /*renderer_initiated_creation=*/false);
  auto orphan_popup_view =
      std::make_unique<TestRenderWidgetHostView>(orphan_popup_widget.get());
  orphan_popup_view->SetWidgetType(WidgetType::kPopup);
  EXPECT_EQ(manager, orphan_popup_view->GetTextInputManager());

  manager->UpdateTextInputState(orphan_popup_view.get(), popup_state);
  EXPECT_EQ(popup_view.get(), manager->active_view_for_testing());
  EXPECT_EQ(popup_widget.get(), manager->GetActiveWidget());
}

}  // namespace content

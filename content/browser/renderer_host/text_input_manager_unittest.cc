// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
  TextInputManagerTest() = default;
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

// Runs |callback| the first time an observer is told about an update for
// |target_view|, simulating a nested message loop (e.g. a third-party IME)
// running arbitrary tasks from inside the notification.
class ReentrantUpdateObserver : public TextInputManager::Observer {
 public:
  ReentrantUpdateObserver(RenderWidgetHostViewBase* target_view,
                          base::OnceClosure callback)
      : target_view_(target_view), callback_(std::move(callback)) {}

  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_update_state) override {
    if (updated_view != target_view_ || !callback_) {
      return;
    }
    text_input_manager->RemoveObserver(this);
    std::move(callback_).Run();
  }

 private:
  raw_ptr<RenderWidgetHostViewBase, DisableDanglingPtrDetection> target_view_;
  base::OnceClosure callback_;
};

class TextInputManagerReentrancyTest : public TextInputManagerTest {
 public:
  TextInputManagerReentrancyTest() {
    // Needed so the cross-site child frame gets its own RenderWidgetHostView.
    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  }

 protected:
  // Commits a cross-site child frame, and makes the main frame the active
  // text input view.
  void SetUpMainAndChild() {
    NavigationSimulator::CreateRendererInitiated(GURL("https://a.test/"),
                                                 main_rfh())
        ->Commit();
    child_rfh_ = static_cast<RenderFrameHostImpl*>(
        NavigationSimulator::NavigateAndCommitFromDocument(
            GURL("https://b.test/"),
            RenderFrameHostTester::For(main_rfh())->AppendChild("child")));
    main_view_ = static_cast<RenderWidgetHostViewBase*>(
        main_rfh()->GetRenderWidgetHost()->GetView());
    child_view_ = static_cast<RenderWidgetHostViewBase*>(
        child_rfh_->GetRenderWidgetHost()->GetView());
    manager_ = main_view_->GetTextInputManager();
    ASSERT_TRUE(manager_);
    ASSERT_EQ(manager_, child_view_->GetTextInputManager());

    ui::mojom::TextInputState state;
    state.type = ui::TEXT_INPUT_TYPE_TEXT;
    manager_->UpdateTextInputState(main_view_, state);
    ASSERT_EQ(main_view_, manager_->active_view_for_testing());

    // Move focus to the child frame, so that its text input update replaces
    // the main frame as the active view.
    child_rfh_->frame_tree_node()->frame_tree().SetFocusedFrame(
        child_rfh_->frame_tree_node(), nullptr);
  }

  void UpdateChildTextInputState() {
    ui::mojom::TextInputState state;
    state.type = ui::TEXT_INPUT_TYPE_TEXT;
    manager_->UpdateTextInputState(child_view_, state);
  }

  raw_ptr<RenderFrameHostImpl, DisableDanglingPtrDetection> child_rfh_ =
      nullptr;
  raw_ptr<RenderWidgetHostViewBase, DisableDanglingPtrDetection> main_view_ =
      nullptr;
  raw_ptr<RenderWidgetHostViewBase, DisableDanglingPtrDetection> child_view_ =
      nullptr;
  raw_ptr<TextInputManager, DisableDanglingPtrDetection> manager_ = nullptr;
};

// Regression test for crbug.com/568937999. When a newly focused view replaces
// the active view, observers are notified that the previously active view lost
// its text input state. An observer that runs a nested message loop can close
// the tab, destroying the WebContents and its TextInputManager.
// UpdateTextInputState() must not touch the destroyed TextInputManager after
// that notification returns.
TEST_F(TextInputManagerReentrancyTest,
       WebContentsDestroyedDuringActiveViewReset) {
  ASSERT_NO_FATAL_FAILURE(SetUpMainAndChild());

  bool contents_deleted = false;
  ReentrantUpdateObserver observer(main_view_,
                                   base::BindLambdaForTesting([&]() {
                                     DeleteContents();
                                     contents_deleted = true;
                                   }));
  manager_->AddObserver(&observer);

  UpdateChildTextInputState();
  EXPECT_TRUE(contents_deleted);
}

// Same as above, but the nested message loop only unregisters the newly
// focused child view without destroying it (e.g. the view moving to another
// TextInputManager). The TextInputManager survives, and the still-alive,
// focused view must not become the active view of a TextInputManager it is no
// longer registered with.
TEST_F(TextInputManagerReentrancyTest, ViewUnregisteredDuringActiveViewReset) {
  ASSERT_NO_FATAL_FAILURE(SetUpMainAndChild());

  bool child_unregistered = false;
  ReentrantUpdateObserver observer(main_view_,
                                   base::BindLambdaForTesting([&]() {
                                     manager_->Unregister(child_view_);
                                     child_unregistered = true;
                                   }));
  manager_->AddObserver(&observer);

  UpdateChildTextInputState();
  EXPECT_TRUE(child_unregistered);
  EXPECT_FALSE(manager_->IsRegistered(child_view_));
  EXPECT_EQ(nullptr, manager_->active_view_for_testing());
}

}  // namespace content

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

class TextInputManagerTest : public RenderViewHostTestHarness {
 public:
  TextInputManagerTest() = default;
  ~TextInputManagerTest() override = default;
};

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

}  // namespace content

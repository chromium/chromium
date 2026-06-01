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

}  // namespace content

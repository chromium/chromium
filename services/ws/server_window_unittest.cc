// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/server_window.h"

#include <memory>

#include "base/run_loop.h"
#include "services/ws/client_root_test_helper.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

namespace ws {

TEST(ServerWindow, FindTargetForWindowWithEasyResizeTargeter) {
  WindowServiceTestSetup setup;
  std::unique_ptr<wm::EasyResizeWindowTargeter> easy_resize_window_targeter =
      std::make_unique<wm::EasyResizeWindowTargeter>(
          gfx::Insets(-10, -10, -10, -10), gfx::Insets(-10, -10, -10, -10));
  setup.root()->SetEventTargeter(std::move(easy_resize_window_targeter));
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->SetBounds(gfx::Rect(100, 200, 200, 200));
  top_level->Show();
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(105, 195),
                             gfx::Point(105, 195), base::TimeTicks::Now(),
                             /* flags */ 0,
                             /* changed_button_flags_ */ 0);

  // Even though the mouse is not over |top_level| it should be returned as the
  // target because EasyResizeWindowTargeter enlarges the hit area.
  EXPECT_EQ(top_level, setup.root()->targeter()->FindTargetForEvent(
                           setup.root(), &mouse_event));

  // Repeat with a location outside the extended hit region and ensure
  // |top_level| is not returned.
  ui::MouseEvent mouse_event2(ui::ET_MOUSE_PRESSED, gfx::Point(5, 5),
                              gfx::Point(5, 5), base::TimeTicks::Now(),
                              /* flags */ 0,
                              /* changed_button_flags_ */ 0);
  EXPECT_NE(top_level, setup.root()->targeter()->FindTargetForEvent(
                           setup.root(), &mouse_event2));
}

TEST(ServerWindow, FindTargetForWindowWithResizeInset) {
  WindowServiceTestSetup setup;

  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  const gfx::Rect top_level_bounds(100, 200, 200, 200);
  top_level->SetBounds(top_level_bounds);
  top_level->Show();

  aura::Window* child_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(child_window);
  top_level->AddChild(child_window);
  child_window->SetBounds(gfx::Rect(top_level_bounds.size()));
  child_window->Show();

  const int kInset = 4;
  // Target an event at the resize inset area.
  gfx::Point click_point =
      top_level_bounds.left_center() + gfx::Vector2d(kInset / 2, 0);
  // With no resize inset set yet, the event should go to the child window.
  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, click_point, click_point,
                             base::TimeTicks::Now(),
                             /* flags */ 0,
                             /* changed_button_flags_ */ 0);
  EXPECT_EQ(child_window, setup.root()->targeter()->FindTargetForEvent(
                              setup.root(), &mouse_event));

  // With the resize inset, the event should go to the toplevel.
  top_level->SetProperty(aura::client::kResizeHandleInset, kInset);
  ui::MouseEvent mouse_event_2(ui::ET_MOUSE_PRESSED, click_point, click_point,
                               base::TimeTicks::Now(),
                               /* flags */ 0,
                               /* changed_button_flags_ */ 0);
  EXPECT_EQ(top_level, setup.root()->targeter()->FindTargetForEvent(
                           setup.root(), &mouse_event_2));
}

TEST(ServerWindow, SetClientAreaPropagatesToClientSurfaceEmbedder) {
  WindowServiceTestSetup setup;

  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  const gfx::Rect top_level_bounds(100, 200, 200, 200);
  top_level->SetBounds(top_level_bounds);
  const gfx::Insets top_level_insets(1, 2, 11, 12);
  setup.window_tree_test_helper()->SetClientArea(top_level, top_level_insets);
  aura::ClientSurfaceEmbedder* client_surface_embedder =
      ClientRootTestHelper(
          setup.window_tree()->GetClientRootForWindow(top_level))
          .GetClientSurfaceEmbedder();
  ASSERT_TRUE(client_surface_embedder);
  EXPECT_EQ(top_level_insets, client_surface_embedder->client_area_insets());
}

}  // namespace ws

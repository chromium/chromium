// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/focus_handler.h"

#include <stdint.h>

#include <memory>
#include <queue>

#include "services/ws/event_test_utils.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/focus_controller.h"

namespace ws {
namespace {

TEST(FocusHandlerTest, FocusTopLevel) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  // SetFocus() should fail as |top_level| isn't visible.
  EXPECT_FALSE(setup.window_tree_test_helper()->SetFocus(top_level));

  top_level->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetFocus(top_level));
  EXPECT_TRUE(top_level->HasFocus());
}

// This test simulates the typical sequence of a client closing a window Hide().
// Note that SetFocus(nullptr) shouldn't happen.
TEST(FocusHandlerTest, FocusChangesAfterHide) {
  // Create two top-levels and focus the second.
  WindowServiceTestSetup setup;
  aura::Window* top_level1 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level1);
  top_level1->Show();
  aura::Window* top_level2 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level2);
  top_level2->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetFocus(top_level2));
  EXPECT_TRUE(top_level2->HasFocus());

  // Hiding |top_level2| should focus |top_level1|.
  setup.changes()->clear();
  top_level2->Hide();
  EXPECT_FALSE(top_level2->HasFocus());
  EXPECT_TRUE(top_level1->HasFocus());
  EXPECT_TRUE(ContainsChange(*setup.changes(), "Focused id=0,1"));
}

TEST(FocusHandlerTest, FocusChild) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);

  // SetFocus() should fail as |window| isn't parented yet.
  EXPECT_FALSE(setup.window_tree_test_helper()->SetFocus(window));

  top_level->AddChild(window);
  // SetFocus() should still fail as |window| isn't visible.
  EXPECT_FALSE(setup.window_tree_test_helper()->SetFocus(window));
  setup.window_tree_test_helper()->SetCanFocus(window, false);
  window->Show();

  // SetFocus() should fail as SetCanFocus(false) was called.
  EXPECT_FALSE(setup.window_tree_test_helper()->SetFocus(window));

  setup.window_tree_test_helper()->SetCanFocus(window, true);
  EXPECT_TRUE(setup.window_tree_test_helper()->SetFocus(window));
}

// Regression test for https://crbug.com/880533
TEST(FocusHandlerTest, FocusChildOfActiveWindow) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->Show();
  setup.focus_controller()->ActivateWindow(top_level);
  EXPECT_EQ(top_level, setup.focus_controller()->GetActiveWindow());

  aura::Window* child = setup.window_tree_test_helper()->NewWindow();
  top_level->AddChild(child);
  child->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetFocus(child));
  EXPECT_TRUE(child->HasFocus());
}

TEST(FocusHandlerTest, NotifyOnFocusChange) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  aura::Window* window1 = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window1);
  top_level->AddChild(window1);
  window1->Show();
  aura::Window* window2 = setup.window_tree_test_helper()->NewWindow(4);
  ASSERT_TRUE(window2);
  top_level->AddChild(window2);
  window2->Show();
  setup.changes()->clear();

  // Window is parented and visible, so SetFocus() should succeed.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetFocus(window1));
  // As the client originated the request it is not notified of the change.
  EXPECT_TRUE(setup.changes()->empty());

  // Focus |window2| locally (not from the client), which should result in
  // notifying the client.
  window2->Focus();
  EXPECT_TRUE(window2->HasFocus());
  EXPECT_EQ("Focused id=0,4", SingleChangeToDescription(*setup.changes()));
}

TEST(FocusHandlerTest, FocusChangeFromEmbedded) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(embed_window);
  top_level->AddChild(embed_window);
  embed_window->Show();
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(embed_window);
  setup.changes()->clear();
  embedding_helper->changes()->clear();

  // Set focus from the embedded client.
  EXPECT_TRUE(
      embedding_helper->window_tree_test_helper->SetFocus(embed_window));
  EXPECT_TRUE(embed_window->HasFocus());
  EXPECT_TRUE(setup.changes()->empty());
  EXPECT_TRUE(embedding_helper->changes()->empty());

  // Send an event, the embedded client should get it.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(setup.changes()->empty());
  EXPECT_EQ(
      "KEY_PRESSED",
      EventToEventType(
          embedding_helper->window_tree_client.PopInputEvent().event.get()));
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());
  embedding_helper->changes()->clear();

  // Set focus from the parent. The embedded client should lose focus.
  setup.window_tree_test_helper()->SetFocus(embed_window);
  EXPECT_TRUE(embed_window->HasFocus());
  EXPECT_TRUE(setup.changes()->empty());
  EXPECT_EQ("Focused id=null",
            SingleChangeToDescription(*embedding_helper->changes()));
  embedding_helper->changes()->clear();

  // And events should now target the parent.
  event_generator.PressKey(ui::VKEY_B, ui::EF_NONE);
  EXPECT_EQ("KEY_PRESSED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(embedding_helper->changes()->empty());
}

TEST(FocusHandlerTest, EmbedderGetsInterceptedKeyEvents) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(embed_window);
  top_level->AddChild(embed_window);
  embed_window->Show();

  std::unique_ptr<EmbeddingHelper> embedding_helper = setup.CreateEmbedding(
      embed_window, mojom::kEmbedFlagEmbedderInterceptsEvents);
  ASSERT_TRUE(embedding_helper);
  aura::Window* embed_child_window =
      embedding_helper->window_tree_test_helper->NewWindow();
  ASSERT_TRUE(embed_child_window);
  embed_child_window->Show();
  embed_window->AddChild(embed_child_window);
  setup.changes()->clear();
  embedding_helper->changes()->clear();

  // Set focus from the embedded client.
  EXPECT_TRUE(
      embedding_helper->window_tree_test_helper->SetFocus(embed_child_window));
  EXPECT_TRUE(embed_child_window->HasFocus());

  // Generate a key-press. Even though focus is on a window in the embedded
  // client, the event goes to the embedder because it intercepts events.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(embedding_helper->changes()->empty());
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());
  EXPECT_EQ("KEY_PRESSED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
}

}  // namespace
}  // namespace ws

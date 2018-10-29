// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_queue.h"

#include <string>

#include "base/test/bind_test_util.h"
#include "services/ws/event_queue.h"
#include "services/ws/event_queue_test_helper.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

#include "services/ws/window_tree_test_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"

namespace ws {
namespace {

TEST(EventQueueTest, DontQueueEventsToLocalWindow) {
  WindowServiceTestSetup setup;
  setup.set_ack_events_immediately(false);

  aura::Window* window = new aura::Window(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  setup.root()->AddChild(window);
  window->Show();
  window->Focus();
  EXPECT_TRUE(window->HasFocus());

  setup.changes()->clear();
  EventQueueTestHelper event_queue_test_helper(setup.service()->event_queue());
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(event_queue_test_helper.HasInFlightEvent());
}

TEST(EventQueueTest, Basic) {
  WindowServiceTestSetup setup;
  setup.set_ack_events_immediately(false);

  // Events are only queued if they target a window with a remote client.
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->Focus();
  EXPECT_TRUE(top_level->HasFocus());

  // Generate a single key event.
  setup.changes()->clear();
  EventQueueTestHelper event_queue_test_helper(setup.service()->event_queue());
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(event_queue_test_helper.HasInFlightEvent());
  ASSERT_EQ(1u, setup.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_INPUT_EVENT, (*setup.changes())[0].type);
  setup.changes()->clear();

  // Generator another key event. As still waiting for a response from the
  // client this event should be queued.
  event_generator.PressKey(ui::VKEY_B, ui::EF_NONE);
  EXPECT_TRUE(event_queue_test_helper.HasInFlightEvent());
  EXPECT_TRUE(setup.changes()->empty());

  // Ack the first event, which should then send the second.
  event_queue_test_helper.AckInFlightEvent();
  EXPECT_TRUE(event_queue_test_helper.HasInFlightEvent());
  ASSERT_EQ(1u, setup.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_INPUT_EVENT, (*setup.changes())[0].type);
  setup.changes()->clear();

  // Acking the second should mean no more in flight events.
  event_queue_test_helper.AckInFlightEvent();
  EXPECT_FALSE(event_queue_test_helper.HasInFlightEvent());
  EXPECT_TRUE(setup.changes()->empty());
}

TEST(EventQueueTest, NotifyWhenReadyToDispatch) {
  WindowServiceTestSetup setup;
  setup.set_ack_events_immediately(false);

  bool was_dispatch_closure_run = false;
  setup.service()->event_queue()->NotifyWhenReadyToDispatch(
      base::BindLambdaForTesting([&]() { was_dispatch_closure_run = true; }));
  EXPECT_TRUE(was_dispatch_closure_run);
  was_dispatch_closure_run = false;

  // Events are only queued if they target a window with a remote client.
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->Focus();
  EXPECT_TRUE(top_level->HasFocus());

  // Generate a single key event.
  EventQueueTestHelper event_queue_test_helper(setup.service()->event_queue());
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(event_queue_test_helper.HasInFlightEvent());

  // Set another closure. The closure should not be run immediately.
  setup.service()->event_queue()->NotifyWhenReadyToDispatch(
      base::BindLambdaForTesting([&]() { was_dispatch_closure_run = true; }));
  EXPECT_FALSE(was_dispatch_closure_run);

  // Ack the event, which should notify the closure.
  event_queue_test_helper.AckInFlightEvent();
  EXPECT_TRUE(was_dispatch_closure_run);
}

}  // namespace
}  // namespace ws

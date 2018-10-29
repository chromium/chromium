// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_tree.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>

#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/test/fake_host_frame_sink_client.h"
#include "services/ws/event_test_utils.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/server_window.h"
#include "services/ws/server_window_test_helper.h"
#include "services/ws/window_delegate_impl.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/window_occlusion_tracker_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/mojo/event_constants.mojom.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/window_util.h"

namespace ws {
namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(aura::Window*, kTestPropertyKey, nullptr);
const char kTestPropertyServerKey[] = "test-property-server";

// Passed to Embed() to give the default behavior (see kEmbedFlag* in mojom for
// details).
constexpr uint32_t kDefaultEmbedFlags = 0;

class TestLayoutManager : public aura::LayoutManager {
 public:
  TestLayoutManager() = default;
  ~TestLayoutManager() override = default;

  void set_next_bounds(const gfx::Rect& bounds) { next_bounds_ = bounds; }

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    if (next_bounds_) {
      SetChildBoundsDirect(child, *next_bounds_);
      next_bounds_.reset();
    } else {
      SetChildBoundsDirect(child, requested_bounds);
    }
  }

 private:
  base::Optional<gfx::Rect> next_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Used as callback from ScheduleEmbed().
void ScheduleEmbedCallback(base::UnguessableToken* result_token,
                           const base::UnguessableToken& actual_token) {
  *result_token = actual_token;
}

// Used as callback to EmbedUsingToken().
void EmbedUsingTokenCallback(bool* was_called,
                             bool* result_value,
                             bool actual_result) {
  *was_called = true;
  *result_value = actual_result;
}

// A screen position client with a fixed screen offset applied via SetBounds.
class TestScreenPositionClient : public wm::DefaultScreenPositionClient {
 public:
  explicit TestScreenPositionClient(const gfx::Vector2d& offset)
      : offset_(offset) {}
  ~TestScreenPositionClient() override = default;

  // wm::DefaultScreenPositionClient:
  void ConvertPointToScreen(const aura::Window* window,
                            gfx::PointF* point) override {
    wm::DefaultScreenPositionClient::ConvertPointToScreen(window, point);
    *point += offset_;
  }
  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::PointF* point) override {
    *point -= offset_;
    wm::DefaultScreenPositionClient::ConvertPointFromScreen(window, point);
  }
  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const display::Display& display) override {
    EXPECT_EQ(display, display::Screen::GetScreen()->GetPrimaryDisplay());
    gfx::Rect offset_bounds = bounds;
    offset_bounds.Offset(-offset_);
    wm::DefaultScreenPositionClient::SetBounds(window, offset_bounds, display);
  }

 private:
  const gfx::Vector2d offset_;
  DISALLOW_COPY_AND_ASSIGN(TestScreenPositionClient);
};

TEST(WindowTreeTest, NewWindow) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, NewWindowWithProperties) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* window = setup.window_tree_test_helper()->NewWindow(
      1, {{mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(window);
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(window->GetProperty(aura::client::kAlwaysOnTopKey));
}

TEST(WindowTreeTest, NewTopLevelWindow) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, NewTopLevelWindowWithProperties) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* top_level = setup.window_tree_test_helper()->NewTopLevelWindow(
      1, {{mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
}

TEST(WindowTreeTest, SetTopLevelWindowBounds) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();

  const gfx::Rect bounds_from_client = gfx::Rect(100, 200, 300, 400);
  setup.window_tree_test_helper()->SetWindowBoundsWithAck(
      top_level, bounds_from_client, 2);
  EXPECT_EQ(bounds_from_client, top_level->GetBoundsInScreen());
  ASSERT_EQ(2u, setup.changes()->size());
  {
    const Change& change = (*setup.changes())[0];
    EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, change.type);
    EXPECT_EQ(top_level->GetBoundsInScreen(), change.bounds2);
    EXPECT_TRUE(change.local_surface_id);
    setup.changes()->erase(setup.changes()->begin());
  }
  // See comments in WindowTree::SetBoundsImpl() for why this returns false.
  EXPECT_EQ("ChangeCompleted id=2 success=false",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  const gfx::Rect bounds_from_server = gfx::Rect(101, 102, 103, 104);
  top_level->SetBounds(bounds_from_server);
  ASSERT_EQ(1u, setup.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*setup.changes())[0].type);
  EXPECT_EQ(bounds_from_server, (*setup.changes())[0].bounds2);
  setup.changes()->clear();

  // Set a LayoutManager so that when the client requests a bounds change the
  // window is resized to a different bounds.
  // |layout_manager| is owned by top_level->parent();
  TestLayoutManager* layout_manager = new TestLayoutManager();
  const gfx::Rect restricted_bounds = gfx::Rect(401, 405, 406, 407);
  layout_manager->set_next_bounds(restricted_bounds);
  top_level->parent()->SetLayoutManager(layout_manager);
  setup.window_tree_test_helper()->SetWindowBoundsWithAck(
      top_level, bounds_from_client, 3);
  ASSERT_EQ(2u, setup.changes()->size());
  // The layout manager changes the bounds to a different value than the client
  // requested, so the client should get OnWindowBoundsChanged() with
  // |restricted_bounds|.
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*setup.changes())[0].type);
  EXPECT_EQ(restricted_bounds, (*setup.changes())[0].bounds2);

  // And because the layout manager changed the bounds the result is false.
  EXPECT_EQ("ChangeCompleted id=3 success=false",
            ChangeToDescription((*setup.changes())[1]));
  setup.changes()->clear();

  // Install a screen position client with a non-zero screen bounds offset.
  gfx::Vector2d screen_offset(10, 20);
  TestScreenPositionClient screen_position_client(screen_offset);
  aura::client::SetScreenPositionClient(setup.aura_test_helper()->root_window(),
                                        &screen_position_client);

  // Tests that top-level window bounds are set in screen coordinates.
  setup.window_tree_test_helper()->SetWindowBoundsWithAck(
      top_level, bounds_from_client, 4);
  EXPECT_EQ(bounds_from_client, top_level->GetBoundsInScreen());
  EXPECT_EQ(bounds_from_client - screen_offset, top_level->bounds());
  ASSERT_EQ(2u, setup.changes()->size());
  {
    const Change& change = (*setup.changes())[0];
    EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, change.type);
    EXPECT_EQ(top_level->GetBoundsInScreen(), change.bounds2);
    EXPECT_TRUE(change.local_surface_id);
    setup.changes()->erase(setup.changes()->begin());
  }
  // See comments in WindowTree::SetBoundsImpl() for why this returns false.
  EXPECT_EQ("ChangeCompleted id=4 success=false",
            SingleChangeToDescription(*setup.changes()));

  aura::client::SetScreenPositionClient(setup.aura_test_helper()->root_window(),
                                        nullptr);
}

TEST(WindowTreeTest, SetTopLevelWindowBoundsFailsForSameSize) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();
  const gfx::Rect bounds = gfx::Rect(1, 2, 300, 400);
  top_level->SetBounds(bounds);
  setup.changes()->clear();
  // WindowTreeTestHelper::SetWindowBounds() uses a null LocalSurfaceId, which
  // differs from the current LocalSurfaceId (assigned by ClientRoot). Because
  // of this, the LocalSurfaceIds differ and the call returns false.
  EXPECT_FALSE(
      setup.window_tree_test_helper()->SetWindowBounds(top_level, bounds));
  EXPECT_TRUE(setup.changes()->empty());
}

TEST(WindowTreeTest, SetChildWindowBounds) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  const gfx::Rect bounds = gfx::Rect(1, 2, 300, 400);
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(window, bounds));
  EXPECT_EQ(bounds, window->bounds());

  // Setting to same bounds should return true.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(window, bounds));
  EXPECT_EQ(bounds, window->bounds());
}

TEST(WindowTreeTest, SetBoundsAtEmbedWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  const gfx::Rect bounds1 = gfx::Rect(1, 2, 300, 400);
  EXPECT_TRUE(
      setup.window_tree_test_helper()->SetWindowBounds(window, bounds1));

  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);

  // Child client should not be able to change bounds of embed window.
  EXPECT_FALSE(embedding_helper->window_tree_test_helper->SetWindowBounds(
      window, gfx::Rect()));
  // Bounds should not have changed.
  EXPECT_EQ(bounds1, window->bounds());

  embedding_helper->window_tree_client.tracker()->changes()->clear();
  embedding_helper->window_tree_client.set_track_root_bounds_changes(true);

  // Set the bounds from the parent and ensure client is notified.
  const gfx::Rect bounds2 = gfx::Rect(1, 2, 300, 401);
  base::Optional<viz::LocalSurfaceId> local_surface_id(
      viz::LocalSurfaceId(1, 2, base::UnguessableToken::Create()));
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(
      window, bounds2, local_surface_id));
  EXPECT_EQ(bounds2, window->bounds());
  ASSERT_EQ(1u,
            embedding_helper->window_tree_client.tracker()->changes()->size());
  const Change bounds_change =
      (*(embedding_helper->window_tree_client.tracker()->changes()))[0];
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, bounds_change.type);
  EXPECT_EQ(bounds2, bounds_change.bounds2);
  EXPECT_EQ(local_surface_id, bounds_change.local_surface_id);
  embedding_helper->window_tree_client.tracker()->changes()->clear();

  // Set the bounds from the parent, only updating the LocalSurfaceId (bounds
  // remains the same). The client should be notified.
  base::Optional<viz::LocalSurfaceId> local_surface_id2(
      viz::LocalSurfaceId(1, 3, base::UnguessableToken::Create()));
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(
      window, bounds2, local_surface_id2));
  EXPECT_EQ(bounds2, window->bounds());
  ASSERT_EQ(1u,
            embedding_helper->window_tree_client.tracker()->changes()->size());
  const Change bounds_change2 =
      (*(embedding_helper->window_tree_client.tracker()->changes()))[0];
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, bounds_change2.type);
  EXPECT_EQ(bounds2, bounds_change2.bounds2);
  EXPECT_EQ(local_surface_id2, bounds_change2.local_surface_id);
  embedding_helper->window_tree_client.tracker()->changes()->clear();

  // Try again with the same values. This should succeed, but not notify the
  // client.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(
      window, bounds2, local_surface_id2));
  EXPECT_TRUE(
      embedding_helper->window_tree_client.tracker()->changes()->empty());
}

// Tests the ability of the client to change properties on the server.
TEST(WindowTreeTest, SetTopLevelWindowProperty) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();

  EXPECT_FALSE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  aura::PropertyConverter::PrimitiveType client_value = true;
  std::vector<uint8_t> client_transport_value =
      mojo::ConvertTo<std::vector<uint8_t>>(client_value);
  setup.window_tree_test_helper()->SetWindowProperty(
      top_level, mojom::WindowManager::kAlwaysOnTop_Property,
      client_transport_value, 2);
  EXPECT_EQ("ChangeCompleted id=2 success=true",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  setup.changes()->clear();

  top_level->SetProperty(aura::client::kAlwaysOnTopKey, false);
  EXPECT_EQ(
      "PropertyChanged window=0,1 key=prop:always_on_top "
      "value=0000000000000000",
      SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, WindowToWindowData) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  setup.changes()->clear();

  window->SetBounds(gfx::Rect(1, 2, 300, 400));
  window->SetProperty(aura::client::kAlwaysOnTopKey, true);
  window->Show();  // Called to make the window visible.
  mojom::WindowDataPtr data =
      setup.window_tree_test_helper()->WindowToWindowData(window);
  EXPECT_EQ(gfx::Rect(1, 2, 300, 400), data->bounds);
  EXPECT_TRUE(data->visible);
  EXPECT_EQ(
      1u, data->properties.count(mojom::WindowManager::kAlwaysOnTop_Property));
  EXPECT_EQ(aura::PropertyConverter::PrimitiveType(true),
            mojo::ConvertTo<aura::PropertyConverter::PrimitiveType>(
                data->properties[mojom::WindowManager::kAlwaysOnTop_Property]));
}

TEST(WindowTreeTest, SetWindowPointerProperty) {
  WindowServiceTestSetup setup;
  setup.service()->property_converter()->RegisterWindowPtrProperty(
      kTestPropertyKey, kTestPropertyServerKey);

  WindowTreeTestHelper* helper = setup.window_tree_test_helper();
  aura::Window* top_level1 = helper->NewTopLevelWindow();
  aura::Window* top_level2 = helper->NewTopLevelWindow();
  Id id1 = helper->TransportIdForWindow(top_level1);
  Id id2 = helper->TransportIdForWindow(top_level2);

  base::Optional<std::vector<uint8_t>> value =
      mojo::ConvertTo<std::vector<uint8_t>>(id2);
  setup.window_tree_test_helper()->window_tree()->SetWindowProperty(
      1, id1, kTestPropertyServerKey, value);
  EXPECT_EQ(top_level2, top_level1->GetProperty(kTestPropertyKey));

  value.reset();
  setup.window_tree_test_helper()->window_tree()->SetWindowProperty(
      1, id1, kTestPropertyServerKey, value);
  EXPECT_FALSE(top_level1->GetProperty(kTestPropertyKey));
}

TEST(WindowTreeTest, SetWindowPointerPropertyWithInvalidValues) {
  WindowServiceTestSetup setup;
  setup.service()->property_converter()->RegisterWindowPtrProperty(
      kTestPropertyKey, kTestPropertyServerKey);

  WindowTreeTestHelper* helper = setup.window_tree_test_helper();
  aura::Window* top_level = helper->NewTopLevelWindow();
  Id id = helper->TransportIdForWindow(top_level);
  base::Optional<std::vector<uint8_t>> value =
      mojo::ConvertTo<std::vector<uint8_t>>(kInvalidTransportId);
  setup.window_tree_test_helper()->window_tree()->SetWindowProperty(
      1, id, kTestPropertyServerKey, value);
  EXPECT_FALSE(top_level->GetProperty(kTestPropertyKey));

  value = mojo::ConvertTo<std::vector<uint8_t>>(10);
  setup.window_tree_test_helper()->window_tree()->SetWindowProperty(
      1, id, kTestPropertyServerKey, value);
  EXPECT_FALSE(top_level->GetProperty(kTestPropertyKey));

  value->clear();
  value->push_back(1);
  setup.window_tree_test_helper()->window_tree()->SetWindowProperty(
      1, id, kTestPropertyServerKey, value);
  EXPECT_FALSE(top_level->GetProperty(kTestPropertyKey));
}

TEST(WindowTreeTest, OnWindowInputEventAck) {
  WindowServiceTestSetup setup;
  setup.set_ack_events_immediately(false);
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  WindowTreeTestHelper* tree = setup.window_tree_test_helper();
  aura::Window* top_level = tree->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->Focus();
  top_level->SetBounds(gfx::Rect(100, 100));

  // Send a key event and a mouse event to the client.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  event_generator.MoveMouseTo(10, 10);
  ASSERT_EQ(2u, window_tree_client->input_events().size());
  TestWindowTreeClient::InputEvent event1 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event1.event->IsKeyEvent());
  TestWindowTreeClient::InputEvent event2 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event2.event->IsLocatedEvent());

  // Acking the events in the order they were received works fine.
  EXPECT_EQ(1u, tree->in_flight_key_events().size());
  tree->OnWindowInputEventAck(event1.event_id, ws::mojom::EventResult::HANDLED);
  EXPECT_EQ(0u, tree->in_flight_key_events().size());
  EXPECT_EQ(1u, tree->in_flight_other_events().size());
  tree->OnWindowInputEventAck(event2.event_id, ws::mojom::EventResult::HANDLED);
  EXPECT_EQ(0u, tree->in_flight_other_events().size());

  // Send another key event and a mouse event.
  event_generator.ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  event_generator.MoveMouseTo(11, 11);
  ASSERT_EQ(2u, window_tree_client->input_events().size());
  TestWindowTreeClient::InputEvent event3 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event3.event->IsKeyEvent());
  TestWindowTreeClient::InputEvent event4 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event4.event->IsLocatedEvent());

  // Acking the mouse and key events out of order from one another is okay.
  EXPECT_EQ(1u, tree->in_flight_other_events().size());
  tree->OnWindowInputEventAck(event4.event_id, ws::mojom::EventResult::HANDLED);
  EXPECT_EQ(0u, tree->in_flight_other_events().size());
  EXPECT_EQ(1u, tree->in_flight_key_events().size());
  tree->OnWindowInputEventAck(event3.event_id, ws::mojom::EventResult::HANDLED);
  EXPECT_EQ(0u, tree->in_flight_key_events().size());

  // Send two more mouse events.
  event_generator.MoveMouseTo(12, 12);
  event_generator.MoveMouseTo(13, 13);
  ASSERT_EQ(2u, window_tree_client->input_events().size());
  TestWindowTreeClient::InputEvent event5 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event5.event->IsLocatedEvent());
  TestWindowTreeClient::InputEvent event6 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event6.event->IsLocatedEvent());

  // The client cannot ack the second mouse event before the first.
  EXPECT_EQ(2u, tree->in_flight_other_events().size());
  tree->OnWindowInputEventAck(event6.event_id, ws::mojom::EventResult::HANDLED);
  EXPECT_EQ(2u, tree->in_flight_other_events().size());

  // Send a key-press.
  event_generator.PressKey(ui::VKEY_A, ui::EF_NONE);
  ASSERT_EQ(1u, window_tree_client->input_events().size());
  TestWindowTreeClient::InputEvent event7 = window_tree_client->PopInputEvent();
  ASSERT_TRUE(event7.event->IsKeyEvent());
  // Acking the wrong event should be ignored.
  tree->OnWindowInputEventAck(event7.event_id + 11,
                              ws::mojom::EventResult::HANDLED);
  EXPECT_EQ(1u, tree->in_flight_key_events().size());
}

TEST(WindowTreeTest, EventLocation) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  WindowTreeTestHelper* helper = setup.window_tree_test_helper();
  aura::Window* top_level = helper->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 20, 100, 100));

  // Add a child Window that covers the bottom half of the top-level window.
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  window->Show();
  window->SetBounds(gfx::Rect(0, 50, 100, 50));
  top_level->AddChild(window);

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(33, 44);
  ASSERT_EQ(1u, window_tree_client->input_events().size());
  TestWindowTreeClient::InputEvent event1 = window_tree_client->PopInputEvent();
  EXPECT_EQ(helper->TransportIdForWindow(top_level), event1.window_id);
  ASSERT_TRUE(event1.event->IsLocatedEvent());
  ui::LocatedEvent* located_event1 = event1.event->AsLocatedEvent();
  // The location is in the target window's coordinate system.
  EXPECT_EQ(gfx::Point(23, 24), located_event1->location());
  // The root location is in the client-root coordinate system.
  EXPECT_EQ(gfx::Point(23, 24), located_event1->root_location());

  event_generator.MoveMouseTo(55, 86);
  // 2 input events should happen -- exit on |top_level| and enter on |window|.
  ASSERT_EQ(2u, window_tree_client->input_events().size());

  // Check the exit event on |top_level|.
  TestWindowTreeClient::InputEvent event2 = window_tree_client->PopInputEvent();
  EXPECT_EQ(helper->TransportIdForWindow(top_level), event2.window_id);
  ASSERT_TRUE(event2.event->IsLocatedEvent());
  ui::LocatedEvent* located_event2 = event2.event->AsLocatedEvent();
  // The location is in the target window's coordinate system.
  EXPECT_EQ(gfx::Point(45, 66), located_event2->location());
  // The root location is in the client-root coordinate system.
  EXPECT_EQ(gfx::Point(45, 66), located_event2->root_location());

  // Check the enter event on |window|.
  TestWindowTreeClient::InputEvent event3 = window_tree_client->PopInputEvent();
  EXPECT_EQ(helper->TransportIdForWindow(window), event3.window_id);
  ASSERT_TRUE(event3.event->IsLocatedEvent());
  ui::LocatedEvent* located_event3 = event3.event->AsLocatedEvent();
  // The location is in the target window's coordinate system.
  EXPECT_EQ(gfx::Point(45, 16), located_event3->location());
  // The root location is in the client-root coordinate system.
  EXPECT_EQ(gfx::Point(45, 66), located_event3->root_location());
}

TEST(WindowTreeTest, EventLocationForTransientChildWindow) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  WindowTreeTestHelper* helper = setup.window_tree_test_helper();

  aura::Window* top_level = helper->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 20, 100, 100));

  aura::Window* transient = helper->NewTopLevelWindow();
  ASSERT_TRUE(transient);
  transient->Show();
  transient->SetBounds(gfx::Rect(50, 30, 60, 90));

  helper->window_tree()->AddTransientWindow(
      10, helper->TransportIdForWindow(top_level),
      helper->TransportIdForWindow(transient));

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(61, 44);
  ASSERT_EQ(1u, window_tree_client->input_events().size());
  TestWindowTreeClient::InputEvent event = window_tree_client->PopInputEvent();
  EXPECT_EQ(helper->TransportIdForWindow(transient), event.window_id);
  ASSERT_TRUE(event.event->IsLocatedEvent());
  ui::LocatedEvent* located_event = event.event->AsLocatedEvent();
  // The location is in the target window's coordinate system.
  EXPECT_EQ(gfx::Point(11, 14), located_event->location());
  // The root location is in the client-root coordinate system. Transient
  // parents won't affect the coordinate system.
  EXPECT_EQ(gfx::Point(11, 14), located_event->root_location());
}

TEST(WindowTreeTest, MovePressDragRelease) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  EXPECT_EQ("MOUSE_MOVED 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.PressLeftButton();
  EXPECT_EQ("MOUSE_PRESSED 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.MoveMouseTo(0, 0);
  EXPECT_EQ("MOUSE_DRAGGED -10,-10",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.ReleaseLeftButton();
  EXPECT_EQ("MOUSE_RELEASED -10,-10",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
}

// Used to verify destruction with a touch pointer down doesn't crash.
TEST(WindowTreeTest, ShutdownWithTouchDown) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.set_current_location(gfx::Point(50, 51));
  event_generator.PressTouch();
}

TEST(WindowTreeTest, TouchPressDragRelease) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 11, 100, 100));

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.set_current_location(gfx::Point(50, 51));
  event_generator.PressTouch();
  EXPECT_EQ("ET_TOUCH_PRESSED 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.MoveTouch(gfx::Point(5, 6));
  EXPECT_EQ("ET_TOUCH_MOVED -5,-5",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.ReleaseTouch();
  EXPECT_EQ("ET_TOUCH_RELEASED -5,-5",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
}

class EventRecordingWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  EventRecordingWindowDelegate() = default;
  ~EventRecordingWindowDelegate() override = default;

  std::queue<std::unique_ptr<ui::Event>>& events() { return events_; }

  std::unique_ptr<ui::Event> PopEvent() {
    if (events_.empty())
      return nullptr;
    auto event = std::move(events_.front());
    events_.pop();
    return event;
  }

  void ClearEvents() {
    std::queue<std::unique_ptr<ui::Event>> events;
    std::swap(events_, events);
  }

  // aura::test::TestWindowDelegate:
  void OnEvent(ui::Event* event) override {
    events_.push(ui::Event::Clone(*event));
  }

 private:
  std::queue<std::unique_ptr<ui::Event>> events_;

  DISALLOW_COPY_AND_ASSIGN(EventRecordingWindowDelegate);
};

TEST(WindowTreeTest, MoveFromClientToNonClient) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  window_delegate.ClearEvents();

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  EXPECT_EQ("MOUSE_MOVED 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  // The delegate should see the same events (but as mouse events).
  EXPECT_EQ("MOUSE_ENTERED 40,40", LocatedEventToEventTypeAndLocation(
                                       window_delegate.PopEvent().get()));
  EXPECT_EQ("MOUSE_MOVED 40,40", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));

  // Move the mouse over the non-client area.
  // The event is still sent to the client, and the delegate.
  event_generator.MoveMouseTo(15, 16);
  EXPECT_EQ("MOUSE_MOVED 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  // Delegate should also get the events.
  EXPECT_EQ("MOUSE_MOVED 5,6", LocatedEventToEventTypeAndLocation(
                                   window_delegate.PopEvent().get()));

  // Only the delegate should get the press in this case.
  event_generator.PressLeftButton();
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());

  EXPECT_EQ("MOUSE_PRESSED 5,6", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));

  // Move mouse into client area, only the delegate should get the move (drag).
  event_generator.MoveMouseTo(35, 51);
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());

  EXPECT_EQ("MOUSE_DRAGGED 25,41", LocatedEventToEventTypeAndLocation(
                                       window_delegate.PopEvent().get()));

  // Release over client area, again only delegate should get it.
  event_generator.ReleaseLeftButton();
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());

  EXPECT_EQ("MOUSE_RELEASED",
            EventToEventType(window_delegate.PopEvent().get()));

  event_generator.MoveMouseTo(26, 50);
  EXPECT_EQ("MOUSE_MOVED 16,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  // Delegate should also get the events.
  EXPECT_EQ("MOUSE_MOVED 16,40", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));

  // Press in client area. Only the client should get the event.
  event_generator.PressLeftButton();
  EXPECT_EQ("MOUSE_PRESSED 16,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  ASSERT_FALSE(window_delegate.PopEvent().get());
}

TEST(WindowTreeTest, MouseDownInNonClientWithChildWindow) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  // Add a child Window that is sized to fill the top-level.
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  window->Show();
  window->SetBounds(gfx::Rect(top_level->bounds().size()));
  top_level->AddChild(window);

  window_delegate.ClearEvents();

  // Move the mouse over the non-client area. Both the client and the delegate
  // should get the event.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(15, 16);
  EXPECT_EQ("MOUSE_MOVED 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
  EXPECT_TRUE(window_tree_client->input_events().empty());
  EXPECT_EQ("MOUSE_ENTERED",
            EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_EQ("MOUSE_MOVED", EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_TRUE(window_delegate.events().empty());

  // Press over the non-client. The client should not be notified as the event
  // should be handled locally.
  event_generator.PressLeftButton();
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());
  EXPECT_EQ("MOUSE_PRESSED 5,6", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));
}

TEST(WindowTreeTest, MouseDownInNonClientDragToClientWithChildWindow) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  // Add a child Window that is sized to fill the top-level.
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  window->Show();
  window->SetBounds(gfx::Rect(top_level->bounds().size()));
  top_level->AddChild(window);

  // Press in non-client area.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(15, 16);
  event_generator.PressLeftButton();

  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  window_tree_client->ClearInputEvents();
  window_delegate.ClearEvents();
  // Drag over client area, only the delegate should get it (because the press
  // was in the non-client area).
  event_generator.MoveMouseTo(15, 26);
  EXPECT_EQ("MOUSE_DRAGGED",
            EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_TRUE(window_tree_client->input_events().empty());
}

TEST(WindowTreeTest, SetHitTestInsets) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  window_tree_client->ClearInputEvents();
  window_delegate.ClearEvents();

  // Set the hit test insets in the window's bounds that excludes the top half.
  setup.window_tree_test_helper()->SetHitTestInsets(
      top_level, gfx::Insets(50, 0, 0, 0), gfx::Insets(50, 0, 0, 0));

  // Events outside the hit test insets are not seen by the delegate or client.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 30);
  EXPECT_TRUE(window_tree_client->input_events().empty());
  EXPECT_TRUE(window_delegate.events().empty());

  // Events in the hit test insets are seen by the delegate and client.
  event_generator.MoveMouseTo(50, 80);
  EXPECT_EQ("MOUSE_MOVED 40,70",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
  EXPECT_EQ("MOUSE_ENTERED 40,70", LocatedEventToEventTypeAndLocation(
                                       window_delegate.PopEvent().get()));
  EXPECT_EQ("MOUSE_MOVED 40,70", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));
}

TEST(WindowTreeTest, EventObserver) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  // Start observing mouse press and release.
  setup.window_tree_test_helper()->window_tree()->ObserveEventTypes(
      {ui::mojom::EventType::MOUSE_PRESSED_EVENT,
       ui::mojom::EventType::MOUSE_RELEASED_EVENT});

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  ASSERT_TRUE(window_tree_client->observed_events().empty());

  event_generator.MoveMouseTo(5, 6);
  ASSERT_TRUE(window_tree_client->observed_events().empty());

  event_generator.PressLeftButton();
  EXPECT_EQ("MOUSE_PRESSED 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedEvent().get()));

  event_generator.ReleaseLeftButton();
  EXPECT_EQ("MOUSE_RELEASED 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedEvent().get()));

  // Start also observing mouse move events.
  setup.window_tree_test_helper()->window_tree()->ObserveEventTypes(
      {ui::mojom::EventType::MOUSE_PRESSED_EVENT,
       ui::mojom::EventType::MOUSE_RELEASED_EVENT,
       ui::mojom::EventType::MOUSE_MOVED_EVENT});
  event_generator.MoveMouseTo(8, 9);
  EXPECT_EQ("MOUSE_MOVED 8,9",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedEvent().get()));
}

TEST(WindowTreeTest, MatchesEventObserverSet) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  // Start observing touch press and release.
  setup.window_tree_test_helper()->window_tree()->ObserveEventTypes(
      {ui::mojom::EventType::TOUCH_PRESSED,
       ui::mojom::EventType::TOUCH_RELEASED});

  ui::test::EventGenerator event_generator(setup.root());
  event_generator.set_current_location(gfx::Point(50, 50));
  event_generator.PressTouch();

  // The client should get the input event, and |matches_event_observer| should
  // be true (because it also matched the event observer).
  TestWindowTreeClient::InputEvent press_input =
      window_tree_client->PopInputEvent();
  ASSERT_TRUE(press_input.event);
  EXPECT_EQ("ET_TOUCH_PRESSED 40,40",
            LocatedEventToEventTypeAndLocation(press_input.event.get()));
  EXPECT_TRUE(press_input.matches_event_observer);
  // The event targeted the client, so there are no separately observed events.
  EXPECT_TRUE(window_tree_client->observed_events().empty());
}

TEST(WindowTreeTest, Capture) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();

  // Setting capture on |window| should fail as it's not visible.
  EXPECT_FALSE(setup.window_tree_test_helper()->SetCapture(window));

  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  EXPECT_FALSE(setup.window_tree_test_helper()->SetCapture(top_level));
  top_level->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));

  EXPECT_FALSE(setup.window_tree_test_helper()->ReleaseCapture(window));
  EXPECT_TRUE(setup.window_tree_test_helper()->ReleaseCapture(top_level));

  top_level->AddChild(window);
  window->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.window_tree_test_helper()->ReleaseCapture(window));
}

TEST(WindowTreeTest, CaptureDisallowedWhenEmbedderInterceptsEvents) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  top_level->AddChild(window);
  window->Show();
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window, mojom::kEmbedFlagEmbedderInterceptsEvents);
  ASSERT_TRUE(embedding_helper);
  EXPECT_FALSE(embedding_helper->window_tree_test_helper->SetCapture(window));
}

TEST(WindowTreeTest, TransferCaptureToClient) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  wm::CaptureController::Get()->SetCapture(top_level);
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(6, 6);
  setup.window_tree_client()->ClearInputEvents();
  window_delegate.ClearEvents();
  event_generator.MoveMouseTo(7, 7);

  // Because capture was initiated locally event should go to |window_delegate|
  // only (not the client).
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_EQ("MOUSE_MOVED", EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_TRUE(window_delegate.events().empty());

  // Request capture from the client.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  event_generator.MoveMouseTo(8, 8);
  // Now the event should go to the client and not local.
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_EQ("MOUSE_MOVED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
}

TEST(WindowTreeTest, TransferCaptureBetweenParentAndChild) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  top_level->AddChild(window);
  window->Show();
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);

  // Move the mouse and set capture from the child.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(6, 6);
  setup.window_tree_client()->ClearInputEvents();
  window_delegate.ClearEvents();
  embedding_helper->window_tree_client.ClearInputEvents();
  EXPECT_TRUE(embedding_helper->window_tree_test_helper->SetCapture(window));
  event_generator.MoveMouseTo(7, 7);

  // As capture was set from the child, only the child should get the event.
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_EQ(
      "MOUSE_MOVED",
      EventToEventType(
          embedding_helper->window_tree_client.PopInputEvent().event.get()));
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());

  // Set capture from the parent, only the parent should get the event now.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  event_generator.MoveMouseTo(8, 8);
  EXPECT_EQ("MOUSE_MOVED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());
}

TEST(WindowTreeTest, CaptureNotification) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  setup.changes()->clear();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.changes()->empty());

  wm::CaptureController::Get()->ReleaseCapture(window);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(setup.changes())));
}

TEST(WindowTreeTest, CaptureNotificationForEmbedRoot) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  setup.changes()->clear();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.changes()->empty());

  // Set capture on the embed-root from the embedded client. The embedder
  // should be notified.
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);
  setup.changes()->clear();
  embedding_helper->changes()->clear();
  EXPECT_TRUE(embedding_helper->window_tree_test_helper->SetCapture(window));
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(setup.changes())));
  setup.changes()->clear();
  EXPECT_TRUE(embedding_helper->changes()->empty());

  // Set capture from the embedder. This triggers the embedded client to lose
  // capture.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.changes()->empty());
  // NOTE: the '2' is because the embedded client sees the high order bits of
  // the root.
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=2,1",
            SingleChangeToDescription(*(embedding_helper->changes())));
  embedding_helper->changes()->clear();

  // And release capture locally.
  wm::CaptureController::Get()->ReleaseCapture(window);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(setup.changes())));
  EXPECT_TRUE(embedding_helper->changes()->empty());
}

TEST(WindowTreeTest, CaptureNotificationForTopLevel) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow(11);
  ASSERT_TRUE(top_level);
  top_level->Show();
  setup.changes()->clear();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  EXPECT_TRUE(setup.changes()->empty());

  // Release capture locally.
  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  capture_controller->ReleaseCapture(top_level);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,11",
            SingleChangeToDescription(*(setup.changes())));
  setup.changes()->clear();

  // Set capture locally.
  capture_controller->SetCapture(top_level);
  EXPECT_TRUE(setup.changes()->empty());

  // Set capture from client.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  EXPECT_TRUE(setup.changes()->empty());

  // Release locally.
  capture_controller->ReleaseCapture(top_level);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,11",
            SingleChangeToDescription(*(setup.changes())));
}

TEST(WindowTreeTest, EventsGoToCaptureWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetBounds(gfx::Rect(10, 10, 90, 90));
  // Left press on the top-level, leaving mouse down.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  event_generator.PressLeftButton();
  setup.window_tree_client()->ClearInputEvents();

  // Set capture on |window|.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());

  // Move mouse, should go to |window|.
  event_generator.MoveMouseTo(6, 6);
  auto drag_event = setup.window_tree_client()->PopInputEvent();
  EXPECT_EQ(setup.window_tree_test_helper()->TransportIdForWindow(window),
            drag_event.window_id);
  EXPECT_EQ("MOUSE_DRAGGED -4,-4",
            LocatedEventToEventTypeAndLocation(drag_event.event.get()));
}

TEST(WindowTreeTest, PointerDownResetOnCaptureChange) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->AddChild(window);
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));
  top_level->Show();
  window->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetBounds(gfx::Rect(10, 10, 90, 90));
  // Left press on the top-level, leaving mouse down.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  event_generator.PressLeftButton();
  ServerWindow* top_level_server_window = ServerWindow::GetMayBeNull(top_level);
  ASSERT_TRUE(top_level_server_window);
  ServerWindowTestHelper top_level_server_window_helper(
      top_level_server_window);
  EXPECT_TRUE(top_level_server_window_helper.IsHandlingPointerPress(
      ui::MouseEvent::kMousePointerId));

  // Set capture on |window|, top_level should no longer be in pointer-down
  // (because capture changed).
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_FALSE(top_level_server_window_helper.IsHandlingPointerPress(
      ui::MouseEvent::kMousePointerId));
}

TEST(WindowTreeTest, PointerDownResetOnHide) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));
  top_level->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  // Left press on the top-level, leaving mouse down.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  event_generator.PressLeftButton();
  ServerWindow* top_level_server_window = ServerWindow::GetMayBeNull(top_level);
  ASSERT_TRUE(top_level_server_window);
  ServerWindowTestHelper top_level_server_window_helper(
      top_level_server_window);
  EXPECT_TRUE(top_level_server_window_helper.IsHandlingPointerPress(
      ui::MouseEvent::kMousePointerId));

  // Hiding should implicitly cancel capture.
  top_level->Hide();
  EXPECT_FALSE(top_level_server_window_helper.IsHandlingPointerPress(
      ui::MouseEvent::kMousePointerId));
}

TEST(WindowTreeTest, DeleteWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  aura::WindowTracker tracker;
  tracker.Add(window);
  setup.changes()->clear();
  setup.window_tree_test_helper()->DeleteWindow(window);
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, DeleteTopLevel) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  const ClientWindowId top_level_id =
      setup.window_tree_test_helper()->ClientWindowIdForWindow(top_level);
  ASSERT_TRUE(top_level);
  aura::WindowTracker tracker;
  tracker.Add(top_level);
  setup.changes()->clear();

  // Ask the tree to delete the window, which should result in deleting the
  // Window as well responding with success.
  setup.window_tree_test_helper()->DeleteWindow(top_level);
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));

  // Make sure the WindowTree doesn't have a mapping for the id anymore.
  EXPECT_FALSE(
      setup.window_tree_test_helper()->GetWindowByClientId(top_level_id));
}

TEST(WindowTreeTest, ExternalDeleteTopLevel) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();
  ASSERT_TRUE(top_level);
  delete top_level;
  EXPECT_EQ("WindowDeleted window=0,1",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, ExternalDeleteWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  setup.changes()->clear();
  delete window;
  EXPECT_EQ("WindowDeleted window=0,1",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, Embed) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  ASSERT_TRUE(embed_window);
  window->AddChild(embed_window);
  embed_window->SetBounds(gfx::Rect(1, 2, 3, 4));
  setup.changes()->clear();

  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(embed_window);
  ASSERT_TRUE(embedding_helper);
  ASSERT_EQ("OnEmbed", SingleChangeToDescription(*embedding_helper->changes()));
  const Change& test_change = (*embedding_helper->changes())[0];
  ASSERT_EQ(1u, test_change.windows.size());
  EXPECT_EQ(embed_window->bounds(), test_change.windows[0].bounds);
  EXPECT_EQ(kInvalidTransportId, test_change.windows[0].parent_id);
  EXPECT_EQ(embed_window->TargetVisibility(), test_change.windows[0].visible);
  EXPECT_NE(kInvalidTransportId, test_change.windows[0].window_id);

  // OnFrameSinkIdAllocated() should called on the parent tree.
  ASSERT_EQ(1u, setup.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_FRAME_SINK_ID_ALLOCATED, (*setup.changes())[0].type);
  const Id embed_window_transport_id =
      setup.window_tree_test_helper()->TransportIdForWindow(embed_window);
  EXPECT_EQ(embed_window_transport_id, (*setup.changes())[0].window_id);
  EXPECT_EQ(ServerWindow::GetMayBeNull(embed_window)->frame_sink_id(),
            (*setup.changes())[0].frame_sink_id);
}

// Base class for ScheduleEmbed() related tests. This creates a Window and
// prepares a secondary client (|embed_client_|) that is intended to be embedded
// at some point.
class WindowTreeScheduleEmbedTest : public testing::Test {
 public:
  WindowTreeScheduleEmbedTest() = default;
  ~WindowTreeScheduleEmbedTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    setup_ = std::make_unique<WindowServiceTestSetup>();
    embed_binding_.Bind(mojo::MakeRequest(&embed_client_ptr_));
    window_ = setup_->window_tree_test_helper()->NewWindow();
    ASSERT_TRUE(window_);
  }
  void TearDown() override {
    window_ = nullptr;
    embed_binding_.Close();
    setup_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<WindowServiceTestSetup> setup_;
  TestWindowTreeClient embed_client_;
  mojom::WindowTreeClientPtr embed_client_ptr_;
  aura::Window* window_ = nullptr;

 private:
  mojo::Binding<mojom::WindowTreeClient> embed_binding_{&embed_client_};

  DISALLOW_COPY_AND_ASSIGN(WindowTreeScheduleEmbedTest);
};

TEST_F(WindowTreeScheduleEmbedTest, ScheduleEmbedWithUnregisteredToken) {
  bool embed_result = false;
  bool embed_callback_called = false;
  setup_->window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup_->window_tree_test_helper()->TransportIdForWindow(window_),
      base::UnguessableToken::Create(), kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  // ScheduleEmbed() with an invalid token should fail.
  EXPECT_FALSE(embed_result);
}

TEST_F(WindowTreeScheduleEmbedTest, ScheduleEmbedRegisteredTokenInvalidWindow) {
  // Register a token for embedding.
  base::UnguessableToken token;
  setup_->window_tree_test_helper()->window_tree()->ScheduleEmbed(
      std::move(embed_client_ptr_),
      base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  bool embed_result = false;
  bool embed_callback_called = false;
  setup_->window_tree_test_helper()->window_tree()->EmbedUsingToken(
      kInvalidTransportId, token, kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  // ScheduleEmbed() with a valid token, but invalid window should fail.
  EXPECT_FALSE(embed_result);
}

TEST_F(WindowTreeScheduleEmbedTest, ScheduleEmbed) {
  base::UnguessableToken token;
  // ScheduleEmbed() with a valid token and valid window.
  setup_->window_tree_test_helper()->window_tree()->ScheduleEmbed(
      std::move(embed_client_ptr_),
      base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  bool embed_result = false;
  bool embed_callback_called = false;
  setup_->window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup_->window_tree_test_helper()->TransportIdForWindow(window_), token,
      kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);
  base::RunLoop().RunUntilIdle();

  // The embedded client should get OnEmbed().
  EXPECT_EQ("OnEmbed",
            SingleChangeToDescription(*embed_client_.tracker()->changes()));
}

TEST(WindowTreeTest, ScheduleEmbedForExistingClient) {
  WindowServiceTestSetup setup;
  // Schedule an embed in the tree created by |setup|.
  base::UnguessableToken token;
  const uint32_t window_id_in_child = 149;
  setup.window_tree_test_helper()
      ->window_tree()
      ->ScheduleEmbedForExistingClient(
          window_id_in_child, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Create another client and a window.
  TestWindowTreeClient client2;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&client2);
  ASSERT_TRUE(tree2);
  WindowTreeTestHelper tree2_test_helper(tree2.get());
  aura::Window* window_in_parent = tree2_test_helper.NewWindow();
  ASSERT_TRUE(window_in_parent);

  // Call EmbedUsingToken() from tree2, which should result in the tree from
  // |setup| getting OnEmbedFromToken().
  bool embed_result = false;
  bool embed_callback_called = false;
  WindowTreeTestHelper(tree2.get())
      .window_tree()
      ->EmbedUsingToken(
          tree2_test_helper.TransportIdForWindow(window_in_parent), token,
          kDefaultEmbedFlags,
          base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                         &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);

  EXPECT_EQ("OnEmbedFromToken", SingleChangeToDescription(*setup.changes()));
  EXPECT_EQ(
      static_cast<Id>(window_id_in_child),
      setup.window_tree_test_helper()->TransportIdForWindow(window_in_parent));
}

TEST(WindowTreeTest, DeleteRootOfEmbeddingFromScheduleEmbedForExistingClient) {
  WindowServiceTestSetup setup;
  aura::Window* window_in_parent = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window_in_parent);

  // Create another client.
  TestWindowTreeClient client2;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&client2);
  WindowTreeTestHelper tree2_test_helper(tree2.get());
  base::UnguessableToken token;
  tree2_test_helper.window_tree()->ScheduleEmbedForExistingClient(
      11, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Call EmbedUsingToken() from setup.window_tree(), which should result in
  // |tree2| getting OnEmbedFromToken().
  bool embed_result = false;
  bool embed_callback_called = false;
  setup.window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup.window_tree_test_helper()->TransportIdForWindow(window_in_parent),
      token, kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);

  EXPECT_EQ("OnEmbedFromToken",
            SingleChangeToDescription(*client2.tracker()->changes()));
  client2.tracker()->changes()->clear();

  // Delete |window_in_parent|, which should trigger notifying tree2.
  setup.window_tree_test_helper()->DeleteWindow(window_in_parent);
  window_in_parent = nullptr;

  // 11 is the same value supplied to ScheduleEmbedForExistingClient().
  EXPECT_EQ("WindowDeleted window=0,11",
            SingleChangeToDescription(*client2.tracker()->changes()));
}

TEST(WindowTreeTest, DeleteEmbededTreeFromScheduleEmbedForExistingClient) {
  WindowServiceTestSetup setup;
  aura::Window* window_in_parent = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window_in_parent);

  // Create another client and call ScheduleEmbedForExistingClient() from it.
  TestWindowTreeClient client2;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&client2);
  WindowTreeTestHelper tree2_test_helper(tree2.get());
  base::UnguessableToken token;
  tree2_test_helper.window_tree()->ScheduleEmbedForExistingClient(
      11, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Call EmbedUsingToken() from setup.window_tree(), which should result in
  // |tree2| getting OnEmbedFromToken().
  bool embed_result = false;
  bool embed_callback_called = false;
  setup.window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup.window_tree_test_helper()->TransportIdForWindow(window_in_parent),
      token, kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);
  EXPECT_TRUE(ServerWindow::GetMayBeNull(window_in_parent)->HasEmbedding());

  tree2.reset();
  EXPECT_FALSE(ServerWindow::GetMayBeNull(window_in_parent)->HasEmbedding());
}

TEST(WindowTreeTest, StackAtTop) {
  WindowServiceTestSetup setup;
  aura::Window* top_level1 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level1);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->StackAtTop(
      10, setup.window_tree_test_helper()->TransportIdForWindow(top_level1));
  // This succeeds because |top_level1| is already at top. |10| is the value
  // supplied to StackAtTop().
  EXPECT_EQ("ChangeCompleted id=10 success=true",
            SingleChangeToDescription(*setup.changes()));

  // Create another top-level. |top_level2| should initially be above 1.
  aura::Window* top_level2 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level2);
  ASSERT_EQ(2u, top_level1->parent()->children().size());
  EXPECT_EQ(top_level2, top_level1->parent()->children()[1]);

  // Stack 1 at the top.
  EXPECT_TRUE(setup.window_tree_test_helper()->StackAtTop(top_level1));
  EXPECT_EQ(top_level1, top_level1->parent()->children()[1]);

  // Stacking a non-toplevel window at top should fail.
  aura::Window* non_top_level_window =
      setup.window_tree_test_helper()->NewWindow();
  EXPECT_FALSE(
      setup.window_tree_test_helper()->StackAtTop(non_top_level_window));
}

TEST(WindowTreeTest, OnUnhandledKeyEvent) {
  // Create a top-level, show it and give it focus.
  WindowServiceTestSetup setup;
  // This test acks its own events.
  setup.set_ack_events_immediately(false);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->Focus();
  ASSERT_TRUE(top_level->HasFocus());
  ui::test::EventGenerator event_generator(setup.root());

  // Generate a key-press. The client should get the event, but not the
  // delegate.
  event_generator.PressKey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(setup.delegate()->unhandled_key_events()->empty());

  // Respond that the event was not handled. Should result in notifying the
  // delegate.
  EXPECT_TRUE(setup.window_tree_client()->AckFirstEvent(
      setup.window_tree(), mojom::EventResult::UNHANDLED));
  ASSERT_EQ(1u, setup.delegate()->unhandled_key_events()->size());
  EXPECT_EQ(ui::VKEY_A,
            (*setup.delegate()->unhandled_key_events())[0].key_code());
  EXPECT_EQ(ui::EF_CONTROL_DOWN,
            (*setup.delegate()->unhandled_key_events())[0].flags());
  setup.delegate()->unhandled_key_events()->clear();

  // Repeat, but respond with handled. This should not result in the delegate
  // being notified.
  event_generator.PressKey(ui::VKEY_B, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(setup.window_tree_client()->AckFirstEvent(
      setup.window_tree(), mojom::EventResult::HANDLED));
  EXPECT_TRUE(setup.delegate()->unhandled_key_events()->empty());
}

TEST(WindowTreeTest, ReorderWindow) {
  // Create a top-level and two child windows.
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  aura::Window* window1 = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window1);
  top_level->AddChild(window1);
  aura::Window* window2 = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window2);
  top_level->AddChild(window2);

  // Reorder |window1| on top of |window2|.
  EXPECT_TRUE(setup.window_tree_test_helper()->ReorderWindow(
      window1, window2, mojom::OrderDirection::ABOVE));
  EXPECT_EQ(window2, top_level->children()[0]);
  EXPECT_EQ(window1, top_level->children()[1]);

  // Reorder |window2| on top of |window1|.
  EXPECT_TRUE(setup.window_tree_test_helper()->ReorderWindow(
      window2, window1, mojom::OrderDirection::ABOVE));
  EXPECT_EQ(window1, top_level->children()[0]);
  EXPECT_EQ(window2, top_level->children()[1]);

  // Repeat, but use the WindowTree interface, which should result in an ack.
  setup.changes()->clear();
  uint32_t change_id = 101;
  setup.window_tree_test_helper()->window_tree()->ReorderWindow(
      change_id, setup.window_tree_test_helper()->TransportIdForWindow(window1),
      setup.window_tree_test_helper()->TransportIdForWindow(window2),
      mojom::OrderDirection::ABOVE);
  EXPECT_EQ("ChangeCompleted id=101 success=true",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  // Supply invalid window ids, which should fail.
  setup.window_tree_test_helper()->window_tree()->ReorderWindow(
      change_id, 0, 1, mojom::OrderDirection::ABOVE);
  EXPECT_EQ("ChangeCompleted id=101 success=false",
            SingleChangeToDescription(*setup.changes()));

  // These calls should fail as the windows are not siblings.
  EXPECT_FALSE(setup.window_tree_test_helper()->ReorderWindow(
      window1, top_level, mojom::OrderDirection::ABOVE));
  EXPECT_FALSE(setup.window_tree_test_helper()->ReorderWindow(
      top_level, window2, mojom::OrderDirection::ABOVE));
}

TEST(WindowTreeTest, StackAbove) {
  // Create two top-levels.
  WindowServiceTestSetup setup;
  aura::Window* top_level1 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level1);
  aura::Window* top_level2 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level2);
  ASSERT_TRUE(top_level1->parent());
  ASSERT_EQ(top_level1->parent(), top_level2->parent());
  ASSERT_EQ(2u, top_level2->parent()->children().size());

  // 1 on top of 2.
  EXPECT_TRUE(
      setup.window_tree_test_helper()->StackAbove(top_level1, top_level2));
  EXPECT_EQ(top_level2, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level1, top_level2->parent()->children()[1]);

  // Repeat, should still succeed and nothing should change.
  EXPECT_TRUE(
      setup.window_tree_test_helper()->StackAbove(top_level1, top_level2));
  EXPECT_EQ(top_level2, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level1, top_level2->parent()->children()[1]);

  // 2 on top of 1.
  EXPECT_TRUE(
      setup.window_tree_test_helper()->StackAbove(top_level2, top_level1));
  EXPECT_EQ(top_level1, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level2, top_level2->parent()->children()[1]);

  // 1 on top of 2, using WindowTree interface, which should result in an ack.
  setup.changes()->clear();
  uint32_t change_id = 102;
  setup.window_tree_test_helper()->window_tree()->StackAbove(
      change_id,
      setup.window_tree_test_helper()->TransportIdForWindow(top_level1),
      setup.window_tree_test_helper()->TransportIdForWindow(top_level2));
  EXPECT_EQ("ChangeCompleted id=102 success=true",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();
  EXPECT_EQ(top_level2, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level1, top_level2->parent()->children()[1]);

  // Using invalid id should fail.
  setup.window_tree_test_helper()->window_tree()->StackAbove(
      change_id,
      setup.window_tree_test_helper()->TransportIdForWindow(top_level1),
      kInvalidTransportId);
  EXPECT_EQ("ChangeCompleted id=102 success=false",
            SingleChangeToDescription(*setup.changes()));

  // Using non-top-level should fail.
  aura::Window* non_top_level_window =
      setup.window_tree_test_helper()->NewWindow();
  EXPECT_FALSE(setup.window_tree_test_helper()->StackAbove(
      top_level1, non_top_level_window));
}

TEST(WindowTreeTest, VisibilityChanged) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();
  EXPECT_FALSE(window->IsVisible());

  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ("VisibilityChanged window=0,1 visible=true",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, RunMoveLoopTouch) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      12, top_level_id, mojom::MoveLoopSource::TOUCH, gfx::Point());
  // |top_level| isn't visible, so should fail immediately.
  EXPECT_EQ("ChangeCompleted id=12 success=false",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  // Make the window visible and repeat.
  top_level->Show();
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      13, top_level_id, mojom::MoveLoopSource::TOUCH, gfx::Point());
  // WindowServiceDelegate should be asked to do the move.
  WindowServiceDelegate::DoneCallback move_loop_callback =
      setup.delegate()->TakeMoveLoopCallback();
  ASSERT_TRUE(move_loop_callback);
  // As the move is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Respond to the callback with success, which should notify client.
  std::move(move_loop_callback).Run(true);
  EXPECT_EQ("ChangeCompleted id=13 success=true",
            SingleChangeToDescription(*setup.changes()));

  // Trying to move non-top-level should fail.
  aura::Window* non_top_level_window =
      setup.window_tree_test_helper()->NewWindow();
  non_top_level_window->Show();
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      14,
      setup.window_tree_test_helper()->TransportIdForWindow(
          non_top_level_window),
      mojom::MoveLoopSource::TOUCH, gfx::Point());
  EXPECT_EQ("ChangeCompleted id=14 success=false",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, RunMoveLoopMouse) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      12, top_level_id, mojom::MoveLoopSource::MOUSE, gfx::Point());
  // The mouse isn't down, so this should fail.
  EXPECT_EQ("ChangeCompleted id=12 success=false",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  // Press the left button and repeat.
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.PressLeftButton();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      13, top_level_id, mojom::MoveLoopSource::MOUSE, gfx::Point());
  // WindowServiceDelegate should be asked to do the move.
  WindowServiceDelegate::DoneCallback move_loop_callback =
      setup.delegate()->TakeMoveLoopCallback();
  ASSERT_TRUE(move_loop_callback);
  // As the move is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Respond to the callback, which should notify client.
  std::move(move_loop_callback).Run(true);
  EXPECT_EQ("ChangeCompleted id=13 success=true",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();
}

TEST(WindowTreeTest, CancelMoveLoop) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      12, top_level_id, mojom::MoveLoopSource::TOUCH, gfx::Point());

  // WindowServiceDelegate should be asked to do the move.
  WindowServiceDelegate::DoneCallback move_loop_callback =
      setup.delegate()->TakeMoveLoopCallback();
  ASSERT_TRUE(move_loop_callback);
  // As the move is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Cancelling with an invalid id should do nothing.
  EXPECT_FALSE(setup.delegate()->cancel_window_move_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelWindowMove(
      kInvalidTransportId);
  EXPECT_TRUE(setup.changes()->empty());
  EXPECT_FALSE(setup.delegate()->cancel_window_move_loop_called());

  // Cancel with the real id should notify the delegate.
  EXPECT_FALSE(setup.delegate()->cancel_window_move_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelWindowMove(
      top_level_id);
  EXPECT_TRUE(setup.delegate()->cancel_window_move_loop_called());
  // No changes yet, because |move_loop_callback| was not run yet.
  EXPECT_TRUE(setup.changes()->empty());
  // Run the closure, which triggers notifying the client.
  std::move(move_loop_callback).Run(false);
  EXPECT_EQ("ChangeCompleted id=12 success=false",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, CancelMode) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetFocus(top_level));
  // Dispatch a CancelEvent. This should go to the |top_level| as it has focus.
  setup.root()->GetHost()->dispatcher()->DispatchCancelModeEvent();
  EXPECT_EQ("CANCEL_MODE",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
}

TEST(WindowTreeTest, PerformDragDrop) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      12, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  // Let the posted drag loop task run.
  base::RunLoop().RunUntilIdle();

  // WindowServiceDelegate should be asked to run the drag loop.
  WindowServiceDelegate::DragDropCompletedCallback drag_loop_callback =
      setup.delegate()->TakeDragLoopCallback();
  ASSERT_TRUE(drag_loop_callback);

  // As the drag is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Respond with a drop operation, client should be notified with success.
  std::move(drag_loop_callback).Run(ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ("OnPerformDragDropCompleted id=12 success=true action=1",
            SingleChangeToDescription(*setup.changes()));

  // Starts another drag and but the drag is canceled this time.
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      13, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);
  base::RunLoop().RunUntilIdle();
  drag_loop_callback = setup.delegate()->TakeDragLoopCallback();
  ASSERT_TRUE(drag_loop_callback);

  std::move(drag_loop_callback).Run(ui::DragDropTypes::DRAG_NONE);
  EXPECT_EQ("OnPerformDragDropCompleted id=13 success=false action=0",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, PerformDragDropBeforePreviousOneFinish) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      12, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  // PerformDragDrop before the drag loop task runs should fail.
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      13, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);
  EXPECT_EQ("OnPerformDragDropCompleted id=13 success=false action=0",
            SingleChangeToDescription(*setup.changes()));

  // Let the posted drag loop task run.
  base::RunLoop().RunUntilIdle();

  // WindowServiceDelegate should be asked to run the drag loop.
  WindowServiceDelegate::DragDropCompletedCallback drag_loop_callback =
      setup.delegate()->TakeDragLoopCallback();
  ASSERT_TRUE(drag_loop_callback);

  // PerformDragDrop after the drop loop task runs should fail too because
  // the drag is not finished.
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      14, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);
  EXPECT_EQ("OnPerformDragDropCompleted id=14 success=false action=0",
            SingleChangeToDescription(*setup.changes()));

  // Finish the drop operation, client should be notified with success.
  setup.changes()->clear();
  std::move(drag_loop_callback).Run(ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ("OnPerformDragDropCompleted id=12 success=true action=1",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, CancelDragDrop) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      12, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  // Let the posted drag loop task run.
  base::RunLoop().RunUntilIdle();

  // WindowServiceDelegate should be asked to run the drag loop.
  WindowServiceDelegate::DragDropCompletedCallback drag_loop_callback =
      setup.delegate()->TakeDragLoopCallback();
  ASSERT_TRUE(drag_loop_callback);

  // Cancelling with an invalid id should do nothing.
  EXPECT_FALSE(setup.delegate()->cancel_drag_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelDragDrop(
      kInvalidTransportId);
  EXPECT_TRUE(setup.changes()->empty());
  EXPECT_FALSE(setup.delegate()->cancel_drag_loop_called());

  // Cancel with the real id should notify the delegate.
  EXPECT_FALSE(setup.delegate()->cancel_drag_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelDragDrop(top_level_id);
  EXPECT_TRUE(setup.delegate()->cancel_drag_loop_called());

  // No changes yet because the |drag_loop_callback| has not run.
  EXPECT_TRUE(setup.changes()->empty());

  // Run the closure to simulate drag cancel.
  std::move(drag_loop_callback).Run(ui::DragDropTypes::DRAG_NONE);
  EXPECT_EQ("OnPerformDragDropCompleted id=12 success=false action=0",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, CancelDragDropBeforeDragLoopRun) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformDragDrop(
      12, top_level_id, gfx::Point(),
      base::flat_map<std::string, std::vector<uint8_t>>(), gfx::ImageSkia(),
      gfx::Vector2d(), 0, ::ui::mojom::PointerKind::MOUSE);

  // Cancel the drag before the drag loop task runs.
  EXPECT_FALSE(setup.delegate()->cancel_drag_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelDragDrop(top_level_id);
  EXPECT_TRUE(setup.delegate()->cancel_drag_loop_called());

  // Let the posted drag loop task run.
  base::RunLoop().RunUntilIdle();

  // WindowServiceDelegate should not be notified.
  WindowServiceDelegate::DragDropCompletedCallback drag_loop_callback =
      setup.delegate()->TakeDragLoopCallback();
  EXPECT_FALSE(drag_loop_callback);

  // The request should fail.
  EXPECT_EQ("OnPerformDragDropCompleted id=12 success=false action=0",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest, DsfChanges) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  ServerWindow* top_level_server_window = ServerWindow::GetMayBeNull(top_level);
  const base::Optional<viz::LocalSurfaceId> initial_surface_id =
      top_level_server_window->local_surface_id();
  EXPECT_TRUE(initial_surface_id);

  // Changing the scale factor should change the LocalSurfaceId.
  setup.aura_test_helper()->test_screen()->SetDeviceScaleFactor(2.0f);
  EXPECT_TRUE(top_level_server_window->local_surface_id());
  EXPECT_NE(*top_level_server_window->local_surface_id(), *initial_surface_id);
}

TEST(WindowTreeTest, DontSendGestures) {
  // Create a top-level and a child window.
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();
  aura::Window* child_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(child_window);
  top_level->AddChild(child_window);
  child_window->SetBounds(gfx::Rect(0, 0, 100, 100));
  child_window->Show();

  ui::test::EventGenerator event_generator(setup.root());
  // GestureTapAt() generates a touch down/up, and should not generate a gesture
  // because the Window Service consumes touch events (consuming touch events
  // results in no GestureEvents being generated). Additionally, gestures should
  // never be forwarded to the client, as it's assumed the client runs its own
  // gesture recognizer.
  event_generator.GestureTapAt(gfx::Point(10, 10));
  EXPECT_EQ("ET_TOUCH_PRESSED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_EQ("ET_TOUCH_RELEASED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
}

TEST(WindowTreeTest, DeactivateWindow) {
  // Create two top-levels and focuses (activates) the second.
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
  EXPECT_TRUE(wm::IsActiveWindow(top_level2));

  // Attempting to deactivate |top_level1| should do nothing.
  setup.window_tree_test_helper()->window_tree()->DeactivateWindow(
      setup.window_tree_test_helper()->TransportIdForWindow(top_level1));
  EXPECT_TRUE(wm::IsActiveWindow(top_level2));

  // Similarly, calling Deactivate() with an invalid id should do nothing.
  setup.window_tree_test_helper()->window_tree()->DeactivateWindow(
      kInvalidTransportId);
  EXPECT_TRUE(wm::IsActiveWindow(top_level2));

  // Deactivate() with |top_level2| should activate |top_level1|.
  setup.window_tree_test_helper()->window_tree()->DeactivateWindow(
      setup.window_tree_test_helper()->TransportIdForWindow(top_level2));
  EXPECT_TRUE(wm::IsActiveWindow(top_level1));
}

TEST(WindowTreeTest, AttachFrameSinkId) {
  // Create two top-levels and focuses (activates) the second.
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();

  aura::Window* child_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(child_window);
  viz::FrameSinkId test_frame_sink_id(101, 102);
  viz::HostFrameSinkManager* host_frame_sink_manager =
      child_window->env()->context_factory_private()->GetHostFrameSinkManager();

  // Attach a frame sink to |child_window|. This shouldn't immediately register.
  setup.window_tree_test_helper()->window_tree()->AttachFrameSinkId(
      setup.window_tree_test_helper()->TransportIdForWindow(child_window),
      test_frame_sink_id);
  EXPECT_FALSE(
      host_frame_sink_manager->IsFrameSinkIdRegistered(test_frame_sink_id));

  // Add the window to a parent, which should trigger registering the hierarchy.
  viz::FakeHostFrameSinkClient test_host_frame_sink_client;
  host_frame_sink_manager->RegisterFrameSinkId(
      test_frame_sink_id, &test_host_frame_sink_client,
      viz::ReportFirstSurfaceActivation::kYes);
  EXPECT_EQ(test_frame_sink_id,
            ServerWindow::GetMayBeNull(child_window)->attached_frame_sink_id());
  top_level->AddChild(child_window);
  EXPECT_TRUE(host_frame_sink_manager->IsFrameSinkHierarchyRegistered(
      ServerWindow::GetMayBeNull(top_level)->frame_sink_id(),
      test_frame_sink_id));

  // Removing the window should remove the association.
  top_level->RemoveChild(child_window);
  EXPECT_FALSE(host_frame_sink_manager->IsFrameSinkHierarchyRegistered(
      ServerWindow::GetMayBeNull(top_level)->frame_sink_id(),
      test_frame_sink_id));

  setup.window_tree_test_helper()->DeleteWindow(child_window);

  host_frame_sink_manager->InvalidateFrameSinkId(test_frame_sink_id);
}

TEST(WindowTreeTest, OcclusionStateChange) {
  WindowServiceTestSetup setup;

  // WindowDelegateImpl deletes itself when the window is deleted.
  WindowDelegateImpl* delegate = new WindowDelegateImpl();
  setup.delegate()->set_delegate_for_next_top_level(delegate);

  // Create |top_level1| and tracks its occlusion state.
  aura::Window* top_level1 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  delegate->set_window(top_level1);
  ASSERT_TRUE(top_level1);
  top_level1->SetBounds(gfx::Rect(0, 0, 10, 10));

  top_level1->TrackOcclusionState();

  // Gets HIDDEN state since |top_level1| is created hidden.
  EXPECT_TRUE(ContainsChange(
      *setup.changes(), "OnOcclusionStateChanged window_id=0,1, state=HIDDEN"));

  // Gets VISIBLE state when |top_level1| is shown.
  top_level1->Show();
  EXPECT_TRUE(
      ContainsChange(*setup.changes(),
                     "OnOcclusionStateChanged window_id=0,1, state=VISIBLE"));

  // Creates |top_level2| and make it occlude |top_level1|.
  aura::Window* top_level2 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level2);
  top_level2->SetBounds(gfx::Rect(0, 0, 15, 15));
  top_level2->Show();

  // Gets OCCLUDED state since |top_level2| covers |top_level1|.
  EXPECT_TRUE(
      ContainsChange(*setup.changes(),
                     "OnOcclusionStateChanged window_id=0,1, state=OCCLUDED"));
}

TEST(WindowTreeTest, OcclusionTrackingPause) {
  WindowServiceTestSetup setup;
  aura::test::WindowOcclusionTrackerTestApi tracker_api(
      setup.service()->env()->GetWindowOcclusionTracker());
  ASSERT_FALSE(tracker_api.IsPaused());

  // Simple case of one pause.
  setup.window_tree_test_helper()
      ->window_tree()
      ->PauseWindowOcclusionTracking();
  EXPECT_TRUE(tracker_api.IsPaused());
  setup.window_tree_test_helper()
      ->window_tree()
      ->UnpauseWindowOcclusionTracking();
  EXPECT_FALSE(tracker_api.IsPaused());

  // Multiple pauses.
  constexpr int kPauses = 3;
  for (int i = 0; i < kPauses; ++i) {
    setup.window_tree_test_helper()
        ->window_tree()
        ->PauseWindowOcclusionTracking();
    EXPECT_TRUE(tracker_api.IsPaused());
  }
  for (int i = 0; i < kPauses - 1; ++i) {
    setup.window_tree_test_helper()
        ->window_tree()
        ->UnpauseWindowOcclusionTracking();
    EXPECT_TRUE(tracker_api.IsPaused());
  }
  setup.window_tree_test_helper()
      ->window_tree()
      ->UnpauseWindowOcclusionTracking();
  EXPECT_FALSE(tracker_api.IsPaused());
}

TEST(WindowTreeTest, OcclusionTrackingPauseInterleaved) {
  WindowServiceTestSetup setup;
  aura::test::WindowOcclusionTrackerTestApi tracker_api(
      setup.service()->env()->GetWindowOcclusionTracker());
  ASSERT_FALSE(tracker_api.IsPaused());

  // Creates a second WindowTree.
  TestWindowTreeClient tree_client;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&tree_client);
  tree2->InitFromFactory();
  auto helper2 = std::make_unique<WindowTreeTestHelper>(tree2.get());

  // Tree1 pauses.
  setup.window_tree_test_helper()
      ->window_tree()
      ->PauseWindowOcclusionTracking();
  EXPECT_TRUE(tracker_api.IsPaused());

  // Tree2 pauses.
  helper2->window_tree()->PauseWindowOcclusionTracking();
  EXPECT_TRUE(tracker_api.IsPaused());

  // Tree1 unpauses.
  setup.window_tree_test_helper()
      ->window_tree()
      ->UnpauseWindowOcclusionTracking();
  EXPECT_TRUE(tracker_api.IsPaused());

  // Tree2 unpauses
  helper2->window_tree()->UnpauseWindowOcclusionTracking();
  EXPECT_FALSE(tracker_api.IsPaused());
}

TEST(WindowTreeTest, OcclusionTrackingPauseGoingAwayTree) {
  WindowServiceTestSetup setup;
  aura::test::WindowOcclusionTrackerTestApi tracker_api(
      setup.service()->env()->GetWindowOcclusionTracker());
  ASSERT_FALSE(tracker_api.IsPaused());

  // Tree1 pauses.
  setup.window_tree_test_helper()
      ->window_tree()
      ->PauseWindowOcclusionTracking();
  EXPECT_TRUE(tracker_api.IsPaused());

  // Creates a second WindowTree.
  TestWindowTreeClient tree_client;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&tree_client);
  tree2->InitFromFactory();
  auto helper2 = std::make_unique<WindowTreeTestHelper>(tree2.get());

  // Tree2 creates an outstanding pause.
  helper2->window_tree()->PauseWindowOcclusionTracking();
  EXPECT_TRUE(tracker_api.IsPaused());

  // Tree2 goes away with the outstanding pause.
  helper2.reset();
  tree2.reset();

  // Still paused because tree1 still holds a pause.
  EXPECT_TRUE(tracker_api.IsPaused());

  // Tree1 releases the pause and tracker is unpaused.
  setup.window_tree_test_helper()
      ->window_tree()
      ->UnpauseWindowOcclusionTracking();
  EXPECT_FALSE(tracker_api.IsPaused());
}

}  // namespace
}  // namespace ws

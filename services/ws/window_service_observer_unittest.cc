// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_service_observer.h"

#include <stdint.h>

#include <queue>

#include "services/ws/test_window_tree_client.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ws {
namespace {

class TestWindowServiceObserver : public WindowServiceObserver {
 public:
  TestWindowServiceObserver() = default;
  ~TestWindowServiceObserver() override = default;

  void Reset() {
    last_ack_event_id_ = last_send_event_id_ = 0u;
    last_client_id_ = 0u;
    send_count_ = ack_count_ = 0;
    last_destroyed_client_id_ = 0u;
  }

  uint32_t last_send_event_id() const { return last_send_event_id_; }
  uint32_t last_ack_event_id() const { return last_ack_event_id_; }
  int send_count() const { return send_count_; }
  int ack_count() const { return ack_count_; }
  ClientSpecificId last_client_id() const { return last_client_id_; }
  ClientSpecificId last_destroyed_client_id() const {
    return last_destroyed_client_id_;
  }

  // WindowServiceObserver:
  void OnWillSendEventToClient(ClientSpecificId client_id,
                               uint32_t event_id,
                               const ui::Event& event) override {
    // For the current tests only a single client_id is needed.
    if (send_count_ == 0)
      last_client_id_ = client_id;
    else
      EXPECT_EQ(last_client_id_, client_id);
    last_send_event_id_ = event_id;
    send_count_++;
  }
  void OnClientAckedEvent(ClientSpecificId client_id,
                          uint32_t event_id) override {
    EXPECT_EQ(last_client_id_, client_id);
    last_ack_event_id_ = event_id;
    ack_count_++;
  }
  void OnWillDestroyClient(ClientSpecificId client_id) override {
    last_destroyed_client_id_ = client_id;
  }

 private:
  // Ids supplied to OnWillSendEventToClient() and OnClientAckedEvent().
  uint32_t last_send_event_id_ = 0u;
  uint32_t last_ack_event_id_ = 0u;

  // Number of times OnWillSendEventToClient() was called.
  int send_count_ = 0;

  // Number of times OnClientAckedEvent() was called.
  int ack_count_ = 0;

  // Client id supplied to OnWillSendEventToClient().
  ClientSpecificId last_client_id_ = 0u;

  // Client id supplied to last call to OnWillDestroyClient().
  ClientSpecificId last_destroyed_client_id_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(TestWindowServiceObserver);
};

TEST(WindowServiceObserverTest, EventRelatedFunctions) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  TestWindowServiceObserver window_service_observer;
  setup.service()->AddObserver(&window_service_observer);

  // Move the mouse, but don't ack. This should result in a call to
  // OnWillSendEventToClient().
  ui::test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  EXPECT_EQ(1, window_service_observer.send_count());
  EXPECT_EQ(0, window_service_observer.ack_count());

  // Ack the event, which should call the OnClientAckedEvent().
  EXPECT_TRUE(setup.window_tree_client()->AckFirstEvent(
      setup.window_tree(), mojom::EventResult::UNHANDLED));
  EXPECT_EQ(1, window_service_observer.send_count());
  EXPECT_EQ(1, window_service_observer.ack_count());
  EXPECT_EQ(window_service_observer.last_send_event_id(),
            window_service_observer.last_ack_event_id());

  EXPECT_EQ(setup.window_tree()->client_id(),
            window_service_observer.last_client_id());

  setup.service()->RemoveObserver(&window_service_observer);
}

TEST(WindowServiceObserverTest, OnWillDestroyClient) {
  TestWindowServiceObserver window_service_observer;
  WindowServiceTestSetup setup;
  setup.service()->AddObserver(&window_service_observer);
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();

  // Create a new WindowTree.
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);
  ClientSpecificId embedded_client_id =
      embedding_helper->window_tree->client_id();

  // Deleting the WindowTree should trigger notifying the delegate.
  embedding_helper.reset();
  EXPECT_EQ(embedded_client_id,
            window_service_observer.last_destroyed_client_id());
  setup.service()->RemoveObserver(&window_service_observer);
}

}  // namespace
}  // namespace ws

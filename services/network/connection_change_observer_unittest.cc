// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/connection_change_observer.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/reconnect_event_observer.mojom-shared.h"
#include "services/network/public/mojom/reconnect_event_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Type of notification expected in test.
enum NotificationType {
  // Default value.
  kNone,
  // OnSessionClosed() notification.
  kSessionClosed,
  // OnConnectionFailed() notification,
  kConnectionFailed,
  // OnNetworkChanged() notification.
  kNetworkChanged,
  // OnPipeDisconnected
  kPipeDisconnected,
};
}  // namespace

class TestReconnectEventObserver : public mojom::ReconnectEventObserver {
 public:
  explicit TestReconnectEventObserver() = default;

  TestReconnectEventObserver(const TestReconnectEventObserver&) = delete;
  TestReconnectEventObserver& operator=(const TestReconnectEventObserver&) =
      delete;

  ~TestReconnectEventObserver() override = default;

  void OnSessionClosed() override {
    session_closed_++;
    if (waiting_notification_type_ == kSessionClosed) {
      waiting_notification_type_ = kNone;
      run_loop_->Quit();
    }
  }

  void OnConnectionFailed() override {
    connection_failed_++;
    if (waiting_notification_type_ == kConnectionFailed) {
      waiting_notification_type_ = kNone;
      run_loop_->Quit();
    }
  }

  void OnNetworkEvent(net::NetworkChangeEvent event) override {
    network_event_++;
    last_network_event_ = event;
    if (waiting_notification_type_ == kNetworkChanged) {
      waiting_notification_type_ = kNone;
      run_loop_->Quit();
    }
  }

  void WaitForNotification(NotificationType type) {
    waiting_notification_type_ = type;
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  mojo::PendingRemote<mojom::ReconnectEventObserver> GetPendingRemote() {
    CHECK(!receiver_.is_bound());
    auto remote = receiver_.BindNewPipeAndPassRemote();
    receiver_.set_disconnect_handler(
        base::BindOnce(&TestReconnectEventObserver::OnPipeDisconnected,
                       base::Unretained(this)));
    return remote;
  }

  void OnPipeDisconnected() {
    EXPECT_FALSE(pipe_disconnected_);
    pipe_disconnected_ = true;
    if (waiting_notification_type_ == kPipeDisconnected) {
      waiting_notification_type_ = kNone;
      run_loop_->Quit();
    }
  }

  int session_closed() { return session_closed_; }
  int connection_failed() { return connection_failed_; }
  int network_event() { return network_event_; }
  bool pipe_disconnected() { return pipe_disconnected_; }

  std::optional<net::NetworkChangeEvent> last_network_event() {
    return last_network_event_;
  }

 private:
  int session_closed_ = 0;
  int connection_failed_ = 0;
  int network_event_ = 0;
  bool pipe_disconnected_ = false;

  NotificationType waiting_notification_type_;
  std::optional<net::NetworkChangeEvent> last_network_event_ = std::nullopt;
  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();
  mojo::Receiver<mojom::ReconnectEventObserver> receiver_{this};
};

class ConnectionChangeObserverTest : public testing::Test {
 public:
  ConnectionChangeObserverTest()
      : reconnect_event_observer_(
            std::make_unique<TestReconnectEventObserver>()) {
    auto remote = reconnect_event_observer_->GetPendingRemote();
    connection_change_observer_ =
        std::make_unique<ConnectionChangeObserver>(std::move(remote), nullptr);
  }

  ConnectionChangeObserverTest(const ConnectionChangeObserverTest&) = delete;
  ConnectionChangeObserverTest& operator=(const ConnectionChangeObserverTest&) =
      delete;

  ~ConnectionChangeObserverTest() override = default;

  void OnDestructorEvent(
      const net::ConnectionChangeNotifier::Observer* observer) {
    connection_change_observer_.reset();
  }

  TestReconnectEventObserver* reconnect_event_observer() {
    return reconnect_event_observer_.get();
  }

  ConnectionChangeObserver* connection_change_observer() const {
    return connection_change_observer_.get();
  }

  void RemoveConnectionChangeObserver() { connection_change_observer_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestReconnectEventObserver> reconnect_event_observer_;
  std::unique_ptr<ConnectionChangeObserver> connection_change_observer_;
};

TEST_F(ConnectionChangeObserverTest, OberverNotifiedOnSessionClosed) {
  connection_change_observer()->OnSessionClosed();
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kSessionClosed);
  EXPECT_EQ(1, reconnect_event_observer()->session_closed());
}

TEST_F(ConnectionChangeObserverTest, OberverNotifiedOnConnectionFailed) {
  connection_change_observer()->OnConnectionFailed();
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kConnectionFailed);
  EXPECT_EQ(1, reconnect_event_observer()->connection_failed());
}

TEST_F(ConnectionChangeObserverTest, OberverNotifiedOnNetworkEvent) {
  connection_change_observer()->OnNetworkEvent(
      net::NetworkChangeEvent::kConnected);
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kNetworkChanged);
  EXPECT_EQ(1, reconnect_event_observer()->network_event());
}

TEST_F(ConnectionChangeObserverTest, OberverNotifiedOnNetworkEventTypes) {
  for (int typeInt = 0;
       typeInt != static_cast<int>(net::NetworkChangeEvent::kMaxValue);
       typeInt++) {
    auto event = static_cast<net::NetworkChangeEvent>(typeInt);
    connection_change_observer()->OnNetworkEvent(event);
    reconnect_event_observer()->WaitForNotification(
        NotificationType::kNetworkChanged);
    auto network_event = reconnect_event_observer()->last_network_event();
    ASSERT_TRUE(network_event.has_value());
    EXPECT_EQ(event, network_event.value());
  }
}

TEST_F(ConnectionChangeObserverTest, NotifierDestructed) {
  // Manually remove the observer
  RemoveConnectionChangeObserver();
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kPipeDisconnected);
  EXPECT_TRUE(reconnect_event_observer()->pipe_disconnected());
}

class ConnectionChangeObserverWithNotifierTest
    : public ConnectionChangeObserverTest {
 public:
  ConnectionChangeObserverWithNotifierTest() = default;

  ConnectionChangeObserverWithNotifierTest(
      const ConnectionChangeObserverTest&) = delete;
  ConnectionChangeObserverWithNotifierTest& operator=(
      const ConnectionChangeObserverTest&) = delete;

  ~ConnectionChangeObserverWithNotifierTest() override = default;

  void SetUp() override {
    notifier_->AddObserver(connection_change_observer());
  }

  net::ConnectionChangeNotifier* notifier() { return notifier_.get(); }

 private:
  std::unique_ptr<net::ConnectionChangeNotifier> notifier_ =
      std::make_unique<net::ConnectionChangeNotifier>();
};

TEST_F(ConnectionChangeObserverWithNotifierTest,
       OberverNotifiedOnSessionClosed) {
  notifier()->OnSessionClosed();
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kSessionClosed);
  EXPECT_EQ(1, reconnect_event_observer()->session_closed());
}

TEST_F(ConnectionChangeObserverWithNotifierTest,
       OberverNotifiedOnConnectionFailed) {
  notifier()->OnConnectionFailed();
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kConnectionFailed);
  EXPECT_EQ(1, reconnect_event_observer()->connection_failed());
}

TEST_F(ConnectionChangeObserverWithNotifierTest,
       OberverNotifiedOnNetworkEvent) {
  notifier()->OnNetworkEvent(net::NetworkChangeEvent::kConnected);
  reconnect_event_observer()->WaitForNotification(
      NotificationType::kNetworkChanged);
  EXPECT_EQ(1, reconnect_event_observer()->network_event());
}

TEST_F(ConnectionChangeObserverWithNotifierTest,
       OberverNotifiedOnNetworkEventTypes) {
  for (int typeInt = 0;
       typeInt != static_cast<int>(net::NetworkChangeEvent::kMaxValue);
       typeInt++) {
    auto event = static_cast<net::NetworkChangeEvent>(typeInt);
    notifier()->OnNetworkEvent(event);
    reconnect_event_observer()->WaitForNotification(
        NotificationType::kNetworkChanged);
    auto network_event = reconnect_event_observer()->last_network_event();
    ASSERT_TRUE(network_event.has_value());
    EXPECT_EQ(event, network_event.value());
  }
}

}  // namespace network

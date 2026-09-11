// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr char kTestUserJid[] =
    "remote.user@gmail.com/chromoting_ftl_11111111-2222-3333-4444-555555555555";

class FakeClientSessionControl : public MockClientSessionControl {
 public:
  explicit FakeClientSessionControl(std::string client_jid)
      : client_jid_(std::move(client_jid)) {
    EXPECT_CALL(*this, client_jid())
        .WillRepeatedly(testing::ReturnRef(client_jid_));
  }
  ~FakeClientSessionControl() override = default;

  base::WeakPtr<ClientSessionControl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string client_jid_;
  base::WeakPtrFactory<FakeClientSessionControl> weak_factory_{this};
};

class TestDisconnectWindow : public DisconnectWindowBase {
 public:
  TestDisconnectWindow() = default;
  ~TestDisconnectWindow() override = default;

  bool cooldown_expired_called() const { return cooldown_expired_called_; }

  using DisconnectWindowBase::ResetRepositionAttempts;
  using DisconnectWindowBase::SetExpectedPosition;
  using DisconnectWindowBase::ShouldRepositionOnDisplacement;

 protected:
  void OnCooldownExpired() override { cooldown_expired_called_ = true; }

 private:
  bool cooldown_expired_called_ = false;
};

class DisconnectWindowBaseTest : public testing::Test {
 public:
  DisconnectWindowBaseTest() = default;
  ~DisconnectWindowBaseTest() override = default;

  void SetUp() override {
    DisconnectWindowBase::ResetCurrentAnchorForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(DisconnectWindowBaseTest, InitializationParsesEmailAndFormats) {
  FakeClientSessionControl session_control(kTestUserJid);
  TestDisconnectWindow window;
  window.Start(session_control.GetWeakPtr());

  EXPECT_EQ(window.client_jid(), kTestUserJid);
  EXPECT_EQ(window.email(), "remote.user@gmail.com");
  EXPECT_EQ(window.formatted_email(), u"remote.user@gmail.com");
  EXPECT_EQ(window.current_anchor(),
            DisconnectWindowBase::WindowAnchor::kBottom);
  EXPECT_FALSE(window.is_cooldown_active());
  EXPECT_FALSE(window.expected_x().has_value());
  EXPECT_FALSE(window.expected_y().has_value());
  EXPECT_EQ(window.consecutive_reposition_attempts(), 0);
}

TEST_F(DisconnectWindowBaseTest, DisconnectSessionCallsSessionControl) {
  FakeClientSessionControl session_control(kTestUserJid);
  TestDisconnectWindow window;
  window.Start(session_control.GetWeakPtr());

  EXPECT_CALL(
      session_control,
      DisconnectSession(protocol::ErrorCode::OK,
                        testing::Eq("Custom disconnect reason"), testing::_))
      .Times(1);

  window.DisconnectSession("Custom disconnect reason");
}

TEST_F(DisconnectWindowBaseTest, ToggleAlignmentFlipsAnchorAndCooldown) {
  FakeClientSessionControl session_control(kTestUserJid);
  TestDisconnectWindow window;
  window.Start(session_control.GetWeakPtr());

  EXPECT_EQ(window.current_anchor(),
            DisconnectWindowBase::WindowAnchor::kBottom);
  EXPECT_FALSE(window.is_cooldown_active());

  window.ToggleAlignment();
  EXPECT_EQ(window.current_anchor(), DisconnectWindowBase::WindowAnchor::kTop);
  EXPECT_TRUE(window.is_cooldown_active());
  EXPECT_FALSE(window.cooldown_expired_called());

  // Fast-forward time to expire cooldown.
  task_environment_.FastForwardBy(DisconnectWindowBase::kToggleCooldown);
  EXPECT_FALSE(window.is_cooldown_active());
  EXPECT_TRUE(window.cooldown_expired_called());

  // Toggle back to bottom.
  window.ToggleAlignment();
  EXPECT_EQ(window.current_anchor(),
            DisconnectWindowBase::WindowAnchor::kBottom);
}

TEST_F(DisconnectWindowBaseTest, DisplacementTrackingAndLoopGuard) {
  FakeClientSessionControl session_control(kTestUserJid);
  TestDisconnectWindow window;
  window.Start(session_control.GetWeakPtr());

  EXPECT_FALSE(window.expected_x().has_value());
  EXPECT_FALSE(window.expected_y().has_value());
  EXPECT_FALSE(window.ShouldRepositionOnDisplacement(100, 200));

  window.SetExpectedPosition(100, 200);
  EXPECT_EQ(window.expected_x(), 100);
  EXPECT_EQ(window.expected_y(), 200);

  // Position matching expected coordinates does not trigger reposition.
  EXPECT_FALSE(window.ShouldRepositionOnDisplacement(100, 200));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 0);

  // External displacement triggers reposition and increments attempt count.
  EXPECT_TRUE(window.ShouldRepositionOnDisplacement(150, 250));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 1);

  EXPECT_TRUE(window.ShouldRepositionOnDisplacement(150, 250));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 2);

  EXPECT_TRUE(window.ShouldRepositionOnDisplacement(150, 250));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 3);

  // 4th attempt exceeds kMaxRepositionAttempts (3), loop guard kicks in.
  EXPECT_FALSE(window.ShouldRepositionOnDisplacement(150, 250));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 3);

  // Re-matching expected coordinates resets counter.
  EXPECT_FALSE(window.ShouldRepositionOnDisplacement(100, 200));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 0);

  // Manual reset also resets counter.
  EXPECT_TRUE(window.ShouldRepositionOnDisplacement(150, 250));
  EXPECT_EQ(window.consecutive_reposition_attempts(), 1);
  window.ResetRepositionAttempts();
  EXPECT_EQ(window.consecutive_reposition_attempts(), 0);

  // Negative coordinates on multi-monitor setups are tracked correctly.
  window.SetExpectedPosition(-1, -1);
  EXPECT_EQ(window.expected_x(), -1);
  EXPECT_EQ(window.expected_y(), -1);
  EXPECT_FALSE(window.ShouldRepositionOnDisplacement(-1, -1));
  EXPECT_TRUE(window.ShouldRepositionOnDisplacement(0, 0));
}

}  // namespace

}  // namespace remoting

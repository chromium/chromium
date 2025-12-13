// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/corp_signaling_connector.h"

#include <memory>

#include "base/test/task_environment.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

constexpr char kLocalJid[] = "local_user@domain.com/chromoting_abc123";

}  // namespace

class CorpSignalingConnectorTest : public testing::Test {
 public:
  CorpSignalingConnectorTest() {
    signal_strategy_.SimulateTwoStageConnect();
    signal_strategy_.Disconnect();
    signaling_connector_ =
        std::make_unique<CorpSignalingConnector>(&signal_strategy_);
  }

  ~CorpSignalingConnectorTest() override {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 protected:
  const net::BackoffEntry& GetBackoff() {
    return signaling_connector_->backoff_;
  }

  const base::OneShotTimer& GetBackoffResetTimer() {
    return signaling_connector_->backoff_reset_timer_;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeSignalStrategy signal_strategy_{SignalingAddress(kLocalJid)};
  std::unique_ptr<CorpSignalingConnector> signaling_connector_;
};

TEST_F(CorpSignalingConnectorTest, StartAndSucceed) {
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTING);
  signal_strategy_.ProceedConnect();
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTED);
  ASSERT_TRUE(GetBackoffResetTimer().IsRunning());
}

TEST_F(CorpSignalingConnectorTest, StartAndFailedThenRetryAndSucceeded) {
  ASSERT_EQ(GetBackoff().failure_count(), 0);
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTING);

  signal_strategy_.SetError(SignalStrategy::NETWORK_ERROR);
  signal_strategy_.Disconnect();
  ASSERT_EQ(GetBackoff().failure_count(), 1);

  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTING);
  signal_strategy_.ProceedConnect();

  // Failure count is not reset immediately.
  ASSERT_EQ(GetBackoff().failure_count(), 1);

  // Failure count is eventually reset to 0.
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(GetBackoff().failure_count(), 0);
}

TEST_F(CorpSignalingConnectorTest,
       StartAndImmediatelyDisconnected_RetryWithBackoff) {
  ASSERT_EQ(GetBackoff().failure_count(), 0);
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTING);

  signal_strategy_.ProceedConnect();
  ASSERT_EQ(GetBackoff().failure_count(), 0);

  signal_strategy_.Disconnect();
  ASSERT_EQ(GetBackoff().failure_count(), 1);

  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTING);
  signal_strategy_.ProceedConnect();

  // Failure count is not reset immediately.
  ASSERT_EQ(GetBackoff().failure_count(), 1);

  // Failure count is eventually reset to 0.
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(GetBackoff().failure_count(), 0);
}

TEST_F(CorpSignalingConnectorTest, AutoConnectOnNetworkChange) {
  signaling_connector_->OnNetworkChanged(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  // Reconnection starts with some delay.
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::DISCONNECTED);
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(signal_strategy_.GetState(), SignalStrategy::CONNECTING);
}

}  // namespace remoting

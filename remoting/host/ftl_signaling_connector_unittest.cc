// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_signaling_connector.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using testing::_;

constexpr char kLocalFtlId[] = "local_user@domain.com/chromoting_ftl_abc123";

}  // namespace

class FtlSignalingConnectorTest : public testing::Test {
 public:
  FtlSignalingConnectorTest() {
    signal_strategy_.SimulateTwoStageConnect();
    signal_strategy_.Disconnect();
    signaling_connector_ = std::make_unique<FtlSignalingConnector>(
        &signal_strategy_, auth_failed_callback_.Get());
  }

  ~FtlSignalingConnectorTest() override {
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
  FakeSignalStrategy signal_strategy_{SignalingAddress(kLocalFtlId)};
  base::MockCallback<base::OnceClosure> auth_failed_callback_;
  std::unique_ptr<FtlSignalingConnector> signaling_connector_;
};

TEST_F(FtlSignalingConnectorTest, StartAndSucceed) {
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());
  signal_strategy_.ProceedConnect();
  ASSERT_EQ(SignalStrategy::CONNECTED, signal_strategy_.GetState());
  ASSERT_TRUE(GetBackoffResetTimer().IsRunning());
}

TEST_F(FtlSignalingConnectorTest, StartAndAuthFailed) {
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());

  signal_strategy_.SetIsSignInError(true);
  signal_strategy_.SetError(SignalStrategy::AUTHENTICATION_FAILED);

  EXPECT_CALL(auth_failed_callback_, Run()).Times(1);
  signal_strategy_.Disconnect();
}

TEST_F(FtlSignalingConnectorTest, StartAndFailedThenRetryAndSucceeded) {
  ASSERT_EQ(0, GetBackoff().failure_count());
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());

  signal_strategy_.SetError(SignalStrategy::NETWORK_ERROR);
  signal_strategy_.Disconnect();
  ASSERT_EQ(1, GetBackoff().failure_count());

  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());
  signal_strategy_.ProceedConnect();

  // Failure count is not reset immediately.
  ASSERT_EQ(1, GetBackoff().failure_count());

  // Failure count is eventually reset to 0.
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(FtlSignalingConnectorTest,
       StartAndImmediatelyDisconnected_RetryWithBackoff) {
  ASSERT_EQ(0, GetBackoff().failure_count());
  signaling_connector_->Start();
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());

  signal_strategy_.ProceedConnect();
  ASSERT_EQ(0, GetBackoff().failure_count());

  signal_strategy_.Disconnect();
  ASSERT_EQ(1, GetBackoff().failure_count());

  task_environment_.FastForwardBy(GetBackoff().GetTimeUntilRelease());
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());
  signal_strategy_.ProceedConnect();

  // Failure count is not reset immediately.
  ASSERT_EQ(1, GetBackoff().failure_count());

  // Failure count is eventually reset to 0.
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(0, GetBackoff().failure_count());
}

TEST_F(FtlSignalingConnectorTest, AutoConnectOnNetworkChange) {
  signaling_connector_->OnNetworkChanged(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  // Reconnection starts with some delay.
  ASSERT_EQ(SignalStrategy::DISCONNECTED, signal_strategy_.GetState());
  task_environment_.FastForwardUntilNoTasksRemain();
  ASSERT_EQ(SignalStrategy::CONNECTING, signal_strategy_.GetState());
}

}  // namespace remoting

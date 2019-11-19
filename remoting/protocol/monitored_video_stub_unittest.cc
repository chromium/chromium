// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/monitored_video_stub.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/timer/timer.h"
#include "remoting/protocol/protocol_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtMost;
using ::testing::InvokeWithoutArgs;

namespace remoting {
namespace protocol {

static const int64_t kTestOverrideDelayMilliseconds = 1;

class MonitoredVideoStubTest : public testing::Test {
 protected:
  void SetUp() override {
    packet_.reset(new VideoPacket());
    monitor_.reset(new MonitoredVideoStub(
        &video_stub_,
        base::TimeDelta::FromMilliseconds(kTestOverrideDelayMilliseconds),
        base::Bind(
            &MonitoredVideoStubTest::OnVideoChannelStatus,
            base::Unretained(this))));
    EXPECT_CALL(video_stub_, ProcessVideoPacketPtr(_, _)).Times(AnyNumber());
  }

  MOCK_METHOD1(OnVideoChannelStatus, void(bool connected));

  base::test::SingleThreadTaskEnvironment task_environment_;
  MockVideoStub video_stub_;

  std::unique_ptr<MonitoredVideoStub> monitor_;
  std::unique_ptr<VideoPacket> packet_;
  base::OneShotTimer timer_end_test_;
};

TEST_F(MonitoredVideoStubTest, OnChannelConnected) {
  EXPECT_CALL(*this, OnVideoChannelStatus(true));
  // On slow machines, the connectivity check timer may fire before the test
  // finishes, so we expect to see at most one transition to not ready.
  EXPECT_CALL(*this, OnVideoChannelStatus(false)).Times(AtMost(1));

  monitor_->ProcessVideoPacket(std::move(packet_), base::Closure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MonitoredVideoStubTest, OnChannelDisconnected) {
  EXPECT_CALL(*this, OnVideoChannelStatus(true));
  monitor_->ProcessVideoPacket(std::move(packet_), base::Closure());

  base::RunLoop run_loop;
  EXPECT_CALL(*this, OnVideoChannelStatus(false))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
  run_loop.Run();
}

TEST_F(MonitoredVideoStubTest, OnChannelStayConnected) {
  // Verify no extra connected events are fired when packets are received
  // frequently
  EXPECT_CALL(*this, OnVideoChannelStatus(true));
  // On slow machines, the connectivity check timer may fire before the test
  // finishes, so we expect to see at most one transition to not ready.
  EXPECT_CALL(*this, OnVideoChannelStatus(false)).Times(AtMost(1));

  monitor_->ProcessVideoPacket(std::move(packet_), base::Closure());
  monitor_->ProcessVideoPacket(std::move(packet_), base::Closure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MonitoredVideoStubTest, OnChannelStayDisconnected) {
  // Verify no extra disconnected events are fired.
  EXPECT_CALL(*this, OnVideoChannelStatus(true)).Times(1);
  EXPECT_CALL(*this, OnVideoChannelStatus(false)).Times(1);

  monitor_->ProcessVideoPacket(std::move(packet_), base::Closure());

  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
      // The delay should be much greater than |kTestOverrideDelayMilliseconds|.
      TestTimeouts::tiny_timeout());
  base::RunLoop().Run();
}

}  // namespace protocol
}  // namespace remoting

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_power_save_blocker.h"

#include <memory>

#include "base/message_loop/message_pump_type.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "remoting/host/host_status_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class HostPowerSaveBlockerTest : public testing::Test {
 public:
  HostPowerSaveBlockerTest();

 protected:
  bool is_activated() const;

  void SetUp() override;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  base::Thread blocking_thread_;
  scoped_refptr<HostStatusMonitor> monitor_;
  std::unique_ptr<HostPowerSaveBlocker> blocker_;
};

HostPowerSaveBlockerTest::HostPowerSaveBlockerTest()
    : blocking_thread_("block-thread"), monitor_(new HostStatusMonitor()) {}

void HostPowerSaveBlockerTest::SetUp() {
  ASSERT_TRUE(blocking_thread_.StartWithOptions(
                  base::Thread::Options(base::MessagePumpType::IO, 0)) &&
              blocking_thread_.WaitUntilThreadStarted());
  blocker_ = std::make_unique<HostPowerSaveBlocker>(
      monitor_, task_environment_.GetMainThreadTaskRunner(),
      blocking_thread_.task_runner());
}

bool HostPowerSaveBlockerTest::is_activated() const {
  return !!blocker_->blocker_;
}

TEST_F(HostPowerSaveBlockerTest, Activated) {
  blocker_->OnClientConnected("jid/jid1@jid2.org");
  ASSERT_TRUE(is_activated());
  blocker_->OnClientDisconnected("jid/jid3@jid4.org");
  ASSERT_FALSE(is_activated());
}

}  // namespace remoting

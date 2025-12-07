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

#if BUILDFLAG(IS_LINUX)
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace remoting {

class HostPowerSaveBlockerTest : public testing::Test {
 public:
  HostPowerSaveBlockerTest();

 protected:
  bool is_activated() const;

  void SetUp() override;

  void TearDown() override;

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<HostStatusMonitor> monitor_;
  std::unique_ptr<HostPowerSaveBlocker> blocker_;
};

HostPowerSaveBlockerTest::HostPowerSaveBlockerTest()
    : monitor_(new HostStatusMonitor()) {}

void HostPowerSaveBlockerTest::SetUp() {
  blocker_ = std::make_unique<HostPowerSaveBlocker>(
      monitor_, task_environment_.GetMainThreadTaskRunner());
}

void HostPowerSaveBlockerTest::TearDown() {
  blocker_.reset();
  task_environment_.RunUntilIdle();
#if BUILDFLAG(IS_LINUX)
  dbus_thread_linux::ShutdownOnDBusThreadAndBlock();
#endif  // BUILDFLAG(IS_LINUX)
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

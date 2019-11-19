// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/idleness_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class IdlenessDetectorTest : public PageTestBase {
 protected:
  void SetUp() override {
    EnablePlatform();
    auto task_runner = platform()->test_task_runner();
    platform_time_ = task_runner->NowTicks();
    DCHECK(!platform_time_.is_null());
    PageTestBase::SetUp();
  }

  IdlenessDetector* Detector() { return GetFrame().GetIdlenessDetector(); }

  bool IsNetworkQuietTimerActive() {
    return Detector()->network_quiet_timer_.IsActive();
  }

  bool HadNetworkQuiet() {
    return !Detector()->in_network_2_quiet_period_ &&
           !Detector()->in_network_0_quiet_period_;
  }

  void WillProcessTask(base::TimeTicks start_time) {
    DCHECK(start_time >= platform_time_);
    platform()->AdvanceClock(start_time - platform_time_);
    platform_time_ = start_time;
    Detector()->WillProcessTask(start_time);
  }

  void DidProcessTask(base::TimeTicks start_time, base::TimeTicks end_time) {
    DCHECK(start_time < end_time);
    platform()->AdvanceClock(end_time - start_time);
    platform_time_ = end_time;
    Detector()->DidProcessTask(start_time, end_time);
  }

  static base::TimeTicks SecondsToTimeTicks(double seconds) {
    return base::TimeTicks() + base::TimeDelta::FromSecondsD(seconds);
  }

 private:
  base::TimeTicks platform_time_;
};

TEST_F(IdlenessDetectorTest, NetworkQuietBasic) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(SecondsToTimeTicks(1));
  DidProcessTask(SecondsToTimeTicks(1), SecondsToTimeTicks(1.01));

  WillProcessTask(SecondsToTimeTicks(1.52));
  EXPECT_TRUE(HadNetworkQuiet());
  DidProcessTask(SecondsToTimeTicks(1.52), SecondsToTimeTicks(1.53));
}

TEST_F(IdlenessDetectorTest, NetworkQuietWithLongTask) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(SecondsToTimeTicks(1));
  DidProcessTask(SecondsToTimeTicks(1), SecondsToTimeTicks(1.01));

  WillProcessTask(SecondsToTimeTicks(1.02));
  DidProcessTask(SecondsToTimeTicks(1.02), SecondsToTimeTicks(1.6));
  EXPECT_FALSE(HadNetworkQuiet());

  WillProcessTask(SecondsToTimeTicks(2.11));
  EXPECT_TRUE(HadNetworkQuiet());
  DidProcessTask(SecondsToTimeTicks(2.11), SecondsToTimeTicks(2.12));
}

TEST_F(IdlenessDetectorTest, NetworkQuietWatchdogTimerFired) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(SecondsToTimeTicks(1));
  DidProcessTask(SecondsToTimeTicks(1), SecondsToTimeTicks(1.01));

  platform()->RunForPeriodSeconds(3);
  EXPECT_FALSE(IsNetworkQuietTimerActive());
  EXPECT_TRUE(HadNetworkQuiet());
}

}  // namespace blink

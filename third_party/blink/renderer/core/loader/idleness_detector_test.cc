// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/idleness_detector.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class IdlenessDetectorTest : public PageTestBase {
 protected:
  IdlenessDetectorTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    EnablePlatform();
    platform_time_ = platform()->NowTicks();
    initial_time_ = platform_time_;
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
    AdvanceClock(start_time - platform_time_);
    platform_time_ = start_time;
    Detector()->WillProcessTask(start_time);
  }

  void DidProcessTask(base::TimeTicks start_time, base::TimeTicks end_time) {
    DCHECK(start_time < end_time);
    AdvanceClock(end_time - start_time);
    platform_time_ = end_time;
    Detector()->DidProcessTask(start_time, end_time);
  }

  base::TimeTicks SecondsToTimeTicks(double seconds) {
    return initial_time_ + base::Seconds(seconds);
  }

 private:
  base::TimeTicks initial_time_;
  base::TimeTicks platform_time_;
};

TEST_F(IdlenessDetectorTest, NetworkQuietBasic) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(SecondsToTimeTicks(0));
  DidProcessTask(SecondsToTimeTicks(0), SecondsToTimeTicks(0.01));

  WillProcessTask(SecondsToTimeTicks(0.52));
  EXPECT_TRUE(HadNetworkQuiet());
  DidProcessTask(SecondsToTimeTicks(0.52), SecondsToTimeTicks(0.53));
}

TEST_F(IdlenessDetectorTest, NetworkQuietWithLongTask) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(SecondsToTimeTicks(0));
  DidProcessTask(SecondsToTimeTicks(0), SecondsToTimeTicks(0.01));

  WillProcessTask(SecondsToTimeTicks(0.02));
  DidProcessTask(SecondsToTimeTicks(0.02), SecondsToTimeTicks(0.6));
  EXPECT_FALSE(HadNetworkQuiet());

  WillProcessTask(SecondsToTimeTicks(1.11));
  EXPECT_TRUE(HadNetworkQuiet());
  DidProcessTask(SecondsToTimeTicks(1.11), SecondsToTimeTicks(1.12));
}

TEST_F(IdlenessDetectorTest, NetworkQuietWatchdogTimerFired) {
  EXPECT_TRUE(IsNetworkQuietTimerActive());

  WillProcessTask(SecondsToTimeTicks(0));
  DidProcessTask(SecondsToTimeTicks(0), SecondsToTimeTicks(0.01));

  FastForwardBy(base::Seconds(2));
  EXPECT_FALSE(IsNetworkQuietTimerActive());
  EXPECT_TRUE(HadNetworkQuiet());
}

}  // namespace blink

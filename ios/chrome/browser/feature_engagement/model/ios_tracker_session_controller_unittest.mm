// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/ios_tracker_session_controller.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/simple_test_clock.h"
#import "base/time/time.h"
#import "components/feature_engagement/test/scoped_iph_feature_list.h"
#import "testing/platform_test.h"

namespace {
const base::TimeDelta kMaxSessionDuration = base::Minutes(60);
}

class IOSTrackerSessionControllerTest : public PlatformTest {
 public:
  IOSTrackerSessionControllerTest();
  ~IOSTrackerSessionControllerTest() override;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::SimpleTestClock> clock_;
  std::unique_ptr<IOSTrackerSessionController> ios_tracker_session_controller_;
};

IOSTrackerSessionControllerTest::IOSTrackerSessionControllerTest() {
  clock_ = std::make_unique<base::SimpleTestClock>();
  clock_->SetNow(base::Time::Now());
  ios_tracker_session_controller_ =
      std::make_unique<IOSTrackerSessionController>(clock_.get());
}

IOSTrackerSessionControllerTest::~IOSTrackerSessionControllerTest() {}

// Tests that the session is not reset after init.
TEST_F(IOSTrackerSessionControllerTest, TestShouldNotResetAfterInit) {
  EXPECT_FALSE(ios_tracker_session_controller_->ShouldResetSession());
}

// Tests that the session is not reset before max session duration has passed.
TEST_F(IOSTrackerSessionControllerTest, TestShouldNotResetWithinMaxDuration) {
  clock_->Advance(kMaxSessionDuration - base::Seconds(1));
  EXPECT_FALSE(ios_tracker_session_controller_->ShouldResetSession());
}

// Tests that the session is reset after max session duration has passed.
TEST_F(IOSTrackerSessionControllerTest, TestShouldResetAfterMaxDuration) {
  clock_->Advance(kMaxSessionDuration + base::Seconds(1));
  EXPECT_TRUE(ios_tracker_session_controller_->ShouldResetSession());
}

// Tests that the session is reset once as the time passes.
TEST_F(IOSTrackerSessionControllerTest, TestShouldResetSessionOnce) {
  clock_->Advance(kMaxSessionDuration + base::Seconds(1));
  EXPECT_TRUE(ios_tracker_session_controller_->ShouldResetSession());
  clock_->Advance(kMaxSessionDuration - base::Seconds(1));
  EXPECT_FALSE(ios_tracker_session_controller_->ShouldResetSession());
}

// Tests that the session is twice as the time passes.
TEST_F(IOSTrackerSessionControllerTest, TestShouldResetSessionTwice) {
  clock_->Advance(kMaxSessionDuration + base::Seconds(1));
  EXPECT_TRUE(ios_tracker_session_controller_->ShouldResetSession());
  clock_->Advance(kMaxSessionDuration + base::Seconds(1));
  EXPECT_TRUE(ios_tracker_session_controller_->ShouldResetSession());
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/intelligence/actor/tools/model/observation_delay_controller.h"

#import <memory>
#import <utility>

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {

class ObservationDelayControllerTest : public PlatformTest {
 protected:
  ObservationDelayControllerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeatureWithParameters(
        kActorTools, {{"PageStabilityEnabled", "true"}});
  }

  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = std::make_unique<ObservationDelayController>(
        ActorTaskId{}, /*journal=*/nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ObservationDelayController> controller_;
};

TEST_F(ObservationDelayControllerTest, DefaultTimeout) {
  base::test::TestFuture<ObservationDelayController::Result> future;

  controller_->Wait(/*web_frame=*/nullptr, future.GetCallback());
  // Fast forward past the default timeout to trigger the timeout.
  task_environment_.FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
}

TEST_F(ObservationDelayControllerTest, ConfiguredTimeout) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kActorTools,
      {{"ObservationDelayTimeout", "5s"}, {"PageStabilityEnabled", "true"}});
  base::test::TestFuture<ObservationDelayController::Result> future;

  controller_->Wait(/*web_frame=*/nullptr, future.GetCallback());
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), ObservationDelayController::Result::kOk);
}

}  // namespace actor

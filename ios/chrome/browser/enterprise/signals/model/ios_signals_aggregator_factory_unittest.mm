// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/enterprise/signals/model/ios_signals_aggregator_factory.h"

#import <memory>

#import "base/task/single_thread_task_runner.h"
#import "base/test/task_environment.h"
#import "components/device_signals/core/browser/signals_aggregator.h"
#import "components/device_signals/core/browser/signals_types.h"
#import "components/policy/core/common/cloud/cloud_external_data_manager.h"
#import "components/policy/core/common/cloud/cloud_policy_constants.h"
#import "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class IOSSignalsAggregatorFactoryTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    auto store = std::make_unique<policy::MockUserCloudPolicyStore>(
        policy::dm_protocol::GetChromeUserPolicyType());
    auto cloud_policy_manager =
        std::make_unique<policy::UserCloudPolicyManager>(
            std::move(store), /*extension_install_store=*/nullptr,
            base::FilePath(),
            /*cloud_external_data_manager=*/nullptr,
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            network::TestNetworkConnectionTracker::CreateGetter());

    TestProfileIOS::Builder builder;
    builder.SetUserCloudPolicyManager(std::move(cloud_policy_manager));
    profile_ = std::move(builder).Build();
  }
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that `GetForProfile` returns a non-null SignalsAggregator instance
// for a given profile.
TEST_F(IOSSignalsAggregatorFactoryTest, GetForProfile) {
  device_signals::SignalsAggregator* aggregator =
      IOSSignalsAggregatorFactory::GetForProfile(profile_.get());
  // Verify aggregator is created.
  ASSERT_TRUE(aggregator);
}

// Tests that `GetForProfile` returns the same SignalsAggregator instance
// when called multiple times with the same profile.
TEST_F(IOSSignalsAggregatorFactoryTest, GetForProfile_SameInstance) {
  device_signals::SignalsAggregator* aggregator1 =
      IOSSignalsAggregatorFactory::GetForProfile(profile_.get());
  device_signals::SignalsAggregator* aggregator2 =
      IOSSignalsAggregatorFactory::GetForProfile(profile_.get());
  // Verify same instance is returned for same profile.
  EXPECT_EQ(aggregator1, aggregator2);
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/resource_broker/resource_broker_host.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/public/common/content_features.h"
#include "content/services/resource_broker/public/mojom/resource_broker.mojom.h"
#include "content/services/resource_broker/resource_broker_service_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class ResourceBrokerHostTest : public testing::Test {
 public:
  ResourceBrokerHostTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kResourceBroker);
  }

  void SetUp() override {
    ResourceBrokerHost::GetInstance().SetServiceLauncherForTesting(
        base::BindRepeating(&ResourceBrokerHostTest::LaunchService,
                            base::Unretained(this)));
  }

  void TearDown() override {
    ResourceBrokerHost::ResetForTesting();
    service_instance_.reset();
  }

  void LaunchService(
      mojo::PendingReceiver<resource_broker::mojom::ResourceBrokerService>
          receiver) {
    service_instance_ =
        std::make_unique<resource_broker::ResourceBrokerServiceImpl>(
            std::move(receiver));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<resource_broker::ResourceBrokerServiceImpl> service_instance_;
};

TEST_F(ResourceBrokerHostTest, CreateConfig) {
  base::UnguessableToken token = base::UnguessableToken::Create();
  auto config = ResourceBrokerHost::CreateConfig(token);
  EXPECT_EQ(config->session_nonce, token);

  EXPECT_CHECK_DEATH(
      ResourceBrokerHost::CreateConfig(base::UnguessableToken()));
}

TEST_F(ResourceBrokerHostTest, CrashCooldownBlocksRelaunch) {
  auto& host = ResourceBrokerHost::GetInstance();
  EXPECT_TRUE(host.GetService());
  EXPECT_TRUE(host.IsServiceRunningForTesting());

  // Disconnect by destroying the service instance and flushing the pipe.
  service_instance_.reset();
  host.MaybeFlushForTesting();
  EXPECT_FALSE(host.IsServiceRunningForTesting());

  // Within 10s cooldown, GetService() returns nullptr without relaunching.
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(host.GetService());

  // After 10s cooldown, GetService() relaunches successfully.
  task_environment_.FastForwardBy(base::Seconds(6));
  EXPECT_TRUE(host.GetService());
  EXPECT_TRUE(host.IsServiceRunningForTesting());
}

TEST_F(ResourceBrokerHostTest, CrashLoopDisablesService) {
  auto& host = ResourceBrokerHost::GetInstance();
  base::HistogramTester histogram_tester;

  // 1st crash.
  EXPECT_TRUE(host.GetService());
  service_instance_.reset();
  host.MaybeFlushForTesting();
  task_environment_.FastForwardBy(base::Seconds(11));

  // 2nd crash.
  EXPECT_TRUE(host.GetService());
  service_instance_.reset();
  host.MaybeFlushForTesting();
  task_environment_.FastForwardBy(base::Seconds(11));

  // 3rd crash within 10 minutes.
  EXPECT_TRUE(host.GetService());
  service_instance_.reset();
  host.MaybeFlushForTesting();

  // The service must now be permanently disabled for the session.
  EXPECT_FALSE(host.GetService());

  // Even after cooldown elapses, it remains disabled.
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_FALSE(host.GetService());

  histogram_tester.ExpectBucketCount(
      "ResourceBroker.LifecycleEvent",
      ResourceBrokerHost::ResourceBrokerLifecycleEvent::kDisabledDueToCrashLoop,
      1);
}

TEST_F(ResourceBrokerHostTest, CrashWindowPruning) {
  auto& host = ResourceBrokerHost::GetInstance();
  base::HistogramTester histogram_tester;

  // 1st crash.
  EXPECT_TRUE(host.GetService());
  service_instance_.reset();
  host.MaybeFlushForTesting();

  // Advance 11 minutes (past 10-minute window).
  task_environment_.FastForwardBy(base::Minutes(11));

  // 2nd crash.
  EXPECT_TRUE(host.GetService());
  service_instance_.reset();
  host.MaybeFlushForTesting();

  // Advance another 11 minutes.
  task_environment_.FastForwardBy(base::Minutes(11));

  // 3rd crash.
  EXPECT_TRUE(host.GetService());
  service_instance_.reset();
  host.MaybeFlushForTesting();

  // Crashes were spread out (>10 mins apart), so service is NOT disabled for
  // session.
  histogram_tester.ExpectBucketCount(
      "ResourceBroker.LifecycleEvent",
      ResourceBrokerHost::ResourceBrokerLifecycleEvent::kDisabledDueToCrashLoop,
      0);

  // Cooldown still applies for 10s after the 3rd crash.
  EXPECT_FALSE(host.GetService());

  // After 10s cooldown, the service can be relaunched successfully.
  task_environment_.FastForwardBy(base::Seconds(11));
  EXPECT_TRUE(host.GetService());
  EXPECT_TRUE(host.IsServiceRunningForTesting());
}

TEST_F(ResourceBrokerHostTest, SessionNonceStability) {
  auto& host = ResourceBrokerHost::GetInstance();
  base::UnguessableToken nonce_before = host.GetSessionNonce();
  EXPECT_FALSE(nonce_before.is_empty());

  EXPECT_TRUE(host.GetService());
  host.MaybeFlushForTesting();
  ASSERT_TRUE(service_instance_);
  ASSERT_TRUE(service_instance_->config_for_testing());
  EXPECT_EQ(service_instance_->config_for_testing()->session_nonce,
            nonce_before);

  service_instance_.reset();
  host.MaybeFlushForTesting();
  EXPECT_EQ(nonce_before, host.GetSessionNonce());

  task_environment_.FastForwardBy(base::Seconds(11));
  EXPECT_TRUE(host.GetService());
  host.MaybeFlushForTesting();
  EXPECT_EQ(nonce_before, host.GetSessionNonce());
  ASSERT_TRUE(service_instance_);
  ASSERT_TRUE(service_instance_->config_for_testing());
  EXPECT_EQ(service_instance_->config_for_testing()->session_nonce,
            nonce_before);
}

TEST_F(ResourceBrokerHostTest, DisabledFeatureReturnsNull) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(features::kResourceBroker);

  EXPECT_FALSE(ResourceBrokerHost::GetInstance().GetService());
}

TEST_F(ResourceBrokerHostTest, InitialLaunchRecordsTelemetry) {
  auto& host = ResourceBrokerHost::GetInstance();
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(host.GetService());
  histogram_tester.ExpectUniqueSample(
      "ResourceBroker.LifecycleEvent",
      ResourceBrokerHost::ResourceBrokerLifecycleEvent::kLaunched, 1);

  // Subsequent GetService() calls while running must not re-record.
  EXPECT_TRUE(host.GetService());
  histogram_tester.ExpectUniqueSample(
      "ResourceBroker.LifecycleEvent",
      ResourceBrokerHost::ResourceBrokerLifecycleEvent::kLaunched, 1);

  // Relaunching after a disconnect must also not re-record kLaunched.
  service_instance_.reset();
  host.MaybeFlushForTesting();
  task_environment_.FastForwardBy(base::Seconds(11));
  EXPECT_TRUE(host.GetService());
  histogram_tester.ExpectUniqueSample(
      "ResourceBroker.LifecycleEvent",
      ResourceBrokerHost::ResourceBrokerLifecycleEvent::kLaunched, 1);
}

}  // namespace

}  // namespace content

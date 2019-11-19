// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/browser/notifications/notification_trigger_constants.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;

namespace content {

namespace {

// Fake Service Worker registration id to use in tests requiring one.
const int64_t kFakeServiceWorkerRegistrationId = 42;

class NotificationBrowserClient : public TestContentBrowserClient {
 public:
  explicit NotificationBrowserClient(BrowserContext* browser_context)
      : platform_notification_service_(
            std::make_unique<MockPlatformNotificationService>(
                browser_context)) {}

  PlatformNotificationService* GetPlatformNotificationService(
      BrowserContext* browser_context) override {
    return platform_notification_service_.get();
  }

 private:
  std::unique_ptr<PlatformNotificationService> platform_notification_service_;
};

}  // namespace

class PlatformNotificationContextTriggerTest : public ::testing::Test {
 public:
  PlatformNotificationContextTriggerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        notification_browser_client_(&browser_context_),
        success_(false) {
    SetBrowserClientForTesting(&notification_browser_client_);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kNotificationTriggers);
    platform_notification_context_ =
        base::MakeRefCounted<PlatformNotificationContextImpl>(
            base::FilePath(), &browser_context_, nullptr);
    platform_notification_context_->SetTaskRunnerForTesting(
        base::ThreadTaskRunnerHandle::Get());
    platform_notification_context_->Initialize();
    base::RunLoop().RunUntilIdle();
  }

  // Callback to provide when writing a notification to the database.
  void DidWriteNotificationData(bool success,
                                const std::string& notification_id) {
    success_ = success;
  }

 protected:
  void WriteNotificationData(const std::string& tag,
                             base::Optional<base::Time> timestamp) {
    ASSERT_TRUE(
        TryWriteNotificationData("https://example.com", tag, timestamp));
  }

  bool TryWriteNotificationData(const std::string& url,
                                const std::string& tag,
                                base::Optional<base::Time> timestamp) {
    GURL origin(url);
    NotificationDatabaseData notification_database_data;
    notification_database_data.origin = origin;
    notification_database_data.service_worker_registration_id =
        kFakeServiceWorkerRegistrationId;
    notification_database_data.notification_data.show_trigger_timestamp =
        timestamp;
    notification_database_data.notification_data.tag = tag;

    platform_notification_context_->WriteNotificationData(
        next_persistent_notification_id(), kFakeServiceWorkerRegistrationId,
        origin, notification_database_data,
        base::BindOnce(
            &PlatformNotificationContextTriggerTest::DidWriteNotificationData,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    return success_;
  }

  // Gets the currently displayed notifications from
  // |notification_browser_client_| synchronously.
  std::set<std::string> GetDisplayedNotifications() {
    std::set<std::string> displayed_notification_ids;
    base::RunLoop run_loop;
    notification_browser_client_
        .GetPlatformNotificationService(&browser_context_)
        ->GetDisplayedNotifications(base::BindLambdaForTesting(
            [&](std::set<std::string> notification_ids, bool supports_sync) {
              displayed_notification_ids = std::move(notification_ids);
              run_loop.Quit();
            }));
    run_loop.Run();
    return displayed_notification_ids;
  }

  void TriggerNotifications() {
    platform_notification_context_->TriggerNotifications();
    base::RunLoop().RunUntilIdle();
  }

  BrowserTaskEnvironment task_environment_;  // Must be first member

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestBrowserContext browser_context_;
  NotificationBrowserClient notification_browser_client_;
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;

  // Returns the next persistent notification id for tests.
  int64_t next_persistent_notification_id() {
    return next_persistent_notification_id_++;
  }

  bool success_;
  int64_t next_persistent_notification_id_ = 1;
};

TEST_F(PlatformNotificationContextTriggerTest, TriggerInFuture) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  // Wait until the trigger timestamp is reached.
  task_environment_.FastForwardBy(TimeDelta::FromSeconds(10));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest, TriggerInPast) {
  // Trigger timestamp in the past should immediately trigger.
  WriteNotificationData("1", Time::Now() - TimeDelta::FromSeconds(10));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       OverwriteExistingTriggerInFuture) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  task_environment_.FastForwardBy(TimeDelta::FromSeconds(5));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  // Overwrites the scheduled notifications with a new trigger timestamp.
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));

  task_environment_.FastForwardBy(TimeDelta::FromSeconds(5));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  task_environment_.FastForwardBy(TimeDelta::FromSeconds(5));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest, OverwriteExistingTriggerToPast) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  task_environment_.FastForwardBy(TimeDelta::FromSeconds(5));

  // Overwrites the scheduled notifications with a new trigger timestamp.
  WriteNotificationData("1", Time::Now() - TimeDelta::FromSeconds(10));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       OverwriteDisplayedNotificationToPast) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  task_environment_.FastForwardBy(TimeDelta::FromSeconds(10));

  // Overwrites a displayed notification with a trigger timestamp in the past.
  WriteNotificationData("1", Time::Now() - TimeDelta::FromSeconds(10));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       OverwriteDisplayedNotificationToFuture) {
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));
  task_environment_.FastForwardBy(TimeDelta::FromSeconds(10));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  // Overwrites a displayed notification which hides it until the trigger
  // timestamp is reached.
  WriteNotificationData("1", Time::Now() + TimeDelta::FromSeconds(10));

  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  task_environment_.FastForwardBy(TimeDelta::FromSeconds(10));

  // This gets called by the notification scheduling system.
  TriggerNotifications();

  ASSERT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(PlatformNotificationContextTriggerTest,
       LimitsNumberOfScheduledNotificationsPerOrigin) {
  for (int i = 1; i <= kMaximumScheduledNotificationsPerOrigin; ++i) {
    WriteNotificationData(std::to_string(i),
                          Time::Now() + TimeDelta::FromSeconds(i));
  }

  ASSERT_FALSE(TryWriteNotificationData(
      "https://example.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      Time::Now() +
          TimeDelta::FromSeconds(kMaximumScheduledNotificationsPerOrigin + 1)));

  ASSERT_TRUE(TryWriteNotificationData(
      "https://example2.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      Time::Now() +
          TimeDelta::FromSeconds(kMaximumScheduledNotificationsPerOrigin + 1)));
}

TEST_F(PlatformNotificationContextTriggerTest, EnforcesLimitOnUpdate) {
  for (int i = 1; i <= kMaximumScheduledNotificationsPerOrigin; ++i) {
    WriteNotificationData(std::to_string(i),
                          Time::Now() + TimeDelta::FromSeconds(i));
  }

  ASSERT_TRUE(TryWriteNotificationData(
      "https://example.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      base::nullopt));

  ASSERT_FALSE(TryWriteNotificationData(
      "https://example.com",
      std::to_string(kMaximumScheduledNotificationsPerOrigin + 1),
      Time::Now() +
          TimeDelta::FromSeconds(kMaximumScheduledNotificationsPerOrigin + 1)));
}

TEST_F(PlatformNotificationContextTriggerTest, RecordDisplayDelay) {
  base::HistogramTester histogram_tester;
  base::TimeDelta trigger_delay = TimeDelta::FromSeconds(10);
  base::TimeDelta display_delay = TimeDelta::FromSeconds(8);

  WriteNotificationData("1", Time::Now() + trigger_delay);
  ASSERT_EQ(0u, GetDisplayedNotifications().size());

  // Forward time until after the expected trigger time.
  task_environment_.FastForwardBy(trigger_delay + display_delay);

  // Trigger notification |display_delay| after it should have been displayed.
  TriggerNotifications();

  histogram_tester.ExpectUniqueSample("Notifications.Triggers.DisplayDelay",
                                      display_delay.InMilliseconds(), 1);
}

}  // namespace content

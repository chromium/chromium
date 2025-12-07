// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_mediator.h"

#import <optional>

#import "base/json/values_util.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

// Test fixture for ReminderNotificationsMediator.
class ReminderNotificationsMediatorTest : public PlatformTest {
 public:
  ReminderNotificationsMediatorTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();
    // Register the preference used by the mediator.
    pref_service_.registry()->RegisterDictionaryPref(
        prefs::kReminderNotifications);
    // Create the mediator instance.
    mediator_ = [[ReminderNotificationsMediator alloc]
        initWithProfilePrefService:&pref_service_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    PlatformTest::TearDown();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  ReminderNotificationsMediator* mediator_;
};

// Tests that setting a reminder for a valid URL stores the correct data in
// PrefService.
TEST_F(ReminderNotificationsMediatorTest, SetReminderValidURL) {
  GURL test_url("https://www.example.com");
  base::Time reminder_time = base::Time::Now() + base::Days(1);
  base::Time creation_time = base::Time::Now();

  [mediator_ setReminderForURL:test_url time:reminder_time];

  const base::Value::Dict& reminders =
      pref_service_.GetDict(prefs::kReminderNotifications);
  ASSERT_EQ(reminders.size(), 1u);

  const base::Value::Dict* reminder_details =
      reminders.FindDict(test_url.spec());
  ASSERT_TRUE(reminder_details);

  std::optional<base::Time> stored_reminder_time =
      base::ValueToTime(reminder_details->Find(kReminderNotificationsTimeKey));
  ASSERT_TRUE(stored_reminder_time.has_value());
  EXPECT_EQ(reminder_time, stored_reminder_time.value());

  std::optional<base::Time> stored_creation_time = base::ValueToTime(
      reminder_details->Find(kReminderNotificationsCreationTimeKey));
  ASSERT_TRUE(stored_creation_time.has_value());
  EXPECT_EQ(creation_time, stored_creation_time.value());
}

// Tests that setting a reminder for an invalid URL does not store anything.
TEST_F(ReminderNotificationsMediatorTest, SetReminderInvalidURL) {
  GURL invalid_url;  // Default constructor creates an invalid URL.
  ASSERT_FALSE(invalid_url.is_valid());
  base::Time reminder_time = base::Time::Now() + base::Days(1);

  [mediator_ setReminderForURL:invalid_url time:reminder_time];

  const base::Value::Dict& reminders =
      pref_service_.GetDict(prefs::kReminderNotifications);
  EXPECT_TRUE(reminders.empty());
}

// Tests that setting a reminder for an empty URL string does not store
// anything.
TEST_F(ReminderNotificationsMediatorTest, SetReminderEmptyURL) {
  GURL empty_url("");
  ASSERT_FALSE(empty_url.is_valid());  // An empty URL spec is invalid.
  base::Time reminder_time = base::Time::Now() + base::Days(1);

  [mediator_ setReminderForURL:empty_url time:reminder_time];

  const base::Value::Dict& reminders =
      pref_service_.GetDict(prefs::kReminderNotifications);
  EXPECT_TRUE(reminders.empty());
}

// Tests that setting a reminder for the same URL twice overwrites the
// previous entry.
TEST_F(ReminderNotificationsMediatorTest, SetReminderOverwrite) {
  GURL test_url("https://www.example.com");
  base::Time first_reminder_time = base::Time::Now() + base::Days(1);

  // Set the first reminder.
  [mediator_ setReminderForURL:test_url time:first_reminder_time];
  base::Time first_creation_time = base::Time::Now();

  // Advance time a bit before setting the second reminder.
  task_environment_.FastForwardBy(base::Seconds(5));
  base::Time second_reminder_time = base::Time::Now() + base::Days(2);
  base::Time second_creation_time = base::Time::Now();

  // Set the second reminder.
  [mediator_ setReminderForURL:test_url time:second_reminder_time];

  const base::Value::Dict& reminders =
      pref_service_.GetDict(prefs::kReminderNotifications);
  ASSERT_EQ(reminders.size(), 1u);

  const base::Value::Dict* reminder_details =
      reminders.FindDict(test_url.spec());
  ASSERT_TRUE(reminder_details);

  // Check that the reminder time is the second one.
  std::optional<base::Time> stored_reminder_time =
      base::ValueToTime(reminder_details->Find(kReminderNotificationsTimeKey));
  ASSERT_TRUE(stored_reminder_time.has_value());
  EXPECT_EQ(second_reminder_time, stored_reminder_time.value());

  // Check that the creation time is the second one.
  std::optional<base::Time> stored_creation_time = base::ValueToTime(
      reminder_details->Find(kReminderNotificationsCreationTimeKey));
  ASSERT_TRUE(stored_creation_time.has_value());
  EXPECT_EQ(second_creation_time, stored_creation_time.value());
  EXPECT_NE(first_creation_time, stored_creation_time.value());
}

// Tests that setting reminders for multiple different URLs stores all of them.
TEST_F(ReminderNotificationsMediatorTest, SetMultipleReminders) {
  GURL url1("https://www.example.com/page1");
  GURL url2("https://www.example.com/page2");
  base::Time time1 = base::Time::Now() + base::Hours(1);
  base::Time time2 = base::Time::Now() + base::Hours(2);

  [mediator_ setReminderForURL:url1 time:time1];
  base::Time creation_time1 = base::Time::Now();
  task_environment_.FastForwardBy(
      base::Seconds(1));  // Ensure different creation times
  [mediator_ setReminderForURL:url2 time:time2];
  base::Time creation_time2 = base::Time::Now();

  const base::Value::Dict& reminders =
      pref_service_.GetDict(prefs::kReminderNotifications);
  ASSERT_EQ(reminders.size(), 2u);

  // Check first reminder
  const base::Value::Dict* details1 = reminders.FindDict(url1.spec());
  ASSERT_TRUE(details1);
  std::optional<base::Time> stored_time1 =
      base::ValueToTime(details1->Find(kReminderNotificationsTimeKey));
  ASSERT_TRUE(stored_time1.has_value());
  EXPECT_EQ(time1, stored_time1.value());
  std::optional<base::Time> stored_creation_time1 =
      base::ValueToTime(details1->Find(kReminderNotificationsCreationTimeKey));
  ASSERT_TRUE(stored_creation_time1.has_value());
  EXPECT_EQ(creation_time1, stored_creation_time1.value());

  // Check second reminder
  const base::Value::Dict* details2 = reminders.FindDict(url2.spec());
  ASSERT_TRUE(details2);
  std::optional<base::Time> stored_time2 =
      base::ValueToTime(details2->Find(kReminderNotificationsTimeKey));
  ASSERT_TRUE(stored_time2.has_value());
  EXPECT_EQ(time2, stored_time2.value());
  std::optional<base::Time> stored_creation_time2 =
      base::ValueToTime(details2->Find(kReminderNotificationsCreationTimeKey));
  ASSERT_TRUE(stored_creation_time2.has_value());
  EXPECT_EQ(creation_time2, stored_creation_time2.value());
}

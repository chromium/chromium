// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notification_service_impl.h"

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Bogus class to act as a NotificationSource for the messages.
class TestSource {};

class TestObserver : public NotificationObserver {
 public:
  TestObserver() = default;

  int notification_count() const { return notification_count_; }

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override {
    ++notification_count_;
  }

 private:
  int notification_count_ = 0;
};

const int kNotification1 = 1;
const int kNotification2 = 2;

}  // namespace


class NotificationServiceImplTest : public testing::Test {
 protected:
  NotificationRegistrar registrar_;
};

TEST_F(NotificationServiceImplTest, Basic) {
  TestSource test_source;
  TestSource other_source;

  // Check the equality operators defined for NotificationSource
  EXPECT_TRUE(Source<TestSource>(&test_source) ==
              Source<TestSource>(&test_source));
  EXPECT_TRUE(Source<TestSource>(&test_source) !=
              Source<TestSource>(&other_source));

  TestObserver idle_all_sources;
  TestObserver idle_test_source;

  // Make sure it doesn't freak out when there are no observers.
  NotificationService* service = NotificationService::current();
  service->Notify(kNotification1,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  registrar_.Add(&idle_all_sources, kNotification1,
                 NotificationService::AllSources());
  registrar_.Add(&idle_test_source, kNotification1,
                 Source<TestSource>(&test_source));

  EXPECT_EQ(0, idle_all_sources.notification_count());
  EXPECT_EQ(0, idle_test_source.notification_count());

  service->Notify(kNotification1,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(kNotification2,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(kNotification1,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(kNotification2,
                  Source<TestSource>(&other_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Try send with NULL source.
  service->Notify(kNotification1,
                  NotificationService::AllSources(),
                  NotificationService::NoDetails());

  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  registrar_.RemoveAll();

  service->Notify(kNotification1,
                  Source<TestSource>(&test_source),
                  NotificationService::NoDetails());

  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());
}

TEST_F(NotificationServiceImplTest, MultipleRegistration) {
  TestSource test_source;
  TestObserver idle_test_source;

  NotificationService* service = NotificationService::current();

  registrar_.Add(&idle_test_source, kNotification1,
                 Source<TestSource>(&test_source));
  registrar_.Add(&idle_test_source, kNotification2,
                 Source<TestSource>(&test_source));

  service->Notify(kNotification1, Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(1, idle_test_source.notification_count());

  registrar_.Remove(&idle_test_source, kNotification1,
                    Source<TestSource>(&test_source));

  service->Notify(kNotification1, Source<TestSource>(&test_source),
                  NotificationService::NoDetails());
  EXPECT_EQ(1, idle_test_source.notification_count());

  registrar_.RemoveAll();
}

}  // namespace content

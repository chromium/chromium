// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing BreadcrumbManager class.
class BreadcrumbManagerTest : public PlatformTest {
 protected:
  BreadcrumbManagerTest() {}

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  BreadcrumbManager breadcrumb_manager_;
};

// Tests that an event is logged and returned.
TEST_F(BreadcrumbManagerTest, AddEvent) {
  std::string event_message = "event";
  breadcrumb_manager_.AddEvent(event_message);
  std::list<std::string> events = breadcrumb_manager_.GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  // Events returned from |GetEvents| will have a timestamp prepended.
  EXPECT_NE(std::string::npos, events.front().find(event_message));
}

// Tests that returned events returned by |GetEvents| are limited by the
// |event_count_limit| parameter.
TEST_F(BreadcrumbManagerTest, EventCountLimited) {
  breadcrumb_manager_.AddEvent("event1");
  breadcrumb_manager_.AddEvent("event2");
  breadcrumb_manager_.AddEvent("event3");
  breadcrumb_manager_.AddEvent("event4");

  std::list<std::string> events = breadcrumb_manager_.GetEvents(2);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("event3"));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("event4"));
}

// Tests that old events are dropped.
TEST_F(BreadcrumbManagerTest, OldEventsDropped) {
  // Log an event from one and two hours ago.
  breadcrumb_manager_.AddEvent("event1");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_.AddEvent("event2");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));

  // Log three events separated by three minutes to ensure they receive their
  // own event bucket. Otherwise, some old events may be returned to ensure a
  // minimum number of available events. See |MinimumEventsReturned| test below.
  breadcrumb_manager_.AddEvent("event3");
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(3));
  breadcrumb_manager_.AddEvent("event4");
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(3));
  breadcrumb_manager_.AddEvent("event5");

  std::list<std::string> events = breadcrumb_manager_.GetEvents(0);
  ASSERT_EQ(3ul, events.size());
  // Validate the three most recent events are the ones which were returned.
  EXPECT_NE(std::string::npos, events.front().find("event3"));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("event4"));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("event5"));
}

// Tests that expired events are returned if not enough new events exist.
TEST_F(BreadcrumbManagerTest, MinimumEventsReturned) {
  // Log an event from one and two hours ago.
  breadcrumb_manager_.AddEvent("event1");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_.AddEvent("event2");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_.AddEvent("event3");

  std::list<std::string> events = breadcrumb_manager_.GetEvents(0);
  EXPECT_EQ(2ul, events.size());
}

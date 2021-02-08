// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer_bridge.h"

#import "base/strings/sys_string_conversions.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test BreadcrumbManagerObserverBridge class.
class BreadcrumbManagerObserverBridgeTest : public PlatformTest {
 protected:
  BreadcrumbManagerObserverBridgeTest() {
    mock_observer_ = OCMProtocolMock(@protocol(BreadcrumbManagerObserving));
    observer_bridge_ = std::make_unique<BreadcrumbManagerObserverBridge>(
        &breadcrumb_manager_, mock_observer_);
  }

  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  breadcrumbs::BreadcrumbManager breadcrumb_manager_;
  id mock_observer_;
  std::unique_ptr<BreadcrumbManagerObserverBridge> observer_bridge_;
};

// Tests |breadcrumbManager:didAddEvent:| forwarding.
TEST_F(BreadcrumbManagerObserverBridgeTest, EventAdded) {
  std::string event("sample event");

  id event_parameter_validator = [OCMArg checkWithBlock:^BOOL(id value) {
    // The manager will prepended a timestamp to the event so verify that the
    // end matches |event|.
    return [value isKindOfClass:[NSString class]] &&
           [value hasSuffix:base::SysUTF8ToNSString(event)];
  }];
  OCMExpect([mock_observer_ breadcrumbManager:&breadcrumb_manager_
                                  didAddEvent:event_parameter_validator]);

  breadcrumb_manager_.AddEvent(event);

  [mock_observer_ verify];
}

// Tests |breadcrumbManagerDidRemoveOldEvents:| forwarding.
TEST_F(BreadcrumbManagerObserverBridgeTest, OldEventsRemoved) {
  OCMExpect([mock_observer_ breadcrumbManager:&breadcrumb_manager_
                                  didAddEvent:OCMOCK_ANY]);
  OCMExpect([mock_observer_
      breadcrumbManagerDidRemoveOldEvents:&breadcrumb_manager_]);

  // Oldest event will be dropped becuase it is old enough and there are two
  // newer buckets.
  breadcrumb_manager_.AddEvent("event1");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_.AddEvent("event2");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_.AddEvent("event3");

  [mock_observer_ verify];
}

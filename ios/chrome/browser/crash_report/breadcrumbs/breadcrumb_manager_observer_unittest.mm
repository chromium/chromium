// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"

#import "base/macros.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
class FakeBreadcrumbManagerObserver : public BreadcrumbManagerObserver {
 public:
  FakeBreadcrumbManagerObserver() {}
  ~FakeBreadcrumbManagerObserver() override = default;

  // BreadcrumbManagerObserver
  void EventAdded(BreadcrumbManager* manager,
                  const std::string& event) override {
    last_received_manager_ = manager;
    last_received_event_ = event;
  }

  BreadcrumbManager* last_received_manager_ = nullptr;
  std::string last_received_event_;

  DISALLOW_COPY_AND_ASSIGN(FakeBreadcrumbManagerObserver);
};
}

class BreadcrumbManagerObserverTest : public PlatformTest {
 protected:
  BreadcrumbManagerObserverTest() { manager_.AddObserver(&observer_); }
  ~BreadcrumbManagerObserverTest() override {
    manager_.RemoveObserver(&observer_);
  }

  BreadcrumbManager manager_;
  FakeBreadcrumbManagerObserver observer_;
};

// Tests that |BreadcrumbManagerObserver::EventAdded| is called when an event to
// added to |manager_|.
TEST_F(BreadcrumbManagerObserverTest, EventAdded) {
  ASSERT_FALSE(observer_.last_received_manager_);
  ASSERT_TRUE(observer_.last_received_event_.empty());

  std::string event = "event";
  manager_.AddEvent(event);

  EXPECT_EQ(&manager_, observer_.last_received_manager_);
  // A timestamp will be prepended to the event passed to |AddEvent|.
  EXPECT_NE(std::string::npos, observer_.last_received_event_.find(event));
}

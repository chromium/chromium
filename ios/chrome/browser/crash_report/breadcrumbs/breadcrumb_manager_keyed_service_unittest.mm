// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"

#include "base/time/time.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Creates a new BreadcrumbManagerKeyedService for |browser_state|.
std::unique_ptr<KeyedService> BuildBreadcrumbManagerKeyedService(
    web::BrowserState* browser_state) {
  return std::make_unique<breadcrumbs::BreadcrumbManagerKeyedService>(
      browser_state->IsOffTheRecord());
}
}

// Test fixture for testing BreadcrumbManagerKeyedService class.
class BreadcrumbManagerKeyedServiceTest : public PlatformTest {
 protected:
  BreadcrumbManagerKeyedServiceTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        BreadcrumbManagerKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildBreadcrumbManagerKeyedService));
    chrome_browser_state_ = test_cbs_builder.Build();

    breadcrumb_manager_service_ =
        static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
            BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
  }

  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_manager_service_;
};

// Tests that events logged to Normal and OffTheRecord BrowserStates are
// seperately identifiable.
TEST_F(BreadcrumbManagerKeyedServiceTest, EventsLabeledWithBrowserState) {
  breadcrumb_manager_service_->AddEvent("event");
  std::string event = breadcrumb_manager_service_->GetEvents(0).front();

  ChromeBrowserState* off_the_record_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();

  breadcrumbs::BreadcrumbManagerKeyedService* otr_breadcrumb_manager_service =
      static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
          BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
              off_the_record_browser_state));
  otr_breadcrumb_manager_service->AddEvent("event");

  std::string off_the_record_event =
      otr_breadcrumb_manager_service->GetEvents(0).front();
  // Event should indicate it was logged from an off the record "Incognito"
  // browser state.
  EXPECT_NE(std::string::npos, off_the_record_event.find(" I "));

  EXPECT_STRNE(event.c_str(), off_the_record_event.c_str());
}

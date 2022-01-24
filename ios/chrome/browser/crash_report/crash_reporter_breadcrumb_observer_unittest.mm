// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/task_environment.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"
#import "ios/testing/scoped_block_swizzler.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns an OCMArg validator which checks that the parameter value is a string
// containing |count| occurances of |substring|.
id StringParameterValidatorWithCountOfSubstring(NSUInteger count,
                                                NSString* substring) {
  return [OCMArg checkWithBlock:^(id value) {
    if (![value isKindOfClass:[NSString class]]) {
      return NO;
    }
    NSError* error = nil;
    NSRegularExpression* regex = [NSRegularExpression
        regularExpressionWithPattern:substring
                             options:NSRegularExpressionCaseInsensitive
                               error:&error];
    if (error) {
      return NO;
    }
    return count == [regex
                        numberOfMatchesInString:value
                                        options:0
                                          range:NSMakeRange(0, [value length])];
  }];
}
}  // namespace

// Tests that CrashReporterBreadcrumbObserver attaches observed breadcrumb
// events to crash reports.
class CrashReporterBreadcrumbObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    TestChromeBrowserState::Builder test_cbs_builder_2;
    chrome_browser_state_2_ = test_cbs_builder_2.Build();

    mock_breakpad_controller_ =
        [OCMockObject mockForClass:[BreakpadController class]];

    // Swizzle +[BreakpadController sharedInstance] to return
    // |mock_breakpad_controller_| instead of the normal singleton instance.
    id implementation_block = ^BreakpadController*(id self) {
      return mock_breakpad_controller_;
    };
    breakpad_controller_shared_instance_swizzler_.reset(new ScopedBlockSwizzler(
        [BreakpadController class], @selector(sharedInstance),
        implementation_block));
  }

  void TearDown() override {
    [[mock_breakpad_controller_ stub] stop];
    crash_helper::SetEnabled(false);
    PlatformTest::TearDown();
  }

 protected:
  // Returns the BreadcrumbManagerKeyedService for |browser_state|, and sets
  // |crash_reporter_breadcrumb_observer_| as its observer.
  breadcrumbs::BreadcrumbManagerKeyedService* GetAndObserveBreadcrumbService(
      web::BrowserState* const browser_state) {
    breadcrumbs::BreadcrumbManagerKeyedService* const breadcrumb_service =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(browser_state);
    crash_reporter_breadcrumb_observer_.ObserveBreadcrumbManagerService(
        breadcrumb_service);
    return breadcrumb_service;
  }

  id mock_breakpad_controller_;
  std::unique_ptr<ScopedBlockSwizzler>
      breakpad_controller_shared_instance_swizzler_;

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_2_;

  // Must be destroyed before browser states to ensure that it stops observing
  // the browser states' BreadcrumbManagers before they are destroyed.
  breadcrumbs::CrashReporterBreadcrumbObserver
      crash_reporter_breadcrumb_observer_;
};

// Tests that breadcrumb events logged to a single BreadcrumbManagerKeyedService
// are collected by the CrashReporterBreadcrumbObserver and attached to crash
// reports.
TEST_F(CrashReporterBreadcrumbObserverTest, EventsAttachedToCrashReport) {
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      GetAndObserveBreadcrumbService(chrome_browser_state_.get());

  id breadcrumbs_param_validation_block = [OCMArg checkWithBlock:^(id value) {
    if (![value isKindOfClass:[NSString class]]) {
      return NO;
    }
    const std::list<std::string> events = breadcrumb_service->GetEvents(0);
    std::string expected_breadcrumbs;
    for (const auto& event : events) {
      expected_breadcrumbs += event + "\n";
    }
    return
        [value isEqualToString:base::SysUTF8ToNSString(expected_breadcrumbs)];
  }];
  [[mock_breakpad_controller_ expect]
      addUploadParameter:breadcrumbs_param_validation_block
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];

  breadcrumb_service->AddEvent("Breadcrumb Event");
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

// Tests that breadcrumbs string is cut when it exceeds the max allowed length.
TEST_F(CrashReporterBreadcrumbObserverTest, ProductDataOverflow) {
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      GetAndObserveBreadcrumbService(chrome_browser_state_.get());

  // Build a sample breadcrumbs string greater than the maximum allowed size.
  NSMutableString* breadcrumbs = [[NSMutableString alloc] init];
  while (breadcrumbs.length < breadcrumbs::kMaxDataLength) {
    [breadcrumbs appendString:@"12:01 Fake Breadcrumb Event/n"];
  }
  [breadcrumbs appendString:@"12:01 Fake Breadcrumb Event/n"];
  ASSERT_GT([breadcrumbs length], breadcrumbs::kMaxDataLength);

  id validation_block = [OCMArg checkWithBlock:^(id value) {
    EXPECT_EQ(breadcrumbs::kMaxDataLength, [value length]);
    return YES;
  }];
  [[mock_breakpad_controller_ expect]
      addUploadParameter:validation_block
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  breadcrumb_service->AddEvent(base::SysNSStringToUTF8(breadcrumbs));
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

// Tests that breadcrumb events logged to multiple BreadcrumbManagerKeyedService
// instances are collected by the CrashReporterBreadcrumbObserver and attached
// to crash reports.
TEST_F(CrashReporterBreadcrumbObserverTest,
       MultipleBrowserStatesAttachedToCrashReport) {
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);

  const std::string event = "Breadcrumb Event";
  NSString* event_nsstring = base::SysUTF8ToNSString(event);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      GetAndObserveBreadcrumbService(chrome_browser_state_.get());

  [[mock_breakpad_controller_ expect]
      addUploadParameter:StringParameterValidatorWithCountOfSubstring(
                             1, event_nsstring)
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  breadcrumb_service->AddEvent(event);

  breadcrumbs::BreadcrumbManagerKeyedService* otr_breadcrumb_service =
      GetAndObserveBreadcrumbService(
          chrome_browser_state_->GetOffTheRecordChromeBrowserState());

  [[mock_breakpad_controller_ expect]
      addUploadParameter:StringParameterValidatorWithCountOfSubstring(
                             2, event_nsstring)
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  otr_breadcrumb_service->AddEvent(event);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_2 =
      GetAndObserveBreadcrumbService(chrome_browser_state_2_.get());

  [[mock_breakpad_controller_ expect]
      addUploadParameter:StringParameterValidatorWithCountOfSubstring(
                             3, event_nsstring)
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  breadcrumb_service_2->AddEvent(event);

  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

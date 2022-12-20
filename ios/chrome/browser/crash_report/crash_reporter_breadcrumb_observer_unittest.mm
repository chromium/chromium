// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/breadcrumbs/core/crash_reporter_breadcrumb_observer.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/breadcrumbs/core/breadcrumb_manager.h"
#import "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#import "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/test/ocmock/OCMockObject+BreakpadControllerTesting.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/breakpad/breakpad/src/client/ios/BreakpadController.h"
#import "third_party/crashpad/crashpad/client/annotation_list.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns an OCMArg validator which checks that the parameter value is a string
// containing `count` occurances of `substring`.
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

    // Ensure the CrashReporterBreadcrumbObserver singleton is created
    // and registered.
    breadcrumbs::CrashReporterBreadcrumbObserver::GetInstance();

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    TestChromeBrowserState::Builder test_cbs_builder_2;
    chrome_browser_state_2_ = test_cbs_builder_2.Build();

    mock_breakpad_controller_ =
        [OCMockObject mockForClass:[BreakpadController class]];

    // Swizzle +[BreakpadController sharedInstance] to return
    // `mock_breakpad_controller_` instead of the normal singleton instance.
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

  std::string BreadcrumbAnnotations() {
    auto* annotations = crashpad::AnnotationList::Get();
    if (!annotations || annotations->begin() == annotations->end()) {
      EXPECT_TRUE(false);
      return "";
    }

    for (const crashpad::Annotation* annotation : *annotations) {
      if (!annotation->is_set())
        continue;

      if (annotation->type() != crashpad::Annotation::Type::kString)
        continue;

      const std::string kBreadcrumbs("breadcrumbs");
      if (annotation->name() != kBreadcrumbs)
        continue;

      base::StringPiece cp_value(static_cast<const char*>(annotation->value()),
                                 annotation->size());
      return std::string(cp_value);
    }
    EXPECT_TRUE(false);
    return "";
  }

 protected:
  id mock_breakpad_controller_;
  std::unique_ptr<ScopedBlockSwizzler>
      breakpad_controller_shared_instance_swizzler_;

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_2_;
};

// Tests that breadcrumb events logged to a single BreadcrumbManagerKeyedService
// are collected by the CrashReporterBreadcrumbObserver and attached to crash
// reports.
TEST_F(CrashReporterBreadcrumbObserverTest, EventsAttachedToCrashReport) {
  if (crash_helper::common::CanUseCrashpad()) {
    breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    breadcrumb_service->AddEvent("Breadcrumb Event");
    const auto& events =
        breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
    std::string expected_breadcrumbs;
    for (const auto& event : events) {
      expected_breadcrumbs += event + "\n";
    }
    EXPECT_EQ(BreadcrumbAnnotations(), expected_breadcrumbs);
    return;
  }

  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);

  id breadcrumbs_param_validation_block = [OCMArg checkWithBlock:^(id value) {
    if (![value isKindOfClass:[NSString class]]) {
      return NO;
    }
    const auto& events =
        breadcrumbs::BreadcrumbManager::GetInstance().GetEvents();
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

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  breadcrumb_service->AddEvent("Breadcrumb Event");
  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

// Tests that breadcrumbs string is cut when it exceeds the max allowed length.
TEST_F(CrashReporterBreadcrumbObserverTest, ProductDataOverflow) {
  [[mock_breakpad_controller_ expect] start:NO];
  crash_helper::SetEnabled(true);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());

  // Build a sample breadcrumbs string greater than the maximum allowed size.
  NSMutableString* breadcrumbs = [[NSMutableString alloc] init];
  while (breadcrumbs.length < breadcrumbs::kMaxDataLength) {
    [breadcrumbs appendString:@"12:01 Fake Breadcrumb Event/n"];
  }
  [breadcrumbs appendString:@"12:01 Fake Breadcrumb Event/n"];
  ASSERT_GT([breadcrumbs length], breadcrumbs::kMaxDataLength);

  if (crash_helper::common::CanUseCrashpad()) {
    breadcrumb_service->AddEvent(base::SysNSStringToUTF8(breadcrumbs));
    EXPECT_EQ(BreadcrumbAnnotations().size(), breadcrumbs::kMaxDataLength);
    return;
  }

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
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());

  [[mock_breakpad_controller_ expect]
      addUploadParameter:StringParameterValidatorWithCountOfSubstring(
                             1, event_nsstring)
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  breadcrumb_service->AddEvent(event);

  breadcrumbs::BreadcrumbManagerKeyedService* otr_breadcrumb_service =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_->GetOffTheRecordChromeBrowserState());

  [[mock_breakpad_controller_ expect]
      addUploadParameter:StringParameterValidatorWithCountOfSubstring(
                             2, event_nsstring)
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  otr_breadcrumb_service->AddEvent(event);

  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_service_2 =
      BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
          chrome_browser_state_2_.get());

  [[mock_breakpad_controller_ expect]
      addUploadParameter:StringParameterValidatorWithCountOfSubstring(
                             3, event_nsstring)
                  forKey:base::SysUTF8ToNSString(
                             breadcrumbs::kBreadcrumbsProductDataKey)];
  breadcrumb_service_2->AddEvent(event);

  if (crash_helper::common::CanUseCrashpad()) {
    std::string breadcrumbs = BreadcrumbAnnotations();
    // 1 incognito
    EXPECT_NE(breadcrumbs.find("I Breadcrumb Event"), std::string::npos);
    // 3 total
    auto iter = breadcrumbs.find(event);
    int count = 0;
    while (iter != std::string::npos) {
      count++;
      iter = breadcrumbs.find(event, iter + 1);
    }
    EXPECT_EQ(count, 3);
    return;
  }

  EXPECT_OCMOCK_VERIFY(mock_breakpad_controller_);
}

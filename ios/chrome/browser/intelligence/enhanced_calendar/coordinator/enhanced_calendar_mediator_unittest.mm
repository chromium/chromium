// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/constants/error_strings.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/optimization_guide/mojom/enhanced_calendar_service.mojom.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// A fake delegate to capture calls from the mediator and verify behavior.
@interface FakeEnhancedCalendarMediatorDelegate
    : NSObject <EnhancedCalendarMediatorDelegate>
@property(nonatomic, assign) BOOL cancelCalled;
@property(nonatomic, assign) BOOL presentCalled;
@property(nonatomic, strong) EnhancedCalendarConfiguration* lastConfig;
@end

@implementation FakeEnhancedCalendarMediatorDelegate
- (void)cancelRequestsAndDismissViewController:
    (EnhancedCalendarMediator*)mediator {
  _cancelCalled = YES;
}
- (void)presentAddToCalendar:(EnhancedCalendarMediator*)mediator
                      config:(EnhancedCalendarConfiguration*)config {
  _presentCalled = YES;
  _lastConfig = config;
}
@end

// Expose private methods for testing.
@interface EnhancedCalendarMediator (Testing)
- (void)handleEnhancedCalendarResponseResult:
    (ai::mojom::EnhancedCalendarResponseResultPtr)responseResult;
- (void)updateConfigWithEnhancedCalendarResponse:
    (optimization_guide::proto::EnhancedCalendarResponse)
        enhancedCalendarResponse;
@end

namespace {

// Test fixture for EnhancedCalendarMediator.
class EnhancedCalendarMediatorTest : public PlatformTest {
 protected:
  EnhancedCalendarMediatorTest()
      : task_environment_(web::WebTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    PlatformTest::SetUp();

    // Pin timezone to UTC to avoid flakiness.
    original_timezone_ = [NSTimeZone defaultTimeZone];
    [NSTimeZone
        setDefaultTimeZone:[NSTimeZone timeZoneWithAbbreviation:@"UTC"]];

    // Set up a fake profile with the OptimizationGuideService.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    // Set up a fake web state.
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetBrowserState(profile_.get());

    // Initialize configuration.
    config_ = [[EnhancedCalendarConfiguration alloc] init];
    config_.calendarEventConfig = [[CalendarEventConfiguration alloc] init];
    config_.URL = "https://example.com";

    // Create the mediator.
    mediator_ =
        [[EnhancedCalendarMediator alloc] initWithWebState:web_state_.get()
                                    enhancedCalendarConfig:config_];
  }

  void TearDown() override {
    [mediator_ disconnect];
    mediator_ = nil;
    config_ = nil;
    web_state_.reset();
    profile_.reset();

    // Restore the original timezone.
    [NSTimeZone setDefaultTimeZone:original_timezone_];

    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::FakeWebState> web_state_;
  EnhancedCalendarConfiguration* config_;
  EnhancedCalendarMediator* mediator_;
  NSTimeZone* original_timezone_;
};

// Tests that cancelling the request correctly notifies the delegate.
TEST_F(EnhancedCalendarMediatorTest, CancelCallsDelegate) {
  FakeEnhancedCalendarMediatorDelegate* delegate =
      [[FakeEnhancedCalendarMediatorDelegate alloc] init];
  mediator_.delegate = delegate;

  [mediator_ cancelEnhancedCalendarRequestAndDismissBottomSheet];

  EXPECT_TRUE(delegate.cancelCalled);
}

// Tests that a primary account change error triggers a cancellation.
TEST_F(EnhancedCalendarMediatorTest, HandleResponseAccountChangeError) {
  FakeEnhancedCalendarMediatorDelegate* delegate =
      [[FakeEnhancedCalendarMediatorDelegate alloc] init];
  mediator_.delegate = delegate;

  std::string error = ai::GetEnhancedCalendarErrorString(
      ai::EnhancedCalendarError::kPrimaryAccountChangeError);
  ai::mojom::EnhancedCalendarResponseResultPtr result =
      ai::mojom::EnhancedCalendarResponseResult::NewError(error);

  [mediator_ handleEnhancedCalendarResponseResult:std::move(result)];

  EXPECT_TRUE(delegate.cancelCalled);
  EXPECT_FALSE(delegate.presentCalled);
}

// Tests that other errors result in presenting the current configuration as a
// fallback.
TEST_F(EnhancedCalendarMediatorTest, HandleResponseOtherError) {
  FakeEnhancedCalendarMediatorDelegate* delegate =
      [[FakeEnhancedCalendarMediatorDelegate alloc] init];
  mediator_.delegate = delegate;

  ai::mojom::EnhancedCalendarResponseResultPtr result =
      ai::mojom::EnhancedCalendarResponseResult::NewError("some other error");

  [mediator_ handleEnhancedCalendarResponseResult:std::move(result)];

  EXPECT_FALSE(delegate.cancelCalled);
  EXPECT_TRUE(delegate.presentCalled);
  EXPECT_EQ(delegate.lastConfig, config_);
}

// Tests that the configuration is correctly populated from the proto response.
TEST_F(EnhancedCalendarMediatorTest, UpdateConfigPopulatesFields) {
  optimization_guide::proto::EnhancedCalendarResponse response;
  response.set_event_title("Test Title");
  response.set_event_summary("Test Summary");
  response.set_event_location("Test Location");
  response.set_event_confirmation_code("CONF123");
  response.set_is_event_booked(true);
  response.set_is_all_day(true);
  response.set_start_date("10/04/2026");
  response.set_start_time("15:30");
  response.set_end_date("10/04/2026");
  response.set_end_time("16:30");

  [mediator_ updateConfigWithEnhancedCalendarResponse:response];

  // Verify title prefixing for booked events.
  EXPECT_NSEQ(@"[BOOKED] Test Title", config_.calendarEventConfig.eventTitle);
  EXPECT_TRUE(config_.calendarEventConfig.isAllDay);

  // Verify date and time parsing.
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  [dateFormatter setDateFormat:@"dd/MM/yyyy HH:mm"];
  NSDate* expectedStartDate =
      [dateFormatter dateFromString:@"10/04/2026 15:30"];
  NSDate* expectedEndDate = [dateFormatter dateFromString:@"10/04/2026 16:30"];

  EXPECT_TRUE(
      [config_.calendarEventConfig.startDateTime isEqual:expectedStartDate]);
  EXPECT_TRUE(
      [config_.calendarEventConfig.endDateTime isEqual:expectedEndDate]);

  // Verify description construction.
  NSString* expectedDescription =
      @"Test Summary\n\nLocation: Test Location\nConfirmation code: "
       "CONF123\nURL: https://example.com";
  EXPECT_NSEQ(expectedDescription,
              config_.calendarEventConfig.eventDescription);
}

// Tests that the prefix is not applied when is_event_booked is false.
TEST_F(EnhancedCalendarMediatorTest, UpdateConfigUnbookedEvent) {
  optimization_guide::proto::EnhancedCalendarResponse response;
  response.set_event_title("Test Title");
  response.set_is_event_booked(false);

  [mediator_ updateConfigWithEnhancedCalendarResponse:response];

  EXPECT_NSEQ(@"Test Title", config_.calendarEventConfig.eventTitle);
}

// Tests that optional fields are omitted from description when missing in
// proto.
TEST_F(EnhancedCalendarMediatorTest, UpdateConfigHandlesMissingFields) {
  optimization_guide::proto::EnhancedCalendarResponse response;
  response.set_event_title("Test Title");
  response.set_event_summary("Test Summary");

  [mediator_ updateConfigWithEnhancedCalendarResponse:response];

  NSString* expectedDescription = @"Test Summary\n\nURL: https://example.com";
  EXPECT_NSEQ(expectedDescription,
              config_.calendarEventConfig.eventDescription);
}

}  // namespace

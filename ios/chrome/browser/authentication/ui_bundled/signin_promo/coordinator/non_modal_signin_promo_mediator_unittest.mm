// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/coordinator/non_modal_signin_promo_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_promo/signin_promo_types.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/non_modal_signin_promo_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// Fake implementation of NonModalSignInPromoCommands for testing
@interface FakeNonModalSignInPromoHandler
    : NSObject <NonModalSignInPromoCommands>

@property(nonatomic, assign) NSInteger dismissCount;
@property(nonatomic, assign) NSInteger showCount;
@property(nonatomic, assign) SignInPromoType lastPromoType;

@end

@implementation FakeNonModalSignInPromoHandler

- (instancetype)init {
  self = [super init];
  if (self) {
    _dismissCount = 0;
    _showCount = 0;
  }
  return self;
}

- (void)dismissNonModalSignInPromo {
  _dismissCount++;
}

- (void)showNonModalSignInPromoWithType:(SignInPromoType)promoType {
  _showCount++;
  _lastPromoType = promoType;
}

@end

// Fake implementation of NonModalSignInPromoMediatorDelegate for testing
@interface FakeNonModalSignInPromoMediatorDelegate
    : NSObject <NonModalSignInPromoMediatorDelegate>

@property(nonatomic, assign) NSInteger timerExpiredCount;
@property(nonatomic, assign) NSInteger shouldDismissCount;
@property(nonatomic, weak) NonModalSignInPromoMediator* lastMediator;

@end

@implementation FakeNonModalSignInPromoMediatorDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _timerExpiredCount = 0;
    _shouldDismissCount = 0;
  }
  return self;
}

- (void)nonModalSignInPromoMediatorTimerExpired:
    (NonModalSignInPromoMediator*)mediator {
  _timerExpiredCount++;
  _lastMediator = mediator;
}

- (void)nonModalSignInPromoMediatorShouldDismiss:
    (NonModalSignInPromoMediator*)mediator {
  _shouldDismissCount++;
  _lastMediator = mediator;
}

@end

namespace {
std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* context) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}
}  // namespace

class NonModalSignInPromoMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Setup fake identity and system identity manager
    identity_ = [FakeSystemIdentity fakeIdentity1];
    auto* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);

    // Setup profile with fake authentication service and mock tracker factory
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));

    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    profile_ = std::move(builder).Build();
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());

    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());

    // Get the mock tracker from the profile
    mock_tracker_ = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForProfile(profile_.get()));

    // Create fake delegate and command handler
    fake_delegate_ = [[FakeNonModalSignInPromoMediatorDelegate alloc] init];
    fake_command_handler_ = [[FakeNonModalSignInPromoHandler alloc] init];

    // Create mediator with password promo type by default
    mediator_ = [[NonModalSignInPromoMediator alloc]
        initWithAuthenticationService:authentication_service_
                      identityManager:identity_manager
             featureEngagementTracker:mock_tracker_
                            promoType:SignInPromoType::kPassword];

    mediator_.delegate = fake_delegate_;
  }

  void TearDown() override {
    // Disconnect mediator to clean up the resources
    [mediator_ disconnect];
    mediator_ = nil;

    PlatformTest::TearDown();
  }

  // Fast-forwards time to trigger the display timer
  void FireDisplayTimer() {
    base::RunLoop run_loop;
    task_environment_.FastForwardBy(base::Seconds(1));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Helper method to fast-forward time to trigger the timeout timer
  void FireTimeoutTimer() {
    base::RunLoop run_loop;
    task_environment_.FastForwardBy(base::Seconds(8));
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Environment and state for the tests
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  std::unique_ptr<TestProfileIOS> profile_;
  FakeSystemIdentity* identity_ = nullptr;
  raw_ptr<AuthenticationService> authentication_service_ = nullptr;
  raw_ptr<feature_engagement::test::MockTracker> mock_tracker_;

  FakeNonModalSignInPromoMediatorDelegate* fake_delegate_ = nil;
  FakeNonModalSignInPromoHandler* fake_command_handler_ = nil;

  NonModalSignInPromoMediator* mediator_ = nil;
};

// Tests that the display timer fires after the delay and calls the delegate
TEST_F(NonModalSignInPromoMediatorTest, DisplayTimerFiresAndCallsDelegate) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  // Start the display timer
  [mediator_ startPromoDisplayTimer];

  // Fast-forward time to trigger the timer
  FireDisplayTimer();

  // Verify delegate was called
  EXPECT_EQ(1, fake_delegate_.timerExpiredCount);
  EXPECT_EQ(mediator_, fake_delegate_.lastMediator);
}

// Tests that the timeout timer fires and calls the delegate's should dismiss
// method
TEST_F(NonModalSignInPromoMediatorTest,
       TimeoutTimerCallsDelegateShouldDismiss) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  // Start the display timer
  [mediator_ startPromoDisplayTimer];

  // Fast-forward time to trigger the display timer
  FireDisplayTimer();

  // Fast-forward time to trigger the timeout timer
  FireTimeoutTimer();

  // Verify delegate was called with proper method
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_EQ(mediator_, fake_delegate_.lastMediator);
}

// Tests that the promo is dismissed when the user signs in
TEST_F(NonModalSignInPromoMediatorTest, DismissWhenUserSignsIn) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  // Start the display timer
  [mediator_ startPromoDisplayTimer];

  // Fast-forward time to trigger the display timer
  FireDisplayTimer();

  // Sign in the user
  authentication_service_->SignIn(identity_,
                                  signin_metrics::AccessPoint::kUnknown);

  // Verify delegate was called to dismiss
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_EQ(mediator_, fake_delegate_.lastMediator);
}

// Tests that stopTimeOutTimers correctly resets the timeout timer
TEST_F(NonModalSignInPromoMediatorTest, StopTimeOutTimersResetsTimer) {
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));

  // Start the display timer
  [mediator_ startPromoDisplayTimer];

  // Fast-forward time to trigger the display timer (which starts timeout timer)
  FireDisplayTimer();

  // Stop the timeout timer
  [mediator_ stopTimeOutTimers];

  // Fast-forward time to when timeout timer would fire
  FireTimeoutTimer();

  // Verify shouldDismiss wasn't called because we stopped the timer
  EXPECT_EQ(0, fake_delegate_.shouldDismissCount);
}

// Tests that the promo is not displayed when the feature engagement tracker
// indicates it should not be shown
TEST_F(NonModalSignInPromoMediatorTest,
       DoNotShowPromoWhenTrackerDisallowsAtWouldStage) {
  // Set up the mock tracker to disallow the password promo
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(false));

  // Start the display timer
  [mediator_ startPromoDisplayTimer];

  // The mediator should immediately dismiss instead of waiting for the timer
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_EQ(0, fake_delegate_.timerExpiredCount);

  // Verify that NotifyEvent was never called
  EXPECT_CALL(*mock_tracker_, NotifyEvent(testing::_)).Times(0);
}

// Tests that the promo is not displayed when the feature engagement tracker
// allows at WouldTriggerHelpUI but disallows at ShouldTriggerHelpUI
TEST_F(NonModalSignInPromoMediatorTest,
       DoNotShowPromoWhenTrackerDisallowsAtShouldStage) {
  // Set up the mock tracker to allow at WouldTriggerHelpUI
  EXPECT_CALL(
      *mock_tracker_,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(true));

  // But disallow at ShouldTriggerHelpUI
  EXPECT_CALL(
      *mock_tracker_,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSPromoNonModalSigninPasswordFeature)))
      .WillRepeatedly(testing::Return(false));

  // Start the display timer
  [mediator_ startPromoDisplayTimer];

  // Fast-forward time to trigger the timer
  FireDisplayTimer();

  // The mediator should dismiss after timer fires without showing the promo
  EXPECT_EQ(1, fake_delegate_.shouldDismissCount);
  EXPECT_EQ(0, fake_delegate_.timerExpiredCount);

  // Verify that NotifyEvent was never called
  EXPECT_CALL(*mock_tracker_, NotifyEvent(testing::_)).Times(0);
}

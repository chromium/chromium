// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_mediator.h"

#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer.h"
#import "ios/chrome/browser/ui/first_run/legacy_signin/legacy_signin_screen_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Fake implementing the consumer protocol.
@interface FakeLegacySigninScreenConsumer
    : NSObject <LegacySigninScreenConsumer>

@property(nonatomic, assign) BOOL hidden;
@property(nonatomic, copy) NSString* userName;
@property(nonatomic, copy) NSString* email;
@property(nonatomic, copy) NSString* givenName;
@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) BOOL UIWasEnabled;

@end

@implementation FakeLegacySigninScreenConsumer

- (void)noIdentityAvailable {
  self.hidden = YES;
}

- (void)setSelectedIdentityUserName:(NSString*)userName
                              email:(NSString*)email
                          givenName:(NSString*)givenName
                             avatar:(UIImage*)avatar {
  self.userName = userName;
  self.email = email;
  self.givenName = givenName;
  self.avatar = avatar;
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  self.UIWasEnabled = UIEnabled;
}

@end

class LegacySigninScreenMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    mediator_ = [[LegacySigninScreenMediator alloc]
        initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForBrowserState(
                                              browser_state_.get())
                authenticationService:AuthenticationServiceFactory::
                                          GetForBrowserState(
                                              browser_state_.get())];
    consumer_ = [[FakeLegacySigninScreenConsumer alloc] init];
    identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  void TearDown() override {
    PlatformTest::TearDown();
    [mediator_ disconnect];
    browser_state_.reset();
    identity_service_->WaitForServiceCallbacksToComplete();
  }

  void SetIdentityService(
      std::unique_ptr<ios::FakeChromeIdentityService> service) {
    // Run all callbacks on the old service.
    identity_service_->WaitForServiceCallbacksToComplete();

    // Update the service in the browser provider.
    identity_service_ = service.get();
    ios::TestChromeBrowserProvider::GetTestProvider()
        .SetChromeIdentityServiceForTesting(std::move(service));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  LegacySigninScreenMediator* mediator_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  ios::FakeChromeIdentityService* identity_service_;
  FakeLegacySigninScreenConsumer* consumer_;
  FakeSystemIdentity* identity_;
};

// Tests that setting the consumer after the selected identity is set is
// correctly notifying the consumer.
TEST_F(LegacySigninScreenMediatorTest,
       TestSettingConsumerWithExistingIdentity) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Regular);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));
}

// Tests that the consumer is correctly updated when the selected identity is
// updated.
TEST_F(LegacySigninScreenMediatorTest, TestUpdatingSelectedIdentity) {
  mediator_.consumer = consumer_;

  EXPECT_EQ(nil, consumer_.email);
  EXPECT_EQ(nil, consumer_.userName);
  // True because the selected identity is nil.
  EXPECT_TRUE(consumer_.hidden);
  EXPECT_EQ(nil, consumer_.avatar);

  consumer_.hidden = NO;
  mediator_.selectedIdentity = identity_;

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Regular);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));
}

// Tests IdentityService observations of the identity list.
TEST_F(LegacySigninScreenMediatorTest, TestIdentityListChanged) {
  mediator_.consumer = consumer_;

  EXPECT_EQ(nil, consumer_.email);
  EXPECT_EQ(nil, consumer_.userName);
  // True because the selected identity is nil.
  EXPECT_TRUE(consumer_.hidden);
  EXPECT_EQ(nil, consumer_.avatar);

  consumer_.hidden = NO;

  // Adding an identity is selecting it.
  identity_service_->AddIdentity(identity_);

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::Regular);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));

  // Removing all the identity is resetting the selected identity.
  __block bool callback_done = false;
  identity_service_->ForgetIdentity(identity_, ^(NSError*) {
    callback_done = true;
  });
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    return callback_done;
  }));

  EXPECT_TRUE(consumer_.hidden);
}

// Tests BrowserProvider observation of the identity service.
TEST_F(LegacySigninScreenMediatorTest, TestProfileUpdate) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  ASSERT_EQ(identity_.userEmail, consumer_.email);
  ASSERT_EQ(identity_.userFullName, consumer_.userName);

  FakeSystemIdentity* const second_identity =
      [FakeSystemIdentity fakeIdentity2];
  NSString* const email = second_identity.userEmail;
  NSString* const name = second_identity.userFullName;
  std::unique_ptr<ios::FakeChromeIdentityService> second_service_unique =
      std::make_unique<ios::FakeChromeIdentityService>();
  ios::FakeChromeIdentityService* second_service = second_service_unique.get();
  SetIdentityService(std::move(second_service_unique));

  second_service->AddIdentity(second_identity);

  EXPECT_EQ(email, consumer_.email);
  EXPECT_EQ(name, consumer_.userName);
  // Get the avatar before the fetch (the default avatar).
  UIImage* default_avatar = consumer_.avatar;
  EXPECT_NE(nil, default_avatar);

  // Wait for the avatar to be fetched.
  second_service->WaitForServiceCallbacksToComplete();

  NSString* const updated_email = @"updated@email.com";
  NSString* const updated_name = @"Second - Updated";

  second_identity.userEmail = updated_email;
  second_identity.userFullName = updated_name;

  // The name shouldn't have changed yet.
  EXPECT_EQ(email, consumer_.email);
  EXPECT_EQ(name, consumer_.userName);

  // Triggering the update is updating the consumer.
  second_service->TriggerIdentityUpdateNotification(second_identity);

  EXPECT_EQ(updated_email, consumer_.email);
  EXPECT_EQ(updated_name, consumer_.userName);
  // With the notification the real avatar is expected instead of the default
  // avatar.
  UIImage* real_avatar = consumer_.avatar;
  EXPECT_NE(default_avatar, real_avatar);
}

// Tests Signing In the selected identity.
TEST_F(LegacySigninScreenMediatorTest, TestSignIn) {
  mediator_.selectedIdentity = identity_;
  identity_service_->AddIdentity(identity_);
  mediator_.consumer = consumer_;

  // Set browser UI objects.
  std::unique_ptr<Browser> browser =
      std::make_unique<TestBrowser>(browser_state_.get());
  UIViewController* presenting_view_controller_mock =
      OCMStrictClassMock([UIViewController class]);

  AuthenticationFlowPerformer* performer_mock =
      OCMStrictClassMock([AuthenticationFlowPerformer class]);

  // Set the authenticaiton flow for testing.
  AuthenticationFlow* authentication_flow = [[AuthenticationFlow alloc]
               initWithBrowser:browser.get()
                      identity:identity_
              postSignInAction:POST_SIGNIN_ACTION_NONE
      presentingViewController:presenting_view_controller_mock];
  [authentication_flow setPerformerForTesting:performer_mock];

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());

  // Mock the performer.
  OCMExpect([performer_mock fetchManagedStatus:browser_state_.get()
                                   forIdentity:identity_])
      .andDo(^(NSInvocation*) {
        [authentication_flow didFetchManagedStatus:nil];
      });
  OCMExpect([performer_mock signInIdentity:identity_
                          withHostedDomain:nil
                            toBrowserState:browser_state_.get()])
      .andDo(^(NSInvocation* invocation) {
        auth_service->SignIn(identity_);
      });
  OCMExpect([performer_mock
                shouldHandleMergeCaseForIdentity:identity_
                               browserStatePrefs:browser_state_->GetPrefs()])
      .andReturn(NO);

  // Verify that there is no primary identity already signed in.
  EXPECT_NSEQ(nil,
              auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin));

  // Sign-in asynchronously using the mediator.
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  [mediator_ startSignInWithAuthenticationFlow:authentication_flow
                                    completion:^() {
                                      run_loop_ptr->QuitWhenIdle();
                                    }];

  EXPECT_FALSE(consumer_.UIWasEnabled);

  run_loop.Run();

  EXPECT_TRUE(consumer_.UIWasEnabled);

  EXPECT_NSEQ(identity_,
              auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin));
}

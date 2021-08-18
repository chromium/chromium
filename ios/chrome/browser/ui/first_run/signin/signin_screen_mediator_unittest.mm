// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin/signin_screen_mediator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/first_run/signin/signin_screen_consumer.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Fake implementing the consumer protocol.
@interface FakeSigninScreenConsumer : NSObject <SigninScreenConsumer>

@property(nonatomic, assign) BOOL hidden;
@property(nonatomic, copy) NSString* userName;
@property(nonatomic, copy) NSString* email;
@property(nonatomic, copy) NSString* givenName;
@property(nonatomic, strong) UIImage* avatar;

@end

@implementation FakeSigninScreenConsumer

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

@end

class SigninScreenMediatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));

    browser_state_ = builder.Build();
    mediator_ = [[SigninScreenMediator alloc]
        initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                          GetForBrowserState(
                                              browser_state_.get())
                authenticationService:AuthenticationServiceFactory::
                                          GetForBrowserState(
                                              browser_state_.get())];
    consumer_ = [[FakeSigninScreenConsumer alloc] init];
    identity_ = [FakeChromeIdentity identityWithEmail:@"test@email.com"
                                               gaiaID:@"gaiaID"
                                                 name:@"Test Name"];
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
  SigninScreenMediator* mediator_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  ios::FakeChromeIdentityService* identity_service_;
  FakeSigninScreenConsumer* consumer_;
  FakeChromeIdentity* identity_;
};

// Tests that setting the consumer after the selected identity is set is
// correctly notifying the consumer.
TEST_F(SigninScreenMediatorTest, TestSettingConsumerWithExistingIdentity) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  EXPECT_EQ(identity_.userEmail, consumer_.email);
  EXPECT_EQ(identity_.userFullName, consumer_.userName);
  EXPECT_FALSE(consumer_.hidden);
  UIImage* avatar = consumer_.avatar;
  EXPECT_NE(nil, avatar);
  CGSize expected_size =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::DefaultLarge);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));
}

// Tests that the consumer is correctly updated when the selected identity is
// updated.
TEST_F(SigninScreenMediatorTest, TestUpdatingSelectedIdentity) {
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
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::DefaultLarge);
  EXPECT_TRUE(CGSizeEqualToSize(expected_size, avatar.size));
}

// Tests IdentityService observations of the identity list.
TEST_F(SigninScreenMediatorTest, TestIdentityListChanged) {
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
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::DefaultLarge);
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
TEST_F(SigninScreenMediatorTest, TestProfileUpdate) {
  mediator_.selectedIdentity = identity_;
  mediator_.consumer = consumer_;

  ASSERT_EQ(identity_.userEmail, consumer_.email);
  ASSERT_EQ(identity_.userFullName, consumer_.userName);

  NSString* email = @"second@email.com";
  NSString* name = @"Second identity";
  FakeChromeIdentity* second_identity =
      [FakeChromeIdentity identityWithEmail:email
                                     gaiaID:@"second gaiaID"
                                       name:name];
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

  NSString* updated_email = @"updated@email.com";
  NSString* updated_name = @"Second - Updated";

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
TEST_F(SigninScreenMediatorTest, TestSignIn) {
  mediator_.selectedIdentity = identity_;

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());

  EXPECT_NSEQ(nil, authenticationService->GetPrimaryIdentity(
                       signin::ConsentLevel::kSignin));

  [mediator_ startSignIn];

  EXPECT_NSEQ(identity_, authenticationService->GetPrimaryIdentity(
                             signin::ConsentLevel::kSignin));
}

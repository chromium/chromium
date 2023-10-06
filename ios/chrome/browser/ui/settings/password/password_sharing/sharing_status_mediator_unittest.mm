// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_consumer.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

const CGFloat kProfileImageSize = 60.0;

}

// Test class that conforms to SharingStatusConsumer in order to test the
// consumer methods are called correctly.
@interface FakeSharingStatusConsumer : NSObject <SharingStatusConsumer>

@property(nonatomic, strong) UIImage* senderImage;

@end

@implementation FakeSharingStatusConsumer

- (void)setSenderImage:(UIImage*)senderImage {
  _senderImage = senderImage;
}

@end

class SharingStatusMediatorTest : public PlatformTest {
 protected:
  SharingStatusMediatorTest() {
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  id<SystemIdentity> fake_identity() { return fake_identity_; }

  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  ChromeAccountManagerService* GetAccountManagerService() {
    return ChromeAccountManagerServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  id<SystemIdentity> fake_identity_;
};

TEST_F(SharingStatusMediatorTest, NotifiesSignedInConsumerAboutTheirAvatar) {
  GetAuthenticationService()->SignIn(
      fake_identity(), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()];
  mediator.consumer = consumer;

  EXPECT_NSEQ(UIImagePNGRepresentation(CircularImageFromImage(
                  GetAccountManagerService()->GetIdentityAvatarWithIdentity(
                      fake_identity(), IdentityAvatarSize::Large),
                  kProfileImageSize)),
              UIImagePNGRepresentation(consumer.senderImage));
}

TEST_F(SharingStatusMediatorTest, NotifiesSignedOutConsumerWithDefaultAvatar) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()];
  mediator.consumer = consumer;

  EXPECT_NSEQ(UIImagePNGRepresentation(DefaultSymbolTemplateWithPointSize(
                  kPersonCropCircleSymbol, kProfileImageSize)),
              UIImagePNGRepresentation(consumer.senderImage));
}

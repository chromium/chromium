// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr NSString* kWebsite = @"example.com";
const CGFloat kProfileImageSize = 60.0;
const GURL kGURL = GURL("www.example.com");

NSArray<RecipientInfoForIOSDisplay*>* CreateRecipients(int amount) {
  NSMutableArray<RecipientInfoForIOSDisplay*>* recipients =
      [NSMutableArray array];
  for (int i = 0; i < amount; i++) {
    password_manager::RecipientInfo recipient;
    recipient.user_name = "test" + base::NumberToString(i) + "@gmail.com";
    [recipients addObject:([[RecipientInfoForIOSDisplay alloc]
                              initWithRecipientInfo:recipient])];
  }
  return recipients;
}

}  // namespace

// Test class that conforms to SharingStatusConsumer in order to test the
// consumer methods are called correctly.
@interface FakeSharingStatusConsumer : NSObject <SharingStatusConsumer>

@property(nonatomic, strong) UIImage* senderImage;
@property(nonatomic, strong) UIImage* recipientImage;
@property(nonatomic, strong) NSString* subtitleString;
@property(nonatomic, strong) NSString* footerString;
@property(nonatomic, readonly) GURL URL;

@end

@implementation FakeSharingStatusConsumer

- (void)setSenderImage:(UIImage*)senderImage {
  _senderImage = senderImage;
}

- (void)setRecipientImage:(UIImage*)recipientImage {
  _recipientImage = recipientImage;
}

- (void)setSubtitleString:(NSString*)subtitleString {
  _subtitleString = subtitleString;
}

- (void)setFooterString:(NSString*)footerString {
  _footerString = footerString;
}

- (void)setURL:(const GURL&)URL {
  _URL = URL;
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

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());

    profile_ = std::move(builder).Build();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
  }

  id<SystemIdentity> fake_identity() { return fake_identity_; }

  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForProfile(profile_.get());
  }

  ChromeAccountManagerService* GetAccountManagerService() {
    return ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

  FaviconLoader* GetFaviconLoader() {
    return IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<SystemIdentity> fake_identity_;
};

TEST_F(SharingStatusMediatorTest, NotifiesSignedInConsumerAboutTheirAvatar) {
  GetAuthenticationService()->SignIn(
      fake_identity(), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(1)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
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
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(1)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
  mediator.consumer = consumer;

  EXPECT_NSEQ(UIImagePNGRepresentation(DefaultSymbolTemplateWithPointSize(
                  kPersonCropCircleSymbol, kProfileImageSize)),
              UIImagePNGRepresentation(consumer.senderImage));
}

TEST_F(SharingStatusMediatorTest, NotifiesConsumerWithRecipientImage) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(1)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
  mediator.consumer = consumer;

  EXPECT_NSEQ(UIImagePNGRepresentation(CircularImageFromImage(
                  DefaultSymbolTemplateWithPointSize(
                      kPersonCropCircleSymbol, kAccountProfilePhotoDimension),
                  60.0)),
              UIImagePNGRepresentation(consumer.recipientImage));
}

TEST_F(SharingStatusMediatorTest,
       NotifiesConsumerAboutSingleRecipientSubtitle) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(1)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
  mediator.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
                  IDS_IOS_PASSWORD_SHARING_SUCCESS_SUBTITLE, u"test0@gmail.com",
                  u"example.com")),
              consumer.subtitleString);
}

TEST_F(SharingStatusMediatorTest,
       NotifiesConsumerAboutMultipleRecipientsSubtitle) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(2)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
  mediator.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
                  IDS_IOS_PASSWORD_SHARING_SUCCESS_SUBTITLE_MULTIPLE_RECIPIENTS,
                  u"example.com")),
              consumer.subtitleString);
}

TEST_F(SharingStatusMediatorTest, NotifiesConsumerAboutFooterForWebsite) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(2)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:kGURL];
  mediator.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
                  IDS_IOS_PASSWORD_SHARING_SUCCESS_FOOTNOTE, u"example.com")),
              consumer.footerString);
}

TEST_F(SharingStatusMediatorTest, NotifiesConsumerAboutFooterForAndroidApp) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(2)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
  mediator.consumer = consumer;

  EXPECT_NSEQ(base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
                  IDS_IOS_PASSWORD_SHARING_SUCCESS_FOOTNOTE_ANDROID_APP,
                  u"example.com")),
              consumer.footerString);
}

TEST_F(SharingStatusMediatorTest, NotifiesConsumerAboutGURL) {
  auto* consumer = [[FakeSharingStatusConsumer alloc] init];
  auto* mediator = [[SharingStatusMediator alloc]
        initWithAuthService:GetAuthenticationService()
      accountManagerService:GetAccountManagerService()
              faviconLoader:GetFaviconLoader()
                 recipients:CreateRecipients(2)
                    website:kWebsite
                        URL:kGURL
          changePasswordURL:std::nullopt];
  mediator.consumer = consumer;

  EXPECT_EQ(kGURL, consumer.URL);
}

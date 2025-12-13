// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_confirmation_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_selection_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_mediator_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake consumer for the mediator.
@interface FakeSaveToPhotosSettingsConsumer
    : NSObject <SaveToPhotosSettingsAccountConfirmationConsumer,
                SaveToPhotosSettingsAccountSelectionConsumer>

@property(nonatomic, strong) UIImage* identityAvatar;
@property(nonatomic, copy) NSString* identityName;
@property(nonatomic, copy) NSString* identityEmail;
@property(nonatomic, assign) GaiaId identityGaiaID;
@property(nonatomic, assign) BOOL askEveryTimeSwitchOn;

@property(nonatomic, strong)
    NSArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*
        identityConfigurators;

@end

@implementation FakeSaveToPhotosSettingsConsumer

- (void)setIdentityButtonAvatar:(UIImage*)avatar
                           name:(NSString*)name
                          email:(NSString*)email
                         gaiaID:(const GaiaId&)gaiaID
           askEveryTimeSwitchOn:(BOOL)askEveryTimeSwitchOn {
  self.identityAvatar = avatar;
  self.identityName = name;
  self.identityEmail = email;
  self.identityGaiaID = gaiaID;
  self.askEveryTimeSwitchOn = askEveryTimeSwitchOn;
}

- (void)populateAccountsOnDevice:
    (NSArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*)
        configurators {
  self.identityConfigurators = configurators;
}

- (void)displaySaveToPhotosSettingsUI {
}
- (void)hideSaveToPhotosSettingsUI {
}

@end

// Fake delegate.
@interface FakeSaveToPhotosSettingsMediatorDelegate
    : NSObject <SaveToPhotosSettingsMediatorDelegate>

@property(nonatomic, assign) BOOL hideSaveToPhotosSettingsCalled;

@end

@implementation FakeSaveToPhotosSettingsMediatorDelegate

- (void)hideSaveToPhotosSettings {
  self.hideSaveToPhotosSettingsCalled = YES;
}

@end

// SaveToPhotosSettingsMediator unit tests.
class SaveToPhotosSettingsMediatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();

    fake_identity_a_ = [FakeSystemIdentity fakeIdentity1];
    AddIdentity(fake_identity_a_, /*as_primary=*/true);
    fake_identity_b_ = [FakeSystemIdentity fakeIdentity2];
    AddIdentity(fake_identity_b_, /*as_primary=*/false);
  }

  void TearDown() final { [mediator_ disconnect]; }

  void AddIdentity(id<SystemIdentity> identity, bool as_primary) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);

    // Note: The (cross-platform) IdentityManager isn't fully hooked up to the
    // (iOS-specific) SystemIdentityManager in this test, so update it
    // separately.
    // TODO(crbug.com/368409110): Improve the test plumbing so that this isn't
    // necessary. This likely means either adding plumbing towards
    // SystemIdentityManager to FakeProfileOAuth2TokenService, or alternatively
    // using the real ProfileOAuth2TokenServiceIOSDelegate here.
    auto options =
        signin::AccountAvailabilityOptionsBuilder().WithGaiaId(identity.gaiaId);
    if (as_primary) {
      options = options.AsPrimary(signin::ConsentLevel::kSignin);
    }
    signin::MakeAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        options.Build(base::SysNSStringToUTF8(identity.userEmail)));
  }

  void ForgetIdentity(id<SystemIdentity> identity) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->ForgetIdentityFromOtherApplication(identity);

    // Note: The (cross-platform) IdentityManager isn't fully hooked up to the
    // (iOS-specific) SystemIdentityManager in this test, so update it
    // separately.
    // TODO(crbug.com/368409110): Improve the test plumbing so that this isn't
    // necessary. This likely means either adding plumbing towards
    // SystemIdentityManager to FakeProfileOAuth2TokenService, or alternatively
    // using the real ProfileOAuth2TokenServiceIOSDelegate here.
    signin::RemoveRefreshTokenForAccount(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        CoreAccountId::FromGaiaId(identity.gaiaId));
  }

  // Creates a SaveToPhotosSettingsMediator with services from the test browser
  // state.
  SaveToPhotosSettingsMediator* CreateSaveToPhotosSettingsMediator() {
    PrefService* pref_service = profile_->GetPrefs();
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    mediator_ = [[SaveToPhotosSettingsMediator alloc]
        initWithAccountManagerService:account_manager_service
                          prefService:pref_service
                      identityManager:identity_manager
                        photosService:PhotosServiceFactory::GetForProfile(
                                          profile_.get())];
    return mediator_;
  }

  ChromeAccountManagerService* GetAccountManagerService() {
    return ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

  // Checks that the identities given to the consumer, either through the
  // primary or secondary consumer interfaces, match an expected value
  // `saved_identity`. It does not test the values of `askEveryTimeSwitchOn` or
  // `identityEnabled` in `fake_consumer`.
  void CheckFakeConsumerIdentities(
      FakeSaveToPhotosSettingsConsumer* fake_consumer,
      id<SystemIdentity> saved_identity) {
    UIImage* saved_identity_avatar =
        GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
            saved_identity, IdentityAvatarSize::TableViewIcon);

    // Test that the presented identity is the expected one.
    EXPECT_NSEQ(saved_identity_avatar, fake_consumer.identityAvatar);
    EXPECT_NSEQ(saved_identity.userFullName, fake_consumer.identityName);
    EXPECT_NSEQ(saved_identity.userEmail, fake_consumer.identityEmail);
    EXPECT_EQ(saved_identity.gaiaId, fake_consumer.identityGaiaID);

    // Tests that if there is at least an element, it matches `fake_identity_a_`
    // and is selected if its GAIA ID matches that of the expected selected
    // identity.
    if (fake_consumer.identityConfigurators.count > 0) {
      UIImage* fake_identity_a_avatar =
          GetApplicationContext()
              ->GetIdentityAvatarProvider()
              ->GetIdentityAvatar(fake_identity_a_,
                                  IdentityAvatarSize::TableViewIcon);
      EXPECT_EQ(fake_identity_a_.gaiaId,
                fake_consumer.identityConfigurators[0].gaiaID);
      EXPECT_NSEQ(fake_identity_a_.userFullName,
                  fake_consumer.identityConfigurators[0].name);
      EXPECT_NSEQ(fake_identity_a_.userEmail,
                  fake_consumer.identityConfigurators[0].email);
      EXPECT_NSEQ(fake_identity_a_avatar,
                  fake_consumer.identityConfigurators[0].avatar);
      EXPECT_EQ(fake_consumer.identityConfigurators[0].gaiaID ==
                    saved_identity.gaiaId,
                fake_consumer.identityConfigurators[0].selected);
    }

    // Tests that if there is at least a second element, it matches
    // `fake_identity_b_` and is selected if its GAIA ID matches that of the
    // expected selected identity.
    if (fake_consumer.identityConfigurators.count > 1) {
      UIImage* fake_identity_b_avatar =
          GetApplicationContext()
              ->GetIdentityAvatarProvider()
              ->GetIdentityAvatar(fake_identity_b_,
                                  IdentityAvatarSize::TableViewIcon);
      EXPECT_EQ(fake_identity_b_.gaiaId,
                fake_consumer.identityConfigurators[1].gaiaID);
      EXPECT_NSEQ(fake_identity_b_.userFullName,
                  fake_consumer.identityConfigurators[1].name);
      EXPECT_NSEQ(fake_identity_b_.userEmail,
                  fake_consumer.identityConfigurators[1].email);
      EXPECT_NSEQ(fake_identity_b_avatar,
                  fake_consumer.identityConfigurators[1].avatar);
      EXPECT_EQ(fake_consumer.identityConfigurators[1].gaiaID ==
                    saved_identity.gaiaId,
                fake_consumer.identityConfigurators[1].selected);
    }
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<SystemIdentity> fake_identity_a_;
  id<SystemIdentity> fake_identity_b_;
  SaveToPhotosSettingsMediator* mediator_;
};

// Tests that invoking the SaveToPhotosSettingsMutator interface on the mediator
// leads to the expected changes in the preferences.
// TODO(crbug.com/451601299): Re-enable the test.
TEST_F(SaveToPhotosSettingsMediatorTest, DISABLED_CanMutateSelectedIdentity) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  profile_->GetPrefs()->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                                  fake_identity_a_.gaiaId.ToString());
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  FakeSaveToPhotosSettingsConsumer* fake_consumer =
      [[FakeSaveToPhotosSettingsConsumer alloc] init];
  mediator.accountConfirmationConsumer = fake_consumer;
  mediator.accountSelectionConsumer = fake_consumer;

  GaiaId gaiaID = fake_identity_b_.gaiaId;
  [mediator setSelectedIdentityGaiaID:&gaiaID];
  EXPECT_EQ(
      fake_identity_b_.gaiaId.ToString(),
      profile_->GetPrefs()->GetString(prefs::kIosSaveToPhotosDefaultGaiaId));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kIosSaveToPhotosSkipAccountPicker));

  [mediator setAskWhichAccountToUseEveryTime:YES];
  EXPECT_EQ(
      fake_identity_b_.gaiaId.ToString(),
      profile_->GetPrefs()->GetString(prefs::kIosSaveToPhotosDefaultGaiaId));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kIosSaveToPhotosSkipAccountPicker));

  [mediator disconnect];
}

// Tests that external changes in preferences leads to expected changes in the
// consumer.
// TODO(crbug.com/451601299): Re-enable the test.
TEST_F(SaveToPhotosSettingsMediatorTest,
       DISABLED_ExternalPrefChangeUpdatesConsumers) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  profile_->GetPrefs()->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                                  fake_identity_a_.gaiaId.ToString());
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  FakeSaveToPhotosSettingsConsumer* fake_consumer =
      [[FakeSaveToPhotosSettingsConsumer alloc] init];
  mediator.accountConfirmationConsumer = fake_consumer;
  mediator.accountSelectionConsumer = fake_consumer;

  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_TRUE(fake_consumer.askEveryTimeSwitchOn);

  profile_->GetPrefs()->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                                  fake_identity_b_.gaiaId.ToString());
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_b_);
  EXPECT_TRUE(fake_consumer.askEveryTimeSwitchOn);

  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosSkipAccountPicker);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_b_);
  EXPECT_FALSE(fake_consumer.askEveryTimeSwitchOn);

  [mediator disconnect];
}

// Tests that external changes in the list of accounts authenticated on the
// device leads to expected changes in the consumer.
// TODO(crbug.com/451601299): Re-enable the test.
TEST_F(SaveToPhotosSettingsMediatorTest,
       DISABLED_ExternalAccountsChangeUpdatesConsumers) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  profile_->GetPrefs()->SetString(prefs::kIosSaveToPhotosDefaultGaiaId,
                                  fake_identity_a_.gaiaId.ToString());
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  FakeSaveToPhotosSettingsConsumer* fake_consumer =
      [[FakeSaveToPhotosSettingsConsumer alloc] init];
  mediator.accountConfirmationConsumer = fake_consumer;
  mediator.accountSelectionConsumer = fake_consumer;

  ASSERT_EQ(2U, fake_consumer.identityConfigurators.count);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_TRUE(fake_consumer.askEveryTimeSwitchOn);

  ForgetIdentity(fake_identity_b_);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_TRUE(fake_consumer.askEveryTimeSwitchOn);
  EXPECT_EQ(1U, fake_consumer.identityConfigurators.count);

  fake_identity_b_ = [FakeSystemIdentity fakeIdentity3];
  AddIdentity(fake_identity_b_, /*as_primary=*/false);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_TRUE(fake_consumer.askEveryTimeSwitchOn);
  EXPECT_EQ(2U, fake_consumer.identityConfigurators.count);

  [mediator disconnect];
}

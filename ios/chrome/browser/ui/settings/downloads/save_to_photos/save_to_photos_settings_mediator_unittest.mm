// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_confirmation_consumer.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_consumer.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mediator_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Fake consumer for the mediator.
@interface FakeSaveToPhotosSettingsConsumer
    : NSObject <SaveToPhotosSettingsAccountConfirmationConsumer,
                SaveToPhotosSettingsAccountSelectionConsumer>

@property(nonatomic, strong) UIImage* identityAvatar;
@property(nonatomic, copy) NSString* identityName;
@property(nonatomic, copy) NSString* identityEmail;
@property(nonatomic, copy) NSString* identityGaiaID;
@property(nonatomic, assign) BOOL askEveryTimeSwitchOn;

@property(nonatomic, strong)
    NSArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*
        identityConfigurators;

@end

@implementation FakeSaveToPhotosSettingsConsumer

- (void)setIdentityButtonAvatar:(UIImage*)avatar
                           name:(NSString*)name
                          email:(NSString*)email
                         gaiaID:(NSString*)gaiaID
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

    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    fake_identity_a_ = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity_a_);
    fake_identity_b_ = [FakeSystemIdentity fakeIdentity2];
    system_identity_manager->AddIdentity(fake_identity_b_);

    signin::MakeAccountAvailable(
        IdentityManagerFactory::GetForProfile(profile_.get()),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithGaiaId(base::SysNSStringToUTF8(fake_identity_a_.gaiaID))
            .Build(base::SysNSStringToUTF8(fake_identity_a_.userEmail)));
  }

  void TearDown() final { [mediator_ disconnect]; }

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
                      identityManager:identity_manager];
    return mediator_;
  }

  ChromeAccountManagerService* GetAccountManagerService() {
    return ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  }

  // Checks that the identities given to the consumer, either through the
  // primary or secondary consumer interfaces, match an expected value
  // `expected_presented_identity`. It does not test the values of
  // `askEveryTimeSwitchOn` or `identityEnabled` in `fake_consumer`.
  void CheckFakeConsumerIdentities(
      FakeSaveToPhotosSettingsConsumer* fake_consumer,
      id<SystemIdentity> saved_identity) {
    UIImage* saved_identity_avatar =
        GetAccountManagerService()->GetIdentityAvatarWithIdentity(
            saved_identity, IdentityAvatarSize::TableViewIcon);

    // Test that the presented identity is the expected one.
    EXPECT_NSEQ(saved_identity_avatar, fake_consumer.identityAvatar);
    EXPECT_NSEQ(saved_identity.userFullName, fake_consumer.identityName);
    EXPECT_NSEQ(saved_identity.userEmail, fake_consumer.identityEmail);
    EXPECT_NSEQ(saved_identity.gaiaID, fake_consumer.identityGaiaID);

    // Tests that if there is at least an element, it matches `fake_identity_a_`
    // and is selected if its GAIA ID matches that of the expected selected
    // identity.
    if (fake_consumer.identityConfigurators.count > 0) {
      UIImage* fake_identity_a_avatar =
          GetAccountManagerService()->GetIdentityAvatarWithIdentity(
              fake_identity_a_, IdentityAvatarSize::TableViewIcon);
      EXPECT_NSEQ(fake_identity_a_.gaiaID,
                  fake_consumer.identityConfigurators[0].gaiaID);
      EXPECT_NSEQ(fake_identity_a_.userFullName,
                  fake_consumer.identityConfigurators[0].name);
      EXPECT_NSEQ(fake_identity_a_.userEmail,
                  fake_consumer.identityConfigurators[0].email);
      EXPECT_NSEQ(fake_identity_a_avatar,
                  fake_consumer.identityConfigurators[0].avatar);
      EXPECT_EQ([fake_consumer.identityConfigurators[0].gaiaID
                    isEqual:saved_identity.gaiaID],
                fake_consumer.identityConfigurators[0].selected);
    }

    // Tests that if there is at least a second element, it matches
    // `fake_identity_b_` and is selected if its GAIA ID matches that of the
    // expected selected identity.
    if (fake_consumer.identityConfigurators.count > 1) {
      UIImage* fake_identity_b_avatar =
          GetAccountManagerService()->GetIdentityAvatarWithIdentity(
              fake_identity_b_, IdentityAvatarSize::TableViewIcon);
      EXPECT_NSEQ(fake_identity_b_.gaiaID,
                  fake_consumer.identityConfigurators[1].gaiaID);
      EXPECT_NSEQ(fake_identity_b_.userFullName,
                  fake_consumer.identityConfigurators[1].name);
      EXPECT_NSEQ(fake_identity_b_.userEmail,
                  fake_consumer.identityConfigurators[1].email);
      EXPECT_NSEQ(fake_identity_b_avatar,
                  fake_consumer.identityConfigurators[1].avatar);
      EXPECT_EQ([fake_consumer.identityConfigurators[1].gaiaID
                    isEqual:saved_identity.gaiaID],
                fake_consumer.identityConfigurators[1].selected);
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id<SystemIdentity> fake_identity_a_;
  id<SystemIdentity> fake_identity_b_;
  SaveToPhotosSettingsMediator* mediator_;
};

// Tests that invoking the SaveToPhotosSettingsMutator interface on the mediator
// leads to the expected changes in the preferences.
TEST_F(SaveToPhotosSettingsMediatorTest, CanMutateSelectedIdentity) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  profile_->GetPrefs()->SetString(
      prefs::kIosSaveToPhotosDefaultGaiaId,
      base::SysNSStringToUTF8(fake_identity_a_.gaiaID));
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  FakeSaveToPhotosSettingsConsumer* fake_consumer =
      [[FakeSaveToPhotosSettingsConsumer alloc] init];
  mediator.accountConfirmationConsumer = fake_consumer;
  mediator.accountSelectionConsumer = fake_consumer;

  [mediator setSelectedIdentityGaiaID:fake_identity_b_.gaiaID];
  EXPECT_EQ(
      base::SysNSStringToUTF8(fake_identity_b_.gaiaID),
      profile_->GetPrefs()->GetString(prefs::kIosSaveToPhotosDefaultGaiaId));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kIosSaveToPhotosSkipAccountPicker));

  [mediator setAskWhichAccountToUseEveryTime:YES];
  EXPECT_EQ(
      base::SysNSStringToUTF8(fake_identity_b_.gaiaID),
      profile_->GetPrefs()->GetString(prefs::kIosSaveToPhotosDefaultGaiaId));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kIosSaveToPhotosSkipAccountPicker));

  [mediator disconnect];
}

// Tests that external changes in preferences leads to expected changes in the
// consumer.
TEST_F(SaveToPhotosSettingsMediatorTest, ExternalPrefChangeUpdatesConsumers) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  profile_->GetPrefs()->SetString(
      prefs::kIosSaveToPhotosDefaultGaiaId,
      base::SysNSStringToUTF8(fake_identity_a_.gaiaID));
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  FakeSaveToPhotosSettingsConsumer* fake_consumer =
      [[FakeSaveToPhotosSettingsConsumer alloc] init];
  mediator.accountConfirmationConsumer = fake_consumer;
  mediator.accountSelectionConsumer = fake_consumer;

  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_FALSE(fake_consumer.askEveryTimeSwitchOn);

  profile_->GetPrefs()->SetString(
      prefs::kIosSaveToPhotosDefaultGaiaId,
      base::SysNSStringToUTF8(fake_identity_b_.gaiaID));
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_b_);
  EXPECT_FALSE(fake_consumer.askEveryTimeSwitchOn);

  profile_->GetPrefs()->ClearPref(prefs::kIosSaveToPhotosSkipAccountPicker);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_b_);
  EXPECT_TRUE(fake_consumer.askEveryTimeSwitchOn);

  [mediator disconnect];
}

// Tests that external changes in the list of accounts authenticated on the
// device leads to expected changes in the consumer.
TEST_F(SaveToPhotosSettingsMediatorTest,
       ExternalAccountsChangeUpdatesConsumers) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  profile_->GetPrefs()->SetString(
      prefs::kIosSaveToPhotosDefaultGaiaId,
      base::SysNSStringToUTF8(fake_identity_a_.gaiaID));
  profile_->GetPrefs()->SetBoolean(prefs::kIosSaveToPhotosSkipAccountPicker,
                                   true);

  FakeSaveToPhotosSettingsConsumer* fake_consumer =
      [[FakeSaveToPhotosSettingsConsumer alloc] init];
  mediator.accountConfirmationConsumer = fake_consumer;
  mediator.accountSelectionConsumer = fake_consumer;

  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_FALSE(fake_consumer.askEveryTimeSwitchOn);

  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->ForgetIdentityFromOtherApplication(fake_identity_b_);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_FALSE(fake_consumer.askEveryTimeSwitchOn);
  EXPECT_EQ(1U, fake_consumer.identityConfigurators.count);

  fake_identity_b_ = [FakeSystemIdentity fakeIdentity3];
  system_identity_manager->AddIdentity(fake_identity_b_);
  CheckFakeConsumerIdentities(fake_consumer, fake_identity_a_);
  EXPECT_FALSE(fake_consumer.askEveryTimeSwitchOn);
  EXPECT_EQ(2U, fake_consumer.identityConfigurators.count);

  [mediator disconnect];
}

// Tests that the mediator asks to hide Save to Photos settings if the user
// signs out.
TEST_F(SaveToPhotosSettingsMediatorTest, HidesSettingsIfUserSignsOut) {
  SaveToPhotosSettingsMediator* mediator = CreateSaveToPhotosSettingsMediator();

  FakeSaveToPhotosSettingsMediatorDelegate* fake_delegate =
      [[FakeSaveToPhotosSettingsMediatorDelegate alloc] init];
  mediator.delegate = fake_delegate;

  signin::ClearPrimaryAccount(
      IdentityManagerFactory::GetForProfile(profile_.get()));

  EXPECT_TRUE(fake_delegate.hideSaveToPhotosSettingsCalled);

  [mediator disconnect];
}

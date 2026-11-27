// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider/model/credential_provider_migrator_app_agent.h"

#import <Foundation/Foundation.h>

#import <cstdint>
#import <memory>
#import <string_view>
#import <utility>
#import <vector>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/run_until.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/fake_startup_information.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/credential_provider/model/fake_credential_provider_settings_handler.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/archivable_credential+passkey.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/archivable_credential_test_util.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr std::string_view kPageUrl = "https://example.com";

class CredentialProviderMigratorAppAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    ClearAppGroupUserDefaults();

    profile_ = profile_manager_.AddProfileWithBuilder(CreateProfileBuilder());
    passkey_model_ = static_cast<webauthn::TestPasskeyModel*>(
        IOSPasskeyModelFactory::GetForProfile(profile_));

    identity_ = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity_);
    system_identity_manager->WaitForServiceCallbacksToComplete();
    AuthenticationServiceFactory::GetForProfile(profile_)->SignIn(
        identity_, signin_metrics::AccessPoint::kStartPage);

    startup_information_ = [[FakeStartupInformation alloc] init];
    app_state_ =
        [[AppState alloc] initWithStartupInformation:startup_information_];
    profile_state_ = [[ProfileState alloc] initWithAppState:app_state_];
    profile_state_.profile = profile_;
    [app_state_ profileStateCreated:profile_state_];

    app_agent_ = [[CredentialProviderMigratorAppAgent alloc] init];
    [app_state_ addAgent:app_agent_];
    [app_state_ updateInitStage:AppInitStage::kFinal];
  }

  void TearDown() override {
    [app_state_ removeAgent:app_agent_];
    app_agent_ = nil;

    for (FakeSceneState* scene : scenes_) {
      [app_state_ sceneStateDisconnected:scene];
      scene.activationLevel = SceneActivationLevelDisconnected;
      [scene shutdown];
    }
    scenes_.clear();
    settings_handlers_.clear();

    for (ProfileState* extra_profile_state : extra_profile_states_) {
      [app_state_ profileStateDestroyed:extra_profile_state];
      extra_profile_state.profile = nullptr;
    }
    extra_profile_states_.clear();

    [app_state_ profileStateDestroyed:profile_state_];
    profile_state_.profile = nullptr;
    profile_state_ = nil;
    app_state_ = nil;
    startup_information_ = nil;
    passkey_model_ = nullptr;
    profile_ = nullptr;

    ClearAppGroupUserDefaults();
    PlatformTest::TearDown();
  }

  // Returns a `TestProfileIOS::Builder` configured with the factories required
  // by `CredentialProviderMigratorAppAgent`.
  TestProfileIOS::Builder CreateProfileBuilder() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(&password_manager::BuildPasswordStore<
                       password_manager::TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAccountPasswordStoreFactory::GetInstance(),
        base::BindOnce(&password_manager::BuildPasswordStoreWithArgs<
                           password_manager::TestPasswordStore,
                           password_manager::IsAccountStore>,
                       password_manager::IsAccountStore(true)));
    builder.AddTestingFactory(
        IOSPasskeyModelFactory::GetInstance(),
        base::BindOnce(&std::make_unique<webauthn::TestPasskeyModel>));
    return builder;
  }

  // Creates a `FakeSceneState` in background state for `profile` and
  // `profile_state`, optionally registers `SettingsCommands` handlers on its
  // regular and incognito browsers, and connects it to `app_state_` and
  // `profile_state`.
  FakeSceneState* CreateConnectedSceneForProfile(
      ProfileIOS* profile,
      ProfileState* profile_state,
      bool register_settings_commands) {
    FakeSceneState* scene = [[FakeSceneState alloc] initWithProfile:profile];
    scene.activationLevel = SceneActivationLevelBackground;
    if (register_settings_commands) {
      RegisterSettingsHandler(scene.browserProviderInterface.mainBrowser);
      RegisterSettingsHandler(scene.browserProviderInterface.incognitoBrowser);
    }
    [app_state_ sceneStateConnected:scene];
    [profile_state sceneStateConnected:scene];
    scenes_.push_back(scene);
    return scene;
  }

  // Creates a `FakeSceneState` connected to the default `profile_` and
  // `profile_state_`.
  FakeSceneState* CreateConnectedScene(bool register_settings_commands) {
    return CreateConnectedSceneForProfile(profile_, profile_state_,
                                          register_settings_commands);
  }

  // Registers and retains a `FakeCredentialProviderSettingsHandler` on
  // `browser`'s `CommandDispatcher`.
  FakeCredentialProviderSettingsHandler* RegisterSettingsHandler(
      Browser* browser) {
    FakeCredentialProviderSettingsHandler* handler =
        [[FakeCredentialProviderSettingsHandler alloc] init];
    [handler registerWithCommandDispatcher:browser->GetCommandDispatcher()];
    settings_handlers_.push_back(handler);
    return handler;
  }

  // Inserts an active `FakeWebState` with `InfoBarManagerImpl` into `browser`
  // and sets its visibility.
  web::FakeWebState* InsertActiveWebState(Browser* browser, bool visible) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetCurrentURL(GURL(kPageUrl));
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web::FakeWebState* raw_web_state = web_state.get();
    InfoBarManagerImpl::CreateForWebState(raw_web_state);
    browser->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(true));
    if (visible) {
      raw_web_state->WasShown();
    } else {
      raw_web_state->WasHidden();
    }
    return raw_web_state;
  }

  // Writes `credentials` to the shared App Group temporal credential store and
  // waits until persistence completes.
  void SaveCredentialsToTemporalStore(
      NSArray<ArchivableCredential*>* credentials) {
    NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
    UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
        initWithUserDefaults:app_group::GetGroupUserDefaults()
                         key:key];
    for (ArchivableCredential* credential in credentials) {
      [store addCredential:credential];
    }
    base::test::TestFuture<NSError*> save_future;
    [store saveDataWithCompletion:base::CallbackToBlock(
                                      save_future.GetCallback())];
    ASSERT_FALSE(save_future.Get());
  }

  // Creates a passkey `ArchivableCredential` whose `creation_time` defaults to
  // `base::Time::Now()` (within the 5-second infobar freshness window) unless
  // explicitly overridden.
  ArchivableCredential* FreshPasskeyCredential(
      TestPasskeyCredentialOverrides overrides = {}) {
    if (overrides.creation_time == kDefaultCreationTime) {
      overrides.creation_time =
          base::Time::Now().InMillisecondsSinceUnixEpoch();
    }
    return TestPasskeyCredential(overrides);
  }

  void ClearAppGroupUserDefaults() {
    NSUserDefaults* user_defaults = app_group::GetGroupUserDefaults();
    [user_defaults removeObjectForKey:
                       AppGroupUserDefaultsCredentialProviderNewCredentials()];
    [user_defaults
        removeObjectForKey:
            AppGroupUserDefaultsCredentialProviderMultiProfileSetting()];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  raw_ptr<webauthn::TestPasskeyModel> passkey_model_ = nullptr;
  FakeSystemIdentity* identity_ = nil;
  FakeStartupInformation* startup_information_ = nil;
  AppState* app_state_ = nil;
  ProfileState* profile_state_ = nil;
  CredentialProviderMigratorAppAgent* app_agent_ = nil;
  std::vector<FakeSceneState*> scenes_;
  std::vector<FakeCredentialProviderSettingsHandler*> settings_handlers_;
  std::vector<ProfileState*> extra_profile_states_;
};

// Tests that migrating a newly created passkey on foreground entry presents the
// passkey creation infobar on the active scene and dispatches
// `SettingsCommands` when accepted.
TEST_F(CredentialProviderMigratorAppAgentTest,
       MigratingNewPasskeyShowsInfobarAndOpensDetailsOnAccept) {
  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/false);
  Browser* browser = scene.browserProviderInterface.mainBrowser;
  FakeCredentialProviderSettingsHandler* settings_handler =
      RegisterSettingsHandler(browser);
  web::FakeWebState* web_state =
      InsertActiveWebState(browser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  ArchivableCredential* credential = FreshPasskeyCredential(
      {.gaia = identity_.gaiaId.ToNSString(), .username = @"alice"});
  SaveCredentialsToTemporalStore(@[ credential ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return infobar_manager->infobars().size() == 1u; }));

  ConfirmInfoBarDelegate* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_SAVED),
      delegate->GetTitleText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                base::SysNSStringToUTF16(identity_.userEmail)),
            delegate->GetMessageText());

  EXPECT_TRUE(delegate->Accept());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return settings_handler.showPasswordDetailsCallCount == 1u; }));
  EXPECT_FALSE(settings_handler.calledInEditMode);
  ASSERT_TRUE(settings_handler.lastCredential.has_value());
  EXPECT_EQ(u"alice", settings_handler.lastCredential->username);
}

// Tests that when multiple passkeys are migrated in a single batch, only one
// infobar is presented and it corresponds to the passkey with the newest
// `creation_time`.
TEST_F(CredentialProviderMigratorAppAgentTest,
       MultipleNewPasskeysInBatchShowsOnlyNewestPasskeyInfobar) {
  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/false);
  Browser* browser = scene.browserProviderInterface.mainBrowser;
  FakeCredentialProviderSettingsHandler* settings_handler =
      RegisterSettingsHandler(browser);
  web::FakeWebState* web_state =
      InsertActiveWebState(browser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  const base::Time now = base::Time::Now();
  ArchivableCredential* older_credential = TestPasskeyCredential({
      .gaia = identity_.gaiaId.ToNSString(),
      .record_identifier = @"older_record",
      .sync_id = TestDataFromString("syncIdOlder_0016"),
      .username = @"older_user",
      .credential_id = TestDataFromString("credIdOlder_0016"),
      .creation_time = (now - base::Seconds(2)).InMillisecondsSinceUnixEpoch(),
  });
  ArchivableCredential* newer_credential = TestPasskeyCredential({
      .gaia = identity_.gaiaId.ToNSString(),
      .record_identifier = @"newer_record",
      .sync_id = TestDataFromString("syncIdNewer_0016"),
      .username = @"newer_user",
      .credential_id = TestDataFromString("credIdNewer_0016"),
      .creation_time = (now - base::Seconds(1)).InMillisecondsSinceUnixEpoch(),
  });
  SaveCredentialsToTemporalStore(@[ older_credential, newer_credential ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return infobar_manager->infobars().size() == 1u; }));

  ConfirmInfoBarDelegate* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_TRUE(delegate->Accept());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return settings_handler.showPasswordDetailsCallCount == 1u; }));
  ASSERT_TRUE(settings_handler.lastCredential.has_value());
  EXPECT_EQ(u"newer_user", settings_handler.lastCredential->username);
}

// Tests that in a multi-window configuration with both a foreground-inactive
// scene and a foreground-active scene, only the foreground-active scene
// receives the infobar.
TEST_F(CredentialProviderMigratorAppAgentTest,
       MultiWindowPrefersForegroundActiveSceneOverForegroundInactive) {
  FakeSceneState* inactive_scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* inactive_web_state = InsertActiveWebState(
      inactive_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* inactive_infobar_manager =
      InfoBarManagerImpl::FromWebState(inactive_web_state);

  FakeSceneState* active_scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* active_web_state = InsertActiveWebState(
      active_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* active_infobar_manager =
      InfoBarManagerImpl::FromWebState(active_web_state);

  SaveCredentialsToTemporalStore(
      @[ FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()}) ]);

  // Stage both scene activation levels while `app_state_` is below `kFinal` so
  // `SceneObservingAppAgent` does not fire `appDidEnterForeground` prematurely
  // when the first scene leaves background.
  [app_state_ updateInitStage:AppInitStage::kStart];
  inactive_scene.activationLevel = SceneActivationLevelForegroundInactive;
  active_scene.activationLevel = SceneActivationLevelForegroundActive;
  [app_state_ updateInitStage:AppInitStage::kFinal];
  [app_agent_ appDidEnterForeground];

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return active_infobar_manager->infobars().size() == 1u; }));
  EXPECT_TRUE(inactive_infobar_manager->infobars().empty());
}

// Tests that when the foreground-active scene has no visible `WebState` (e.g.,
// showing the Tab Grid), the infobar falls back to another foreground scene
// with a visible `WebState`.
TEST_F(CredentialProviderMigratorAppAgentTest,
       MultiWindowFallsBackToForegroundInactiveSceneWhenActiveWebStateHidden) {
  FakeSceneState* active_scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* hidden_active_web_state = InsertActiveWebState(
      active_scene.browserProviderInterface.mainBrowser, /*visible=*/false);
  InfoBarManagerImpl* active_infobar_manager =
      InfoBarManagerImpl::FromWebState(hidden_active_web_state);

  FakeSceneState* inactive_scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* visible_inactive_web_state = InsertActiveWebState(
      inactive_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* inactive_infobar_manager =
      InfoBarManagerImpl::FromWebState(visible_inactive_web_state);

  SaveCredentialsToTemporalStore(
      @[ FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()}) ]);

  [app_state_ updateInitStage:AppInitStage::kStart];
  active_scene.activationLevel = SceneActivationLevelForegroundActive;
  inactive_scene.activationLevel = SceneActivationLevelForegroundInactive;
  [app_state_ updateInitStage:AppInitStage::kFinal];
  [app_agent_ appDidEnterForeground];

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return inactive_infobar_manager->infobars().size() == 1u; }));
  EXPECT_TRUE(active_infobar_manager->infobars().empty());
}

// Tests that when the foreground scene's `currentBrowser` is Incognito, the
// migrated passkey infobar is displayed on the active Incognito `WebState`
// using the non-OTR profile's primary account email.
TEST_F(CredentialProviderMigratorAppAgentTest,
       IncognitoCurrentBrowserReceivesInfobarWithPrimaryAccountEmail) {
  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  Browser* incognito_browser = scene.browserProviderInterface.incognitoBrowser;
  [scene setCurrentBrowser:incognito_browser];
  web::FakeWebState* incognito_web_state =
      InsertActiveWebState(incognito_browser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(incognito_web_state);

  SaveCredentialsToTemporalStore(
      @[ FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()}) ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return infobar_manager->infobars().size() == 1u; }));

  ConfirmInfoBarDelegate* delegate =
      infobar_manager->infobars()[0]->delegate()->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_IOS_PASSWORD_MANAGER_ON_ACCOUNT_SAVE_SUBTITLE,
                base::SysNSStringToUTF16(identity_.userEmail)),
            delegate->GetMessageText());
}

// Tests that migrating an update to an already-existing passkey does not show
// the passkey creation infobar.
TEST_F(CredentialProviderMigratorAppAgentTest,
       PasskeyUpdateDoesNotShowInfobar) {
  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* web_state = InsertActiveWebState(
      scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  ArchivableCredential* updated_credential =
      FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()});
  passkey_model_->AddNewPasskeyForTesting(
      PasskeyFromCredential(updated_credential));
  updated_credential.lastUsedTime = kDefaultLastUsedTime + 100;

  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(managed_identity);
  system_identity_manager->WaitForServiceCallbacksToComplete();

  ArchivableCredential* managed_new_credential = FreshPasskeyCredential({
      .gaia = managed_identity.gaiaId.ToNSString(),
      .record_identifier = @"managed_record",
      .sync_id = TestDataFromString("syncIdManaged016"),
      .username = @"managed_user",
      .credential_id = TestDataFromString("credIdManaged016"),
  });
  SaveCredentialsToTemporalStore(
      @[ updated_credential, managed_new_credential ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;

  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
        initWithUserDefaults:app_group::GetGroupUserDefaults()
                         key:key];
    return store.credentials.count == 1u;
  }));

  TestProfileIOS* managed_profile =
      profile_manager_.AddManagedProfileForGaiaIDWithBuilder(
          managed_identity.gaiaId, CreateProfileBuilder());
  AuthenticationServiceFactory::GetForProfile(managed_profile)
      ->SignIn(managed_identity, signin_metrics::AccessPoint::kStartPage);

  ProfileState* managed_profile_state =
      [[ProfileState alloc] initWithAppState:app_state_];
  managed_profile_state.profile = managed_profile;
  [app_state_ profileStateCreated:managed_profile_state];
  extra_profile_states_.push_back(managed_profile_state);

  FakeSceneState* managed_scene =
      CreateConnectedSceneForProfile(managed_profile, managed_profile_state,
                                     /*register_settings_commands=*/true);
  web::FakeWebState* managed_web_state = InsertActiveWebState(
      managed_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* managed_infobar_manager =
      InfoBarManagerImpl::FromWebState(managed_web_state);
  managed_scene.activationLevel = SceneActivationLevelForegroundInactive;

  [app_agent_ appDidEnterForeground];

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return managed_infobar_manager->infobars().size() == 1u; }));

  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetPasskeys(
          webauthn::PasskeyModel::AnyRp(),
          webauthn::PasskeyModel::ShadowedCredentials::kInclude);
  ASSERT_EQ(1u, passkeys.size());
  EXPECT_EQ(updated_credential.lastUsedTime,
            passkeys[0].last_used_time_windows_epoch_micros());
  EXPECT_TRUE(infobar_manager->infobars().empty());
}

// Tests that migration is deferred while `PasskeyModel` is not ready and
// proceeds automatically to present the infobar once `PasskeyModel` becomes
// ready.
TEST_F(CredentialProviderMigratorAppAgentTest,
       DefersMigrationUntilPasskeyModelBecomesReady) {
  passkey_model_->SetReady(false);

  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* web_state = InsertActiveWebState(
      scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  SaveCredentialsToTemporalStore(
      @[ FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()}) ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_TRUE(
      passkey_model_
          ->GetPasskeys(webauthn::PasskeyModel::AnyRp(),
                        webauthn::PasskeyModel::ShadowedCredentials::kInclude)
          .empty());
  EXPECT_TRUE(infobar_manager->infobars().empty());

  passkey_model_->SetReady(true);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return infobar_manager->infobars().size() == 1u; }));
  EXPECT_EQ(1u, passkey_model_
                    ->GetPasskeys(
                        webauthn::PasskeyModel::AnyRp(),
                        webauthn::PasskeyModel::ShadowedCredentials::kInclude)
                    .size());
}

// Tests that if the foreground-active scene has not yet registered a
// `SettingsCommands` handler (e.g., mid-UI initialization), presentation
// safely skips it without crashing and falls back to a ready foreground scene.
TEST_F(CredentialProviderMigratorAppAgentTest,
       SkipsSceneWithoutSettingsCommandsDispatcher) {
  FakeSceneState* unready_active_scene =
      CreateConnectedScene(/*register_settings_commands=*/false);
  web::FakeWebState* unready_web_state = InsertActiveWebState(
      unready_active_scene.browserProviderInterface.mainBrowser,
      /*visible=*/true);
  InfoBarManagerImpl* unready_infobar_manager =
      InfoBarManagerImpl::FromWebState(unready_web_state);

  FakeSceneState* ready_inactive_scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* ready_web_state = InsertActiveWebState(
      ready_inactive_scene.browserProviderInterface.mainBrowser,
      /*visible=*/true);
  InfoBarManagerImpl* ready_infobar_manager =
      InfoBarManagerImpl::FromWebState(ready_web_state);

  SaveCredentialsToTemporalStore(
      @[ FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()}) ]);

  [app_state_ updateInitStage:AppInitStage::kStart];
  unready_active_scene.activationLevel = SceneActivationLevelForegroundActive;
  ready_inactive_scene.activationLevel = SceneActivationLevelForegroundInactive;
  [app_state_ updateInitStage:AppInitStage::kFinal];
  [app_agent_ appDidEnterForeground];

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return ready_infobar_manager->infobars().size() == 1u; }));
  EXPECT_TRUE(unready_infobar_manager->infobars().empty());
}

// Tests that if the primary account signs out while migration persistence is
// in flight, no passkey creation infobar is presented when completion runs.
TEST_F(CredentialProviderMigratorAppAgentTest,
       SigningOutInFlightSuppressesInfobar) {
  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* web_state = InsertActiveWebState(
      scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(managed_identity);
  system_identity_manager->WaitForServiceCallbacksToComplete();

  ArchivableCredential* personal_credential =
      FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()});
  ArchivableCredential* managed_credential = FreshPasskeyCredential({
      .gaia = managed_identity.gaiaId.ToNSString(),
      .record_identifier = @"managed_record",
      .sync_id = TestDataFromString("syncIdManaged016"),
      .credential_id = TestDataFromString("credIdManaged016"),
  });
  SaveCredentialsToTemporalStore(@[ personal_credential, managed_credential ]);

  // Start `profile_` migration synchronously (`passkey_model_` receives the
  // passkey and `saveDataWithCompletion:` runs asynchronously), then sign out
  // `profile_` before the main-queue completion executes. Then add a second
  // profile whose store pass is serialized behind `profile_`'s save, so its
  // infobar signals that `profile_`'s migration has finished.
  scene.activationLevel = SceneActivationLevelForegroundActive;
  AuthenticationServiceFactory::GetForProfile(profile_)->SignOut(
      signin_metrics::ProfileSignout::kTest);

  TestProfileIOS* managed_profile =
      profile_manager_.AddManagedProfileForGaiaIDWithBuilder(
          managed_identity.gaiaId, CreateProfileBuilder());
  AuthenticationServiceFactory::GetForProfile(managed_profile)
      ->SignIn(managed_identity, signin_metrics::AccessPoint::kStartPage);

  ProfileState* managed_profile_state =
      [[ProfileState alloc] initWithAppState:app_state_];
  managed_profile_state.profile = managed_profile;
  [app_state_ profileStateCreated:managed_profile_state];
  extra_profile_states_.push_back(managed_profile_state);

  FakeSceneState* managed_scene =
      CreateConnectedSceneForProfile(managed_profile, managed_profile_state,
                                     /*register_settings_commands=*/true);
  web::FakeWebState* managed_web_state = InsertActiveWebState(
      managed_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* managed_infobar_manager =
      InfoBarManagerImpl::FromWebState(managed_web_state);
  managed_scene.activationLevel = SceneActivationLevelForegroundInactive;
  [app_agent_ appDidEnterForeground];

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return managed_infobar_manager->infobars().size() == 1u; }));
  EXPECT_EQ(1u, passkey_model_
                    ->GetPasskeys(
                        webauthn::PasskeyModel::AnyRp(),
                        webauthn::PasskeyModel::ShadowedCredentials::kInclude)
                    .size());
  EXPECT_TRUE(infobar_manager->infobars().empty());
}

// Tests that if a migrated passkey's `creation_time` is older than 5 seconds,
// the passkey is still imported into `PasskeyModel` while its confirmation
// infobar is suppressed.
TEST_F(CredentialProviderMigratorAppAgentTest,
       StaleMigratedPasskeySuppressesInfobar) {
  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* web_state = InsertActiveWebState(
      scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(managed_identity);
  system_identity_manager->WaitForServiceCallbacksToComplete();

  ArchivableCredential* stale_credential = TestPasskeyCredential({
      .gaia = identity_.gaiaId.ToNSString(),
      .creation_time = (base::Time::Now() - base::Seconds(10))
                           .InMillisecondsSinceUnixEpoch(),
  });
  ArchivableCredential* managed_fresh_credential = FreshPasskeyCredential({
      .gaia = managed_identity.gaiaId.ToNSString(),
      .record_identifier = @"managed_record",
      .sync_id = TestDataFromString("syncIdManaged016"),
      .credential_id = TestDataFromString("credIdManaged016"),
  });
  SaveCredentialsToTemporalStore(
      @[ stale_credential, managed_fresh_credential ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;

  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
        initWithUserDefaults:app_group::GetGroupUserDefaults()
                         key:key];
    return store.credentials.count == 1u;
  }));

  TestProfileIOS* managed_profile =
      profile_manager_.AddManagedProfileForGaiaIDWithBuilder(
          managed_identity.gaiaId, CreateProfileBuilder());
  AuthenticationServiceFactory::GetForProfile(managed_profile)
      ->SignIn(managed_identity, signin_metrics::AccessPoint::kStartPage);

  ProfileState* managed_profile_state =
      [[ProfileState alloc] initWithAppState:app_state_];
  managed_profile_state.profile = managed_profile;
  [app_state_ profileStateCreated:managed_profile_state];
  extra_profile_states_.push_back(managed_profile_state);

  FakeSceneState* managed_scene =
      CreateConnectedSceneForProfile(managed_profile, managed_profile_state,
                                     /*register_settings_commands=*/true);
  web::FakeWebState* managed_web_state = InsertActiveWebState(
      managed_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* managed_infobar_manager =
      InfoBarManagerImpl::FromWebState(managed_web_state);
  managed_scene.activationLevel = SceneActivationLevelForegroundInactive;
  [app_agent_ appDidEnterForeground];

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return managed_infobar_manager->infobars().size() == 1u; }));
  EXPECT_EQ(1u, passkey_model_
                    ->GetPasskeys(
                        webauthn::PasskeyModel::AnyRp(),
                        webauthn::PasskeyModel::ShadowedCredentials::kInclude)
                    .size());
  EXPECT_TRUE(infobar_manager->infobars().empty());
}

// Tests that when `PasskeyModel` is unready and then fails initialization on
// one profile, its passwords (including new passwords added during the wait)
// still migrate immediately, other profiles migrate without head-of-line
// blocking, and initialization failure unwinds cleanly.
TEST_F(CredentialProviderMigratorAppAgentTest,
       PasskeyModelInitializationFailureMigratesPasswordsWithoutBlocking) {
  passkey_model_->SetReady(false);

  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* web_state = InsertActiveWebState(
      scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentity(managed_identity);
  system_identity_manager->WaitForServiceCallbacksToComplete();

  TestProfileIOS* managed_profile =
      profile_manager_.AddManagedProfileForGaiaIDWithBuilder(
          managed_identity.gaiaId, CreateProfileBuilder());
  AuthenticationServiceFactory::GetForProfile(managed_profile)
      ->SignIn(managed_identity, signin_metrics::AccessPoint::kStartPage);

  ProfileState* managed_profile_state =
      [[ProfileState alloc] initWithAppState:app_state_];
  managed_profile_state.profile = managed_profile;
  [app_state_ profileStateCreated:managed_profile_state];
  extra_profile_states_.push_back(managed_profile_state);

  FakeSceneState* managed_scene =
      CreateConnectedSceneForProfile(managed_profile, managed_profile_state,
                                     /*register_settings_commands=*/true);
  web::FakeWebState* managed_web_state = InsertActiveWebState(
      managed_scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* managed_infobar_manager =
      InfoBarManagerImpl::FromWebState(managed_web_state);

  ArchivableCredential* personal_password = [[ArchivableCredential alloc]
               initWithFavicon:nil
                          gaia:identity_.gaiaId.ToNSString()
                      password:@"secret_password"
                          rank:1
              recordIdentifier:@"personal_password_record"
             serviceIdentifier:@"https://example.com/login"
                   serviceName:nil
      registryControlledDomain:nil
                      username:@"alice_pw"
                          note:nil
                  lastUsedTime:0];
  ArchivableCredential* personal_passkey = FreshPasskeyCredential({
      .gaia = identity_.gaiaId.ToNSString(),
      .record_identifier = @"personal_passkey_record",
  });
  ArchivableCredential* managed_passkey = FreshPasskeyCredential({
      .gaia = managed_identity.gaiaId.ToNSString(),
      .record_identifier = @"managed_passkey_record",
      .sync_id = TestDataFromString("syncIdManaged016"),
      .username = @"managed_user",
      .credential_id = TestDataFromString("credIdManaged016"),
  });
  SaveCredentialsToTemporalStore(
      @[ personal_password, personal_passkey, managed_passkey ]);

  [app_state_ updateInitStage:AppInitStage::kStart];
  scene.activationLevel = SceneActivationLevelForegroundActive;
  managed_scene.activationLevel = SceneActivationLevelForegroundInactive;
  [app_state_ updateInitStage:AppInitStage::kFinal];
  [app_agent_ appDidEnterForeground];

  // Even while `profile_`'s `passkey_model_` is unready, `managed_profile`
  // completes migration without head-of-line blocking and `personal_password`
  // is migrated immediately to the account password store. Wait for both
  // profiles' serialized `NSUserDefaults` saves to finish regardless of random
  // profile UUID iteration order.
  NSString* key = AppGroupUserDefaultsCredentialProviderNewCredentials();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
        initWithUserDefaults:app_group::GetGroupUserDefaults()
                         key:key];
    return managed_infobar_manager->infobars().size() == 1u &&
           store.credentials.count == 1u;
  }));
  EXPECT_TRUE(infobar_manager->infobars().empty());

  scoped_refptr<password_manager::PasswordStoreInterface>
      personal_account_store =
          IOSChromeAccountPasswordStoreFactory::GetForProfile(
              profile_, ServiceAccessType::IMPLICIT_ACCESS);
  ASSERT_TRUE(personal_account_store);
  password_manager::TestPasswordStore::PasswordMap stored_logins =
      password_manager::GetAllLoginsSync(personal_account_store.get());
  ASSERT_EQ(1u, stored_logins.size());
  const std::vector<password_manager::PasswordForm>& realm_forms =
      stored_logins.begin()->second;
  ASSERT_EQ(1u, realm_forms.size());
  EXPECT_EQ(u"alice_pw", realm_forms[0].username_value);
  EXPECT_EQ(u"secret_password", realm_forms[0].password_value);

  // Add a second password while `profile_` is still awaiting `passkey_model_`
  // readiness and verify re-foregrounding migrates it via
  // `-checkForNewCredentials`.
  UserDefaultsCredentialStore* cpe_store = [[UserDefaultsCredentialStore alloc]
      initWithUserDefaults:app_group::GetGroupUserDefaults()
                       key:key];
  ArchivableCredential* followup_password = [[ArchivableCredential alloc]
               initWithFavicon:nil
                          gaia:identity_.gaiaId.ToNSString()
                      password:@"followup_secret"
                          rank:1
              recordIdentifier:@"followup_password_record"
             serviceIdentifier:@"https://example.com/login"
                   serviceName:nil
      registryControlledDomain:nil
                      username:@"alice_pw_2"
                          note:nil
                  lastUsedTime:0];
  [cpe_store addCredential:followup_password];
  base::test::TestFuture<NSError*> cpe_save_future;
  [cpe_store saveDataWithCompletion:base::CallbackToBlock(
                                        cpe_save_future.GetCallback())];
  ASSERT_FALSE(cpe_save_future.Get());

  [app_agent_ appDidEnterForeground];

  // Wait for the follow-up password removal to finish persisting to
  // `NSUserDefaults` across both profiles' serialized store passes before
  // inspecting logins or signalling `PasskeyModel` initialization failure.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    UserDefaultsCredentialStore* store = [[UserDefaultsCredentialStore alloc]
        initWithUserDefaults:app_group::GetGroupUserDefaults()
                         key:key];
    return store.credentials.count == 1u;
  }));

  stored_logins =
      password_manager::GetAllLoginsSync(personal_account_store.get());
  ASSERT_EQ(1u, stored_logins.size());
  ASSERT_EQ(2u, stored_logins.begin()->second.size());

  // Signal `PasskeyModel` initialization failure on `profile_`; `profile_`
  // unwinds cleanly while preserving `personal_passkey` in `NSUserDefaults`, so
  // a subsequent foreground pass after `passkey_model_` becomes ready imports
  // `personal_passkey` and displays the infobar.
  passkey_model_->SetReady(false);
  EXPECT_TRUE(infobar_manager->infobars().empty());

  UserDefaultsCredentialStore* remaining_store =
      [[UserDefaultsCredentialStore alloc]
          initWithUserDefaults:app_group::GetGroupUserDefaults()
                           key:key];
  ASSERT_EQ(1u, remaining_store.credentials.count);
  EXPECT_NSEQ(@"personal_passkey_record",
              remaining_store.credentials[0].recordIdentifier);

  passkey_model_->SetReady(true);
  [app_agent_ appDidEnterForeground];
  // If `profile_`'s main-queue GCD save completion from the previous pass had
  // not yet run when `appDidEnterForeground` fired, retry at a spaced interval
  // rather than on every RunLoop spin so `saveDataWithCompletion:` is not
  // continuously re-triggered.
  base::RepeatingTimer retry_timer;
  retry_timer.Start(FROM_HERE, base::Milliseconds(20),
                    base::BindRepeating(
                        [](CredentialProviderMigratorAppAgent* agent) {
                          [agent appDidEnterForeground];
                        },
                        app_agent_));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return infobar_manager->infobars().size() == 1u; }));
}

// Tests that entering the background cancels any deferred `PasskeyModel` wait
// so passkeys are not imported while backgrounded (avoiding cross-process
// `NSUserDefaults` races and preserving the foreground infobar for the next
// foreground entry).
TEST_F(CredentialProviderMigratorAppAgentTest,
       BackgroundingCancelsDeferredPasskeyWaitUntilNextForeground) {
  passkey_model_->SetReady(false);

  FakeSceneState* scene =
      CreateConnectedScene(/*register_settings_commands=*/true);
  web::FakeWebState* web_state = InsertActiveWebState(
      scene.browserProviderInterface.mainBrowser, /*visible=*/true);
  InfoBarManagerImpl* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  SaveCredentialsToTemporalStore(
      @[ FreshPasskeyCredential({.gaia = identity_.gaiaId.ToNSString()}) ]);

  scene.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_TRUE(infobar_manager->infobars().empty());

  scene.activationLevel = SceneActivationLevelBackground;
  passkey_model_->SetReady(true);

  EXPECT_TRUE(
      passkey_model_
          ->GetPasskeys(webauthn::PasskeyModel::AnyRp(),
                        webauthn::PasskeyModel::ShadowedCredentials::kInclude)
          .empty());
  EXPECT_TRUE(infobar_manager->infobars().empty());

  scene.activationLevel = SceneActivationLevelForegroundActive;
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return infobar_manager->infobars().size() == 1u; }));
}

}  // namespace

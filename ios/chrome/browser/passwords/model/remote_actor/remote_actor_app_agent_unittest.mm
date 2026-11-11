// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_app_agent.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "components/signin/public/base/signin_metrics.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/mutable_profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_error_controller_factory.h"
#import "ios/chrome/browser/signin/model/signin_profile_info_updater.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class RemoteActorAppAgentTest : public PlatformTest {
 protected:
  RemoteActorAppAgentTest() {
    app_state_ = [[AppState alloc] initWithStartupInformation:nil];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_);
    signin_profile_info_updater_ = std::make_unique<SigninProfileInfoUpdater>(
        IdentityManagerFactory::GetForProfile(profile_),
        ios::SigninErrorControllerFactory::GetForProfile(profile_),
        profile_->GetProfileName());
  }

  void TearDown() override {
    [app_group::GetCommonGroupUserDefaults()
        removeObjectForKey:app_group::kChromeActiveGaiaIDHashesPreference];
    if (agent_) {
      [app_state_ removeAgent:agent_];
      agent_ = nil;
    }
    app_state_ = nil;
    signin_profile_info_updater_.reset();
    authentication_service_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  void AdvanceAppInitStageTo(AppInitStage target_stage) {
    if (app_state_.initStage == AppInitStage::kStart) {
      [app_state_ startInitialization];
    }
    while (app_state_.initStage < target_stage) {
      [app_state_ queueTransitionToNextInitStage];
    }
  }

  FakeSystemIdentity* AddAndSignInFakeIdentity(
      FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1]) {
    FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager())
        ->AddIdentity(identity);
    authentication_service_->SignIn(identity,
                                    signin_metrics::AccessPoint::kStartPage);
    return identity;
  }

  void RegisterUnloadedProfileWithGaiaId(const std::string& profile_name,
                                         const GaiaId& gaia_id) {
    MutableProfileAttributesStorageIOS* storage =
        static_cast<MutableProfileAttributesStorageIOS*>(
            profile_manager_.GetProfileAttributesStorage());
    storage->AddProfile(profile_name);
    storage->UpdateAttributesForProfileWithName(
        profile_name, base::BindOnce(
                          [](GaiaId id, ProfileAttributesIOS& attr) {
                            attr.SetAuthenticationInfo(id, "user@example.com");
                          },
                          gaia_id));
  }

  NSString* ExpectedGaiaIDHash(const GaiaId& gaia_id) {
    return base::SysUTF8ToNSString(
        signin::GaiaIdHash::FromGaiaId(gaia_id).ToBase64());
  }

  NSArray<NSString*>* StoredGaiaIDHashes() {
    return [app_group::GetCommonGroupUserDefaults()
        stringArrayForKey:app_group::kChromeActiveGaiaIDHashesPreference];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  AppState* app_state_;
  raw_ptr<ProfileIOS> profile_;
  raw_ptr<AuthenticationService> authentication_service_;
  std::unique_ptr<SigninProfileInfoUpdater> signin_profile_info_updater_;
  RemoteActorAppAgent* agent_;
};

// Tests that signed-in profiles' GAIA ID hashes are written once app init
// reaches kBrowserObjectsForBackgroundHandlers.
TEST_F(RemoteActorAppAgentTest, SyncsSignedInProfilesWhenBrowserObjectsReady) {
  FakeSystemIdentity* identity = AddAndSignInFakeIdentity();

  AdvanceAppInitStageTo(AppInitStage::kBrowserBasic);
  agent_ = [[RemoteActorAppAgent alloc] init];
  [app_state_ addAgent:agent_];
  EXPECT_EQ(nil, StoredGaiaIDHashes());

  AdvanceAppInitStageTo(AppInitStage::kBrowserObjectsForBackgroundHandlers);
  EXPECT_NSEQ(StoredGaiaIDHashes(), @[ ExpectedGaiaIDHash(identity.gaiaId) ]);
}

// Tests that signing in and signing out updates the shared defaults.
TEST_F(RemoteActorAppAgentTest, UpdatesGaiaIDHashOnSignInAndSignOut) {
  [app_group::GetCommonGroupUserDefaults()
      setObject:@[ @"stale_gaia_id_hash" ]
         forKey:app_group::kChromeActiveGaiaIDHashesPreference];

  AdvanceAppInitStageTo(AppInitStage::kBrowserObjectsForBackgroundHandlers);
  agent_ = [[RemoteActorAppAgent alloc] init];
  [app_state_ addAgent:agent_];
  EXPECT_EQ(nil, StoredGaiaIDHashes());

  FakeSystemIdentity* identity = AddAndSignInFakeIdentity();
  EXPECT_NSEQ(StoredGaiaIDHashes(), @[ ExpectedGaiaIDHash(identity.gaiaId) ]);

  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest);
  EXPECT_EQ(nil, StoredGaiaIDHashes());
}

// Tests that all signed-in profiles (including unloaded profiles) are
// advertised without duplicates across multiple profiles.
TEST_F(RemoteActorAppAgentTest, AdvertisesAllSignedInProfilesAcrossStorage) {
  AdvanceAppInitStageTo(AppInitStage::kBrowserObjectsForBackgroundHandlers);
  agent_ = [[RemoteActorAppAgent alloc] init];
  [app_state_ addAgent:agent_];

  FakeSystemIdentity* identity1 = AddAndSignInFakeIdentity();
  GaiaId gaia_id_2("second-profile-gaia-id");
  RegisterUnloadedProfileWithGaiaId("second_profile", gaia_id_2);

  NSSet<NSString*>* expected_both = [NSSet setWithArray:@[
    ExpectedGaiaIDHash(identity1.gaiaId),
    ExpectedGaiaIDHash(gaia_id_2),
  ]];
  EXPECT_EQ(StoredGaiaIDHashes().count, 2u);
  EXPECT_NSEQ([NSSet setWithArray:StoredGaiaIDHashes()], expected_both);

  // Signing out the second profile updates the list to only the first profile.
  profile_manager_.GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          "second_profile", base::BindOnce([](ProfileAttributesIOS& attr) {
            attr.SetAuthenticationInfo(GaiaId(), std::string_view());
          }));
  EXPECT_NSEQ(StoredGaiaIDHashes(), @[ ExpectedGaiaIDHash(identity1.gaiaId) ]);
}

// Tests that deleting a profile removes its GAIA ID hash immediately while
// preserving remaining signed-in profiles.
TEST_F(RemoteActorAppAgentTest, RemovesHashWhenProfileMarkedForDeletion) {
  FakeSystemIdentity* identity1 = AddAndSignInFakeIdentity();
  GaiaId gaia_id_2("deleted-profile-gaia-id");
  RegisterUnloadedProfileWithGaiaId("deleted_profile", gaia_id_2);

  AdvanceAppInitStageTo(AppInitStage::kBrowserObjectsForBackgroundHandlers);
  agent_ = [[RemoteActorAppAgent alloc] init];
  [app_state_ addAgent:agent_];

  NSSet<NSString*>* expected_both = [NSSet setWithArray:@[
    ExpectedGaiaIDHash(identity1.gaiaId),
    ExpectedGaiaIDHash(gaia_id_2),
  ]];
  EXPECT_EQ(StoredGaiaIDHashes().count, 2u);
  EXPECT_NSEQ([NSSet setWithArray:StoredGaiaIDHashes()], expected_both);

  profile_manager_.MarkProfileForDeletion("deleted_profile");
  EXPECT_NSEQ(StoredGaiaIDHashes(), @[ ExpectedGaiaIDHash(identity1.gaiaId) ]);
}

// Tests that stored GAIA ID hashes persist when the profile manager is torn
// down (simulating app termination).
TEST_F(RemoteActorAppAgentTest, PreservesHashesOnShutdown) {
  FakeSystemIdentity* identity = AddAndSignInFakeIdentity();
  AdvanceAppInitStageTo(AppInitStage::kBrowserObjectsForBackgroundHandlers);
  agent_ = [[RemoteActorAppAgent alloc] init];
  [app_state_ addAgent:agent_];

  NSArray<NSString*>* expected = @[ ExpectedGaiaIDHash(identity.gaiaId) ];
  EXPECT_NSEQ(StoredGaiaIDHashes(), expected);

  signin_profile_info_updater_.reset();
  profile_manager_.PrepareForDestruction();
  EXPECT_NSEQ(StoredGaiaIDHashes(), expected);
}

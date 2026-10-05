// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "net/base/url_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

base::DictValue CreateSessionPrefDict(std::string_view server_id,
                                      std::string_view turn_id,
                                      std::string_view query = "") {
  base::DictValue dict;
  dict.Set("mtid", server_id);
  if (!turn_id.empty()) {
    dict.Set("mstk", turn_id);
  }
  if (!query.empty()) {
    dict.Set("q", query);
  }
  return dict;
}

}  // namespace

class CobrowseBrowserAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.SetName(profile_manager_.ReserveNewProfileName());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindOnce([](ProfileIOS* profile)
                           -> std::unique_ptr<KeyedService> {
          auto service =
              MockIOSChromeAimEligibilityService::CreateTestingProfileService(
                  profile);
          ON_CALL(*service, IsFuseboxEligible())
              .WillByDefault(testing::Return(true));
          ON_CALL(*service, IsCobrowseEligible())
              .WillByDefault(testing::Return(true));
          return service;
        }));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.sceneSessionID = "test_session_id";
    scene_state_.prefs = [[SceneStatePrefs alloc]
        initWithProfileManager:&profile_manager_
                   profileName:profile_->GetProfileName()
             sessionIdentifier:scene_state_.sceneSessionID
                  sceneSession:nil];

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    scoped_feature_list_.InitWithFeatures({kAimCobrowse, kAssistantContainer},
                                          {});
  }

  void TearDown() override {
    [scene_state_ shutdown];
    scene_state_.prefs = nil;
    scene_state_ = nil;
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeSceneState* scene_state_;
};

// Test that `CobrowseBrowserAgent` restores the conversation URL with `q`,
// `mstk`, and `mtid` from prefs.
TEST_F(CobrowseBrowserAgentTest, RestoresContextFromPrefs) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCobrowseSessionActiveMap);
  update->Set(
      "test_session_id",
      CreateSessionPrefDict("my_server_id_123", "my_turn_id_456", "my query"));

  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_TRUE(agent->IsSessionActive());

  CobrowseContext* context = agent->GetCobrowseContext();
  ASSERT_TRUE(context != nil);
  EXPECT_NSEQ(context.searchQuery, @"my query");
  EXPECT_NSEQ(context.serverID, @"my_server_id_123");
  EXPECT_NSEQ(context.turnID, @"my_turn_id_456");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "q", &value));
  EXPECT_EQ(value, "my query");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "mstk", &value));
  EXPECT_EQ(value, "my_turn_id_456");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "mtid", &value));
  EXPECT_EQ(value, "my_server_id_123");
}

// Test that `CobrowseBrowserAgent` restores the conversation URL when `q` is
// absent from prefs.
TEST_F(CobrowseBrowserAgentTest, RestoresContextFromPrefsWithoutQuery) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCobrowseSessionActiveMap);
  update->Set("test_session_id",
              CreateSessionPrefDict("my_server_id_123", "my_turn_id"));

  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_TRUE(agent->IsSessionActive());

  CobrowseContext* context = agent->GetCobrowseContext();
  ASSERT_TRUE(context != nil);
  EXPECT_NSEQ(context.searchQuery, @"");
  EXPECT_NSEQ(context.serverID, @"my_server_id_123");
  EXPECT_NSEQ(context.turnID, @"my_turn_id");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "q", &value));
  EXPECT_EQ(value, "");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "mstk", &value));
  EXPECT_EQ(value, "my_turn_id");
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "mtid", &value));
  EXPECT_EQ(value, "my_server_id_123");
}

// Test that loading a conversation stores `q`, `mtid`, and `mstk` into prefs.
TEST_F(CobrowseBrowserAgentTest, StoresQueryServerIDAndTurnIDInPrefs) {
  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  agent->SetSessionActive(true);
  GURL loaded_url("https://www.google.com/"
                  "search?udm=50&q=hello&mtid=server_99&mstk=turn_88");
  CobrowseContext* loaded_context =
      [[CobrowseContext alloc] initWithURL:loaded_url];
  agent->SetCobrowseContext(loaded_context);

  const base::DictValue& active_map =
      profile_->GetPrefs()->GetDict(prefs::kCobrowseSessionActiveMap);
  const base::DictValue* session_dict = active_map.FindDict("test_session_id");
  ASSERT_NE(session_dict, nullptr);
  const std::string* stored_query = session_dict->FindString("q");
  const std::string* stored_mtid = session_dict->FindString("mtid");
  const std::string* stored_mstk = session_dict->FindString("mstk");
  ASSERT_NE(stored_query, nullptr);
  ASSERT_NE(stored_mtid, nullptr);
  ASSERT_NE(stored_mstk, nullptr);
  EXPECT_EQ(*stored_query, "hello");
  EXPECT_EQ(*stored_mtid, "server_99");
  EXPECT_EQ(*stored_mstk, "turn_88");
}

TEST_F(CobrowseBrowserAgentTest, NoActiveSessionInPrefs) {
  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_FALSE(agent->IsSessionActive());
  EXPECT_EQ(agent->GetCobrowseContext(), nil);
}

// Tests that the cobrowse session is not restored if the start surface should
// be shown.
TEST_F(CobrowseBrowserAgentTest,
       DoesNotRestoreContextWhenStartSurfaceShouldBeShown) {
  test::SetStartSurfaceSessionObjectForSceneStateForTesting(
      scene_state_, base::Time::Now() - base::Hours(8));

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCobrowseSessionActiveMap);
  update->Set("test_session_id",
              CreateSessionPrefDict("my_server_id_123", "my_turn_id_456"));

  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_FALSE(agent->IsSessionActive());
  EXPECT_EQ(agent->GetCobrowseContext(), nil);
  EXPECT_FALSE(profile_->GetPrefs()
                   ->GetDict(prefs::kCobrowseSessionActiveMap)
                   .contains("test_session_id"));
}

// Tests that TerminateSession clears the active session and context.
TEST_F(CobrowseBrowserAgentTest, TerminateSessionClearsStateAndContext) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCobrowseSessionActiveMap);
  update->Set("test_session_id",
              CreateSessionPrefDict("my_server_id_123", "my_turn_id_456"));

  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_TRUE(agent->IsSessionActive());
  EXPECT_NE(agent->GetCobrowseContext(), nil);

  agent->TerminateSession();

  EXPECT_FALSE(agent->IsSessionActive());
  EXPECT_EQ(agent->GetCobrowseContext(), nil);
  EXPECT_FALSE(profile_->GetPrefs()
                   ->GetDict(prefs::kCobrowseSessionActiveMap)
                   .contains("test_session_id"));
}

// Tests that transitioning to foreground active terminates the active session
// when the start surface should be shown.
TEST_F(CobrowseBrowserAgentTest,
       TerminatesSessionOnForegroundActiveWhenStartSurfaceShouldBeShown) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCobrowseSessionActiveMap);
  update->Set("test_session_id",
              CreateSessionPrefDict("my_server_id_123", "my_turn_id_456"));

  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_TRUE(agent->IsSessionActive());
  EXPECT_NE(agent->GetCobrowseContext(), nil);

  test::SetStartSurfaceSessionObjectForSceneStateForTesting(
      scene_state_, base::Time::Now() - base::Hours(8));

  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(agent->IsSessionActive());
  EXPECT_EQ(agent->GetCobrowseContext(), nil);
  EXPECT_FALSE(profile_->GetPrefs()
                   ->GetDict(prefs::kCobrowseSessionActiveMap)
                   .contains("test_session_id"));
}

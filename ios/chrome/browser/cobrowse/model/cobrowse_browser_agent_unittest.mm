// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class CobrowseBrowserAgentTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
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
    profile_ = std::move(builder).Build();

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.sceneSessionID = "test_session_id";

    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    scoped_feature_list_.InitWithFeatures({kAimCobrowse, kAssistantContainer},
                                          {});
  }

  void TearDown() override {
    [scene_state_ shutdown];
    PlatformTest::TearDown();
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeSceneState* scene_state_;
};

TEST_F(CobrowseBrowserAgentTest, RestoresContextFromPrefs) {
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              prefs::kCobrowseSessionActiveMap);
  update->Set("test_session_id", "my_server_id_123");

  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_TRUE(agent->IsSessionActive());

  CobrowseContext* context = agent->GetCobrowseContext();
  ASSERT_TRUE(context != nil);
  EXPECT_TRUE([context.serverID isEqualToString:@"my_server_id_123"]);
}

TEST_F(CobrowseBrowserAgentTest, NoActiveSessionInPrefs) {
  CobrowseBrowserAgent::CreateForBrowser(browser_.get());
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());

  EXPECT_FALSE(agent->IsSessionActive());
  EXPECT_EQ(agent->GetCobrowseContext(), nil);
}

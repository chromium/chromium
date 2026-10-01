// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_with/ui_bundled/search_with_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/search_engines/template_url_data.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/url_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface SearchWithMediator (Testing)
- (NSString*)buttonTitle;
- (void)executeSearchForText:(NSString*)text
    allowedByDataControlsRulesPolicy:(BOOL)allowedByDataControlsRulesPolicy;
@end

namespace {

const char kGoogleSearchTemplate[] = "https://www.google.com/?q={searchTerms}";
const char kNonGoogleSearchTemplate[] =
    "https://www.example.com/?q={searchTerms}";

std::unique_ptr<KeyedService> BuildMockIOSChromeAimEligibilityService(
    ProfileIOS* profile) {
  return MockIOSChromeAimEligibilityService::CreateTestingProfileService(
      profile);
}

}  // namespace

class SearchWithMediatorTest : public PlatformTest {
 public:
  SearchWithMediatorTest() {
    scoped_feature_list_.InitWithFeatures(
        {kAimCobrowse, kAssistantContainer, kAimCobrowseWebSelectionSearch},
        {});
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockIOSChromeAimEligibilityService));
    profile_ = std::move(builder).Build();
    scene_state_ = [[SceneState alloc] init];
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    CobrowseBrowserAgent::CreateForBrowser(browser_.get());

    mock_scene_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_handler_
                     forProtocol:@protocol(SceneCommands)];

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    SetDefaultSearchEngineGoogle();
  }

  void SetDefaultSearchEngineGoogle() {
    TemplateURLData template_url_data;
    template_url_data.SetURL(kGoogleSearchTemplate);
    template_url_data.SetShortName(u"Google");
    template_url_service_->ApplyDefaultSearchChangeForTesting(
        &template_url_data, DefaultSearchManager::FROM_USER);
  }

  void SetDefaultSearchEngineNonGoogle() {
    TemplateURLData template_url_data;
    template_url_data.SetURL(kNonGoogleSearchTemplate);
    template_url_data.SetShortName(u"Example");
    template_url_service_->ApplyDefaultSearchChangeForTesting(
        &template_url_data, DefaultSearchManager::FROM_USER);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  SceneState* scene_state_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  id mock_scene_handler_;
};

// Tests that when eligible and default engine is Google, search with triggers
// Cobrowse.
TEST_F(SearchWithMediatorTest, ExecutesSearchWithCobrowseWhenEligible) {
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  SearchWithMediator* mediator = [[SearchWithMediator alloc]
      initWithTemplateURLService:template_url_service_
           aimEligibilityService:aim_eligibility_service
            cobrowseBrowserAgent:agent
                       incognito:NO];
  mediator.sceneHandler = mock_scene_handler_;

  OCMExpect([mock_scene_handler_ showAssistantWithNewSession]);
  [[mock_scene_handler_ reject] openURLInNewTab:[OCMArg any]];

  [mediator executeSearchForText:@"hello world"
      allowedByDataControlsRulesPolicy:YES];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
  EXPECT_TRUE(agent->IsSessionActive());
  CobrowseContext* context = agent->GetCobrowseContext();
  ASSERT_NE(context, nil);
  EXPECT_NSEQ(context.searchQuery, @"hello world");
  std::string udmValue;
  EXPECT_TRUE(net::GetValueForKeyInQuery(context.url, "udm", &udmValue));
  EXPECT_EQ(udmValue, "50");

  [mediator shutdown];
}

// Tests that when default engine is non-Google, search with opens a standard
// new tab.
TEST_F(SearchWithMediatorTest, ExecutesSearchWithStandardNewTabWhenNonGoogle) {
  SetDefaultSearchEngineNonGoogle();
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  SearchWithMediator* mediator = [[SearchWithMediator alloc]
      initWithTemplateURLService:template_url_service_
           aimEligibilityService:aim_eligibility_service
            cobrowseBrowserAgent:agent
                       incognito:NO];
  mediator.sceneHandler = mock_scene_handler_;

  [[mock_scene_handler_ reject] showAssistantWithNewSession];
  OCMExpect([mock_scene_handler_ openURLInNewTab:[OCMArg any]]);

  [mediator executeSearchForText:@"hello world"
      allowedByDataControlsRulesPolicy:YES];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
  EXPECT_FALSE(agent->IsSessionActive());

  [mediator shutdown];
}

// Tests that in incognito mode, search with opens a standard new tab.
TEST_F(SearchWithMediatorTest, ExecutesSearchWithStandardNewTabWhenIncognito) {
  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  SearchWithMediator* mediator = [[SearchWithMediator alloc]
      initWithTemplateURLService:template_url_service_
           aimEligibilityService:aim_eligibility_service
            cobrowseBrowserAgent:agent
                       incognito:YES];
  mediator.sceneHandler = mock_scene_handler_;

  [[mock_scene_handler_ reject] showAssistantWithNewSession];
  OCMExpect([mock_scene_handler_ openURLInNewTab:[OCMArg any]]);

  [mediator executeSearchForText:@"hello world"
      allowedByDataControlsRulesPolicy:YES];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
  EXPECT_FALSE(agent->IsSessionActive());

  [mediator shutdown];
}

// Tests that when the feature flag is disabled, search with opens a standard
// new tab.
TEST_F(SearchWithMediatorTest,
       ExecutesSearchWithStandardNewTabWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kAimCobrowseWebSelectionSearch);

  CobrowseBrowserAgent* agent =
      CobrowseBrowserAgent::FromBrowser(browser_.get());
  AimEligibilityService* aim_eligibility_service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  SearchWithMediator* mediator = [[SearchWithMediator alloc]
      initWithTemplateURLService:template_url_service_
           aimEligibilityService:aim_eligibility_service
            cobrowseBrowserAgent:agent
                       incognito:NO];
  mediator.sceneHandler = mock_scene_handler_;

  [[mock_scene_handler_ reject] showAssistantWithNewSession];
  OCMExpect([mock_scene_handler_ openURLInNewTab:[OCMArg any]]);

  [mediator executeSearchForText:@"hello world"
      allowedByDataControlsRulesPolicy:YES];

  EXPECT_OCMOCK_VERIFY(mock_scene_handler_);
  EXPECT_FALSE(agent->IsSessionActive());

  [mediator shutdown];
}

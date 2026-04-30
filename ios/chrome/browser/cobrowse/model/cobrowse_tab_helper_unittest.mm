// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"

#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class CobrowseTabHelperTest : public PlatformTest {
 protected:
  CobrowseTabHelperTest() {
    feature_list_.InitAndEnableFeature(kAimCobrowse);

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindRepeating([](ProfileIOS* profile)
                                -> std::unique_ptr<KeyedService> {
          auto service =
              std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
                  *profile->GetPrefs(),
                  ios::TemplateURLServiceFactory::GetForProfile(profile),
                  nullptr, IdentityManagerFactory::GetForProfile(profile));
          ON_CALL(*service, IsFuseboxEligible())
              .WillByDefault(testing::Return(true));
          ON_CALL(*service, IsCobrowseEligible())
              .WillByDefault(testing::Return(true));
          return service;
        }));
    profile_ = std::move(builder).Build();

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();

    // Add default search provider
    TemplateURLData data;
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

    mock_scene_state_ = OCMClassMock([SceneState class]);
    mock_tab_grid_state_ = OCMClassMock([TabGridState class]);
    OCMStub([mock_scene_state_ tabGridState]).andReturn(mock_tab_grid_state_);

    browser_ = std::make_unique<TestBrowser>(profile_.get(), mock_scene_state_);
    web_state_list_ = browser_->GetWebStateList();

    // Create a mock dispatcher and associate it with a mock scene commands
    // handler.
    mock_command_dispatcher_ = OCMClassMock([CommandDispatcher class]);
    mock_scene_commands_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    OCMStub([mock_command_dispatcher_
                strictCallableForProtocol:@protocol(SceneCommands)])
        .andReturn(mock_scene_commands_handler_);
    browser_->SetCommandDispatcher(mock_command_dispatcher_);

    CobrowseBrowserAgent::CreateForBrowser(browser_.get());

    fake_web_state_ = CreateAndInsertWebState(GURL::EmptyGURL());

    tab_helper_ = CobrowseTabHelper::FromWebState(fake_web_state_);
  }

  // Creates a new WebState with the given `url`, adds it to the list and
  // returns a pointer to it.
  web::FakeWebState* CreateAndInsertWebState(const GURL& url) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->SetCurrentURL(url);
    web::FakeWebState* web_state_ptr = web_state.get();
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    CobrowseTabHelper::CreateForWebState(web_state_ptr, template_url_service);
    web_state_list_->InsertWebState(std::move(web_state));
    return web_state_ptr;
  }

  // Creates a new WebState with the given `url` and `opener`, adds it to the
  // list and returns a pointer to it.
  web::FakeWebState* CreateAndInsertWebStateWithOpener(const GURL& url,
                                                       web::WebState* opener) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->SetCurrentURL(url);
    web::FakeWebState* web_state_ptr = web_state.get();
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    CobrowseTabHelper::CreateForWebState(web_state_ptr, template_url_service);
    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().WithOpener(
            WebStateOpener(opener)));
    return web_state_ptr;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<web::FakeWebState> fake_web_state_;
  raw_ptr<CobrowseTabHelper> tab_helper_;
  id mock_scene_commands_handler_;
  id mock_command_dispatcher_;
  id mock_scene_state_;
  id mock_tab_grid_state_;
};

// Tests that showAssistant is called when navigating in a new tab if the opener
// was an AIM URL.
TEST_F(CobrowseTabHelperTest, TriggerAssistantFromOpener) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  OCMStub([mock_tab_grid_state_ tabGridVisible]).andReturn(NO);

  web::FakeWebState* opener_ptr = CreateAndInsertWebState(aim_url);

  web::FakeWebState* new_web_state_ptr =
      CreateAndInsertWebStateWithOpener(GURL::EmptyGURL(), opener_ptr);

  CobrowseTabHelper* new_tab_helper =
      CobrowseTabHelper::FromWebState(new_web_state_ptr);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  OCMExpect([mock_scene_commands_handler_ showAssistant]);

  new_tab_helper->DidStartNavigation(new_web_state_ptr, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that showAssistant is NOT called when navigating in a new tab if the
// opener was NOT an AIM URL.
TEST_F(CobrowseTabHelperTest, NoTriggerFromNonAimOpener) {
  GURL non_aim_url("https://www.google.com/search?q=test");
  GURL next_url("https://www.example.com");

  web::FakeWebState* opener_ptr = CreateAndInsertWebState(non_aim_url);

  web::FakeWebState* new_web_state_ptr =
      CreateAndInsertWebStateWithOpener(GURL::EmptyGURL(), opener_ptr);

  CobrowseTabHelper* new_tab_helper =
      CobrowseTabHelper::FromWebState(new_web_state_ptr);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  OCMStub([mock_tab_grid_state_ tabGridVisible]).andReturn(NO);

  [[mock_scene_commands_handler_ reject] showAssistant];

  new_tab_helper->DidStartNavigation(new_web_state_ptr, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that showAssistant is NOT called when navigating in the same tab,
// even if it's an AIM URL, because it doesn't have an opener.
TEST_F(CobrowseTabHelperTest, NoTriggerInSameTab) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL non_aim_url("https://www.google.com/search?q=test");

  fake_web_state_->SetCurrentURL(aim_url);

  web::FakeNavigationContext context;
  context.SetUrl(non_aim_url);

  OCMStub([mock_tab_grid_state_ tabGridVisible]).andReturn(NO);

  [[mock_scene_commands_handler_ reject] showAssistant];

  tab_helper_->DidStartNavigation(fake_web_state_, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that showAssistant is NOT called when navigating in an incognito
// browser.
TEST_F(CobrowseTabHelperTest, NoTriggerInIncognito) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  // Create an incognito browser.
  ProfileIOS* incognito_profile = profile_->GetOffTheRecordProfile();
  TemplateURLService* incognito_template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(incognito_profile);
  std::unique_ptr<TestBrowser> incognito_browser =
      std::make_unique<TestBrowser>(incognito_profile, mock_scene_state_);

  // Create an opener WebState in the incognito browser.
  auto opener_web_state = std::make_unique<web::FakeWebState>();
  opener_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  opener_web_state->SetCurrentURL(aim_url);
  web::FakeWebState* opener_ptr = opener_web_state.get();
  CobrowseTabHelper::CreateForWebState(opener_web_state.get(),
                                       incognito_template_url_service);
  incognito_browser->GetWebStateList()->InsertWebState(
      std::move(opener_web_state));

  // Create a new WebState with the opener in the incognito browser.
  auto new_web_state = std::make_unique<web::FakeWebState>();
  new_web_state->SetNavigationManager(
      std::make_unique<web::FakeNavigationManager>());
  new_web_state->SetCurrentURL(GURL::EmptyGURL());
  web::FakeWebState* new_web_state_ptr = new_web_state.get();
  CobrowseTabHelper::CreateForWebState(new_web_state_ptr,
                                       incognito_template_url_service);
  incognito_browser->GetWebStateList()->InsertWebState(
      std::move(new_web_state),
      WebStateList::InsertionParams::Automatic().WithOpener(
          WebStateOpener(opener_ptr)));

  CobrowseTabHelper* incognito_tab_helper =
      CobrowseTabHelper::FromWebState(new_web_state_ptr);

  // In an incognito browser, CobrowseBrowserAgent is not created, so the
  // delegate and scene commands handler should be null.

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  OCMStub([mock_tab_grid_state_ tabGridVisible]).andReturn(NO);

  [[mock_scene_commands_handler_ reject] showAssistant];

  incognito_tab_helper->DidStartNavigation(new_web_state_ptr, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that hideAssistant is called when navigating to a search URL.
TEST_F(CobrowseTabHelperTest, HideAssistantOnSearchNavigation) {
  GURL search_url("https://www.google.com/search?q=test");

  web::FakeNavigationContext context;
  context.SetUrl(search_url);

  OCMExpect([mock_scene_commands_handler_ hideAssistant]);

  tab_helper_->DidStartNavigation(fake_web_state_, &context);

  [mock_scene_commands_handler_ verify];
}

// Tests that hideAssistant is called when navigating to the NTP.
TEST_F(CobrowseTabHelperTest, HideAssistantOnNtpNavigation) {
  GURL ntp_url("chrome://newtab");

  web::FakeNavigationContext context;
  context.SetUrl(ntp_url);

  OCMExpect([mock_scene_commands_handler_ hideAssistant]);

  tab_helper_->DidStartNavigation(fake_web_state_, &context);

  [mock_scene_commands_handler_ verify];
}

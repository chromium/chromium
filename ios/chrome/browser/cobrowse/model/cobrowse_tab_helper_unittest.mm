// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"

#import "base/no_destructor.h"
#import "base/test/scoped_feature_list.h"
#import "components/omnibox/browser/mock_aim_eligibility_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
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
 public:
  CobrowseTabHelperTest() {
    feature_list_.InitWithFeatures({kAimCobrowse, kAssistantContainer},
                                   {kPreventCobrowseOnAimSrpTap});

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

    scene_state_ = [[FakeSceneState alloc] initWithProfile:profile_.get()];
    scene_state_.sceneSessionID = "FakeScene";

    // Create a mock command handler for SceneCommands and register it.
    mock_scene_commands_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser()->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_commands_handler_
                     forProtocol:@protocol(SceneCommands)];

    CobrowseBrowserAgent::CreateForBrowser(browser());
  }

  ~CobrowseTabHelperTest() override {
    [scene_state_ shutdown];
    scene_state_ = nil;
  }

  // Creates a new WebState with the given `url`, adds it to the list and
  // returns a pointer to it.
  web::FakeWebState* CreateAndInsertWebState(const GURL& url) {
    return CreateAndInsertWebStateWithOptions(url, browser(),
                                              /*opener=*/nullptr);
  }

  // Creates a new WebState with the given `url` and `opener`, adds it to the
  // list and returns a pointer to it.
  web::FakeWebState* CreateAndInsertWebStateWithOpener(const GURL& url,
                                                       web::WebState* opener) {
    return CreateAndInsertWebStateWithOptions(url, browser(), opener);
  }

  // Creates a new incognito WebState with the given `url`, adds it to the
  // list and returns a pointer to it.
  web::FakeWebState* CreateAndInsertIncognitoWebState(const GURL& url) {
    return CreateAndInsertWebStateWithOptions(url, incognito_browser(),
                                              /*opener=*/nullptr);
  }

  // Creates a new incognito WebState with the given `url` and `opener`, adds
  // it to the list and returns a pointer to it.
  web::FakeWebState* CreateAndInsertIncognitoWebStateWithOpener(
      const GURL& url,
      web::WebState* opener) {
    return CreateAndInsertWebStateWithOptions(url, incognito_browser(), opener);
  }

  // Returns the regular profile.
  TestProfileIOS* profile() { return profile_.get(); }

  // Returns the mock SceneCommandsHandler.
  id mock_scene_commands_handler() { return mock_scene_commands_handler_; }

  // Returns the CobrowseBrowserAgent for the regular browser.
  CobrowseBrowserAgent* agent() {
    return CobrowseBrowserAgent::FromBrowser(browser());
  }

 private:
  // Creates a new WebState with options, adds it to the Browser's list and
  // returns a pointer to it.
  web::FakeWebState* CreateAndInsertWebStateWithOptions(const GURL& url,
                                                        Browser* browser,
                                                        web::WebState* opener) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser->GetProfile());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    web_state->SetCurrentURL(url);

    CobrowseTabHelper::CreateForWebState(
        web_state.get(),
        ios::TemplateURLServiceFactory::GetForProfile(browser->GetProfile()));

    WebStateList* web_state_list = browser->GetWebStateList();
    auto insertion_params = WebStateList::InsertionParams::Automatic();
    if (opener != nullptr) {
      const int opener_index = web_state_list->GetIndexOfWebState(opener);
      CHECK_NE(opener_index, WebStateList::kInvalidIndex);
      insertion_params.WithOpener(WebStateOpener(opener));
    }

    const int insertion_index = web_state_list->InsertWebState(
        std::move(web_state), std::move(insertion_params));
    return static_cast<web::FakeWebState*>(
        web_state_list->GetWebStateAt(insertion_index));
  }

  // Returns the regular browser.
  Browser* browser() {
    return scene_state_.browserProviderInterface.mainBrowserProvider.browser;
  }

  // Returns the incognito browser.
  Browser* incognito_browser() {
    return scene_state_.browserProviderInterface.incognitoBrowserProvider
        .browser;
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  FakeSceneState* scene_state_;
  id mock_scene_commands_handler_;
};

// Tests that showAssistant is called when navigating in a new tab if the opener
// was an AIM URL.
TEST_F(CobrowseTabHelperTest, TriggerAssistantFromOpener) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  web::FakeWebState* opener = CreateAndInsertWebState(aim_url);
  web::FakeWebState* web_state = CreateAndInsertWebStateWithOpener({}, opener);
  web_state->WasShown();
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  OCMExpect([mock_scene_commands_handler() showAssistantInMinimizedState:YES]);

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that showAssistant is NOT called when navigating in a new tab if the
// opener was an AIM URL but the prevent flag is enabled.
TEST_F(CobrowseTabHelperTest, NoTriggerFromOpenerWhenPreventFlagEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kPreventCobrowseOnAimSrpTap);

  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  web::FakeWebState* opener = CreateAndInsertWebState(aim_url);
  web::FakeWebState* web_state = CreateAndInsertWebStateWithOpener({}, opener);
  web_state->WasShown();
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  [[mock_scene_commands_handler() reject] showAssistantInMinimizedState:YES];

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that showAssistant is NOT called when navigating in a new tab if the
// opener was NOT an AIM URL.
TEST_F(CobrowseTabHelperTest, NoTriggerFromNonAimOpener) {
  GURL non_aim_url("https://www.google.com/search?q=test");
  GURL next_url("https://www.example.com");

  web::FakeWebState* opener = CreateAndInsertWebState(non_aim_url);
  web::FakeWebState* web_state = CreateAndInsertWebStateWithOpener({}, opener);
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  [[mock_scene_commands_handler() reject] showAssistant];

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that showAssistant is NOT called when navigating in the same tab,
// even if it's an AIM URL, because it doesn't have an opener.
TEST_F(CobrowseTabHelperTest, NoTriggerInSameTab) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL non_aim_url("https://www.google.com/search?q=test");

  web::FakeWebState* web_state = CreateAndInsertWebState({});
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web_state->SetCurrentURL(aim_url);

  web::FakeNavigationContext context;
  context.SetUrl(non_aim_url);

  [[mock_scene_commands_handler() reject] showAssistant];

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that showAssistant is NOT called when navigating in an incognito
// browser.
TEST_F(CobrowseTabHelperTest, NoTriggerInIncognito) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  // Create an opener WebState in the incognito browser.
  web::FakeWebState* incognito_opener =
      CreateAndInsertIncognitoWebState(aim_url);

  // Create a new WebState with the opener in the incognito browser.
  web::FakeWebState* incognito_web_state =
      CreateAndInsertIncognitoWebStateWithOpener({}, incognito_opener);

  CobrowseTabHelper* incognito_tab_helper =
      CobrowseTabHelper::FromWebState(incognito_web_state);

  // In an incognito browser, CobrowseBrowserAgent is not created, so the
  // delegate and scene commands handler should be null.

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  [[mock_scene_commands_handler() reject] showAssistant];

  incognito_tab_helper->DidStartNavigation(incognito_web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that closeAssistant is NOT called when navigating to a regular search
// URL.
TEST_F(CobrowseTabHelperTest, NoCloseAssistantOnRegularSearchNavigation) {
  GURL search_url("https://www.google.com/search?q=test");

  web::FakeWebState* web_state = CreateAndInsertWebState({});
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web::FakeNavigationContext context;
  context.SetUrl(search_url);

  [[mock_scene_commands_handler() reject] closeAssistant];
  [[mock_scene_commands_handler() reject] hideAssistant];

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that hideAssistant is called when navigating to an AIM search URL.
TEST_F(CobrowseTabHelperTest, HideAssistantOnAimSearchNavigation) {
  GURL aim_search_url("https://www.google.com/search?q=test&udm=50");

  web::FakeWebState* web_state = CreateAndInsertWebState({});
  web_state->WasShown();
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web::FakeNavigationContext context;
  context.SetUrl(aim_search_url);

  OCMExpect([mock_scene_commands_handler() hideAssistant]);

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that hideAssistant is called when navigating to an AIM Zero State
// search URL.
TEST_F(CobrowseTabHelperTest, HideAssistantOnAimZeroStateSearchNavigation) {
  GURL aim_zero_state_url("https://www.google.com/?udm=50");

  web::FakeWebState* web_state = CreateAndInsertWebState({});
  web_state->WasShown();
  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  web::FakeNavigationContext context;
  context.SetUrl(aim_zero_state_url);

  OCMExpect([mock_scene_commands_handler() hideAssistant]);

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that hideAssistant is called when navigating to the NTP, and
// showAssistant is restored when navigating to a normal web page.
TEST_F(CobrowseTabHelperTest, HideOnNtpAndRestoreOnNormalNavigation) {
  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL ntp_url("chrome://newtab");
  GURL normal_url("https://www.example.com");

  web::FakeWebState* opener = CreateAndInsertWebState(aim_url);

  web::FakeWebState* web_state = CreateAndInsertWebStateWithOpener({}, opener);
  web_state->WasShown();

  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);

  // 1. Start session by navigating to a normal page.
  web::FakeNavigationContext context1;
  context1.SetUrl(normal_url);
  OCMExpect([mock_scene_commands_handler() showAssistantInMinimizedState:YES]);
  tab_helper->DidStartNavigation(web_state, &context1);
  [mock_scene_commands_handler() verify];

  // 2. Navigate to NTP -> should hide.
  web::FakeNavigationContext context2;
  context2.SetUrl(ntp_url);
  OCMExpect([mock_scene_commands_handler() hideAssistant]);
  tab_helper->DidStartNavigation(web_state, &context2);
  [mock_scene_commands_handler() verify];

  // 3. Navigate to a normal page again -> should restore (show).
  web::FakeNavigationContext context3;
  context3.SetUrl(normal_url);
  OCMExpect([mock_scene_commands_handler() showAssistantInMinimizedState:YES]);
  tab_helper->DidStartNavigation(web_state, &context3);
  [mock_scene_commands_handler() verify];
}

// Tests that showAssistant is NOT called when navigating in a new tab from an
// AIM URL if Cobrowse is ineligible at runtime (even if the feature flag is
// enabled).
TEST_F(CobrowseTabHelperTest, NoTriggerWhenNotEligible) {
  MockAimEligibilityService* service = static_cast<MockAimEligibilityService*>(
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*service, IsCobrowseEligible())
      .WillRepeatedly(testing::Return(false));

  GURL aim_url("https://www.google.com/search?q=test&udm=50");
  GURL next_url("https://www.example.com");

  web::FakeWebState* opener = CreateAndInsertWebState(aim_url);
  web::FakeWebState* web_state = CreateAndInsertWebStateWithOpener({}, opener);

  CobrowseTabHelper* tab_helper = CobrowseTabHelper::FromWebState(web_state);
  ASSERT_NE(tab_helper, nullptr);

  web::FakeNavigationContext context;
  context.SetUrl(next_url);

  [[mock_scene_commands_handler() reject] showAssistant];

  tab_helper->DidStartNavigation(web_state, &context);

  [mock_scene_commands_handler() verify];
}

// Tests that SetCobrowseContext rejects a context update with an empty query
// UNLESS the context has attached items.
TEST_F(CobrowseTabHelperTest,
       SetCobrowseContextRejectsEmptyQueryUnlessHasAttachments) {
  // 1. Initial valid context.
  GURL valid_url("https://www.google.com/search?q=valid&udm=50");
  CobrowseContext* valid_context =
      [[CobrowseContext alloc] initWithURL:valid_url];
  agent()->SetCobrowseContext(valid_context);
  EXPECT_EQ(agent()->GetCobrowseContext(), valid_context);

  // 2. Empty query context WITHOUT attachments should be REJECTED.
  GURL empty_query_url("https://www.google.com/search?q=&udm=50");
  CobrowseContext* empty_query_context =
      [[CobrowseContext alloc] initWithURL:empty_query_url];
  agent()->SetCobrowseContext(empty_query_context);
  // The agent should ignore the empty query context and keep the valid one.
  EXPECT_EQ(agent()->GetCobrowseContext(), valid_context);

  // 3. Empty query context WITH attachments should be ACCEPTED.
  CobrowseContext* empty_query_with_attachment_context =
      [[CobrowseContext alloc] initWithURL:empty_query_url];
  // Mock an attachment item. A non-empty array is sufficient.
  empty_query_with_attachment_context.attachedItems =
      @[ [[NSObject alloc] init] ];

  agent()->SetCobrowseContext(empty_query_with_attachment_context);
  EXPECT_EQ(agent()->GetCobrowseContext(), empty_query_with_attachment_context);

  // 4. Empty query context WITH valid server session tokens should be ACCEPTED.
  GURL valid_session_url("https://www.google.com/search?q=&udm=50&cinpts=123");
  CobrowseContext* valid_session_context =
      [[CobrowseContext alloc] initWithURL:valid_session_url];
  agent()->SetCobrowseContext(valid_session_context);
  // The agent should accept the update because it has a valid session token,
  // even without attachments.
  EXPECT_EQ(agent()->GetCobrowseContext(), valid_session_context);

  // 5. If transitioning from a non-empty query to an empty query without
  // attachments, it should be REJECTED (simulates the chip tap bug).
  // First, set a valid non-empty query context.
  GURL valid_query_url("https://www.google.com/search?q=hello&udm=50");
  CobrowseContext* valid_query_context =
      [[CobrowseContext alloc] initWithURL:valid_query_url];
  agent()->SetCobrowseContext(valid_query_context);
  EXPECT_EQ(agent()->GetCobrowseContext(), valid_query_context);

  // Now, attempt to transition to an empty query with session tokens.
  GURL buggy_chip_url("https://www.google.com/search?q=&udm=50&mstk=abc");
  CobrowseContext* buggy_chip_context =
      [[CobrowseContext alloc] initWithURL:buggy_chip_url];
  agent()->SetCobrowseContext(buggy_chip_context);
  // The agent should REJECT the update because the transition is from a
  // valid query to an empty query without attachments, simulating the chip tap
  // bug.
  EXPECT_EQ(agent()->GetCobrowseContext(), valid_query_context);
}

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/gtest_util.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_delegate.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/test_scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_interceptor.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

class TestInterceptor : public URLInterceptor {
 public:
  TestInterceptor() { set_active(true); }

  bool OnIntercept(const UrlLoadParams& params) override {
    intercepted_ = true;
    return should_succeed_;
  }

  bool intercepted_ = false;
  bool should_succeed_ = true;
};

class URLLoadingBrowserAgentTest : public BlockCleanupTest {
 public:
  URLLoadingBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    otr_profile_ = profile_->GetOffTheRecordProfile();
    url_loading_delegate_ = [[FakeURLLoadingDelegate alloc] init];
    scene_loader_ = std::make_unique<TestSceneUrlLoadingService>();
    otr_browser_ = std::make_unique<TestBrowser>(otr_profile_);

    // Configure app service.
    scene_loader_->current_browser_ = browser_.get();
    scene_loader_->original_browser_ = browser_.get();
    scene_loader_->otr_browser_ = otr_browser_.get();

    // Disable web usage on both browsers
    WebUsageEnablerBrowserAgent::CreateForBrowser(browser_.get());
    WebUsageEnablerBrowserAgent* enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(browser_.get());
    enabler->SetWebUsageEnabled(false);
    WebUsageEnablerBrowserAgent::CreateForBrowser(otr_browser_.get());
    WebUsageEnablerBrowserAgent* otr_enabler =
        WebUsageEnablerBrowserAgent::FromBrowser(otr_browser_.get());
    otr_enabler->SetWebUsageEnabled(false);

    // Create loaders, insertion and notifier agents.
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(browser_.get());
    loader_ = UrlLoadingBrowserAgent::FromBrowser(browser_.get());
    loader_->SetDelegate(url_loading_delegate_);
    loader_->SetSceneService(scene_loader_.get());

    UrlLoadingNotifierBrowserAgent::CreateForBrowser(otr_browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(otr_browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(otr_browser_.get());
    otr_loader_ = UrlLoadingBrowserAgent::FromBrowser(otr_browser_.get());
    otr_loader_->SetDelegate(url_loading_delegate_);
    otr_loader_->SetSceneService(scene_loader_.get());

    loader_->SetIncognitoLoader(otr_loader_);
  }

  void TearDown() override {
    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = browser_->GetWebStateList();
    CloseAllWebStates(*web_state_list, WebStateList::ClosingReason::kDefault);
    WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
    CloseAllWebStates(*otr_web_state_list,
                      WebStateList::ClosingReason::kDefault);

    BlockCleanupTest::TearDown();
  }

  // Returns a new unique_ptr containing a test webstate.
  std::unique_ptr<web::FakeWebState> CreateFakeWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    return web_state;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<ProfileIOS> otr_profile_;
  FakeURLLoadingDelegate* url_loading_delegate_;
  std::unique_ptr<TestSceneUrlLoadingService> scene_loader_;
  raw_ptr<UrlLoadingBrowserAgent> loader_;
  std::unique_ptr<Browser> otr_browser_;
  raw_ptr<UrlLoadingBrowserAgent> otr_loader_;
};

TEST_F(URLLoadingBrowserAgentTest, TestSwitchToTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("http://test/1"));
  web_state_list->InsertWebState(std::move(web_state));

  std::unique_ptr<web::FakeWebState> web_state_2 = CreateFakeWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(std::move(web_state_2));

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  loader_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests that switch to open tab from the NTP close it if it doesn't have
// navigation history.
TEST_F(URLLoadingBrowserAgentTest, TestSwitchToTabFromNTP) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web_state_list->InsertWebState(std::move(web_state));
  id mock_delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state_ptr);
  NewTabPageTabHelper::FromWebState(web_state_ptr)->SetDelegate(mock_delegate);

  std::unique_ptr<web::FakeWebState> web_state_2 = CreateFakeWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(std::move(web_state_2));

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  loader_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests that trying to switch to a closed tab open from the NTP opens it in the
// NTP.
TEST_F(URLLoadingBrowserAgentTest, TestSwitchToClosedTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web::WebState* web_state_ptr = web_state.get();
  web_state_list->InsertWebState(std::move(web_state));
  web_state_list->ActivateWebStateAt(0);
  id mock_delegate = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  NewTabPageTabHelper::CreateForWebState(web_state_ptr);
  NewTabPageTabHelper::FromWebState(web_state_ptr)->SetDelegate(mock_delegate);

  GURL url("http://test/2");

  loader_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(web_state_ptr, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests open a new url in the NTP or the current tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInCurrentTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab, so we can open in it.
  GURL newtab("chrome://newtab");
  loader_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Test opening this url over NTP.
  GURL url1("http://test/1");
  loader_->Load(
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1)));

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());
  // And that a new tab wasn't created.
  EXPECT_EQ(1, web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests opening a url in a specific tab.
TEST_F(URLLoadingBrowserAgentTest, TestLoadUrlInTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  // Create two tabs and activate the second one.
  std::unique_ptr<web::FakeWebState> web_state = CreateFakeWebState();
  std::unique_ptr<web::FakeWebState> web_state_2 = CreateFakeWebState();
  web::WebState* web_state_ptr = web_state.get();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  web_state->SetCurrentURL(GURL("http://test.example/1"));
  web_state_2->SetCurrentURL(GURL("http://test.example/2"));
  web_state_list->InsertWebState(std::move(web_state));
  web_state_list->InsertWebState(std::move(web_state_2));
  web_state_list->ActivateWebStateAt(1);

  GURL url3("http://test.example/3");
  // Use `InCurrentTab` despite the tab being in the background,
  // since we're navigating within a tab that's already open.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(url3);
  loader_->LoadUrlInTab(params, web_state_ptr);

  // Verify that the background tab navigated to the new URL.
  web::FakeNavigationManager* navigation_manager =
      static_cast<web::FakeNavigationManager*>(
          web_state_ptr->GetNavigationManager());
  EXPECT_TRUE(navigation_manager->LoadURLWithParamsWasCalled());
  const std::optional<web::NavigationManager::WebLoadParams>& last_params =
      navigation_manager->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(url3, last_params->url);

  // Verify that the second tab is still active and didn't navigate.
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  web::FakeNavigationManager* navigation_manager_2 =
      static_cast<web::FakeNavigationManager*>(
          web_state_ptr_2->GetNavigationManager());
  EXPECT_FALSE(navigation_manager_2->LoadURLWithParamsWasCalled());
}

// Tests opening a url in a new tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInNewTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab.
  GURL url1("chrome://newtab");
  loader_->Load(UrlLoadParams::InNewTab(url1));
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(web_state_list->GetWebStateAt(0),
            web_state_list->GetActiveWebState());

  // Open another one.
  GURL url2("http://test/2");
  loader_->Load(UrlLoadParams::InNewTab(url2));
  EXPECT_EQ(2, web_state_list->count());
  EXPECT_EQ(web_state_list->GetWebStateAt(1),
            web_state_list->GetActiveWebState());

  // Activate the first tab.
  web_state_list->ActivateWebStateAt(0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0),
            web_state_list->GetActiveWebState());

  // Open another one.
  GURL url3("http://test/3");
  loader_->Load(UrlLoadParams::InNewTab(url3));
  EXPECT_EQ(3, web_state_list->count());

  // Make sure that the new tab is added to the end of the list.
  EXPECT_EQ(web_state_list->GetWebStateAt(2),
            web_state_list->GetActiveWebState());

  // Activate the first tab.
  web_state_list->ActivateWebStateAt(0);
  EXPECT_EQ(web_state_list->GetWebStateAt(0),
            web_state_list->GetActiveWebState());

  // Open another one next to the active one.
  GURL url4("http://test/4");
  UrlLoadParams params = UrlLoadParams::InNewTab(url4);
  params.append_to = OpenPosition::kCurrentTab;
  loader_->Load(params);
  EXPECT_EQ(4, web_state_list->count());

  // Make sure that the new tab is added next to the previously active tab.
  EXPECT_EQ(web_state_list->GetWebStateAt(1),
            web_state_list->GetActiveWebState());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests open a new url in the current incognito tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInCurrentIncognitoTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  std::unique_ptr<TestBrowser> otr_browser =
      std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
  scene_loader_->current_browser_ = otr_browser.get();

  // Set a new tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_loader_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Open otr request with otr service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_loader_->Load(params1);

  // We won't to wait for the navigation item to be committed, let's just
  // make sure it is at least pending.
  EXPECT_EQ(url1, otr_web_state_list->GetActiveWebState()
                      ->GetNavigationManager()
                      ->GetPendingItem()
                      ->GetOriginalRequestURL());

  // And that a new tab wasn't created.
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Tests opening a url in a new incognito tab.
TEST_F(URLLoadingBrowserAgentTest, TestOpenInNewIncognitoTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  std::unique_ptr<TestBrowser> otr_browser =
      std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
  scene_loader_->current_browser_ = otr_browser.get();

  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = YES;
  otr_loader_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(2, otr_web_state_list->count());

  // Check if we had any app level redirection.
  EXPECT_EQ(0, scene_loader_->load_new_tab_call_count_);
}

// Test opening a normal url in new tab with incognito service.
TEST_F(URLLoadingBrowserAgentTest, TestOpenNormalInNewTabWithIncognitoService) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  std::unique_ptr<TestBrowser> otr_browser =
      std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
  scene_loader_->current_browser_ = otr_browser.get();

  // Send to right service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to wrong service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  otr_loader_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that had one app level redirection.
  EXPECT_EQ(1, scene_loader_->load_new_tab_call_count_);
}

// Test opening an incognito url in new tab with normal service.
TEST_F(URLLoadingBrowserAgentTest, TestOpenIncognitoInNewTabWithNormalService) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_browser_->GetWebStateList();
  ASSERT_EQ(0, otr_web_state_list->count());

  scene_loader_->current_browser_ = browser_.get();

  // Send to wrong service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  loader_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Send to right service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  loader_->Load(params2);
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, scene_loader_->load_new_tab_call_count_);
}

// Tests that the interceptor is called when a matching URL is loaded.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorCalled) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();

  TestInterceptor* interceptor_ptr = interceptor.get();
  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  loader_->Load(UrlLoadParams::InCurrentTab(url));

  // Verify that the interceptor was called.
  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that an inactive interceptor is not called.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorInactive) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();
  interceptor->set_active(false);
  TestInterceptor* interceptor_ptr = interceptor.get();
  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  loader_->Load(UrlLoadParams::InCurrentTab(url));

  // Verify that the inactive interceptor was not called.
  EXPECT_FALSE(interceptor_ptr->intercepted_);
}

// Tests that an interceptor can prevent the normal URL loading flow.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorPreventsLoad) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();
  TestInterceptor* interceptor_ptr = interceptor.get();

  interceptor->set_prevent_normal_flow(true);
  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  loader_->Load(UrlLoadParams::InCurrentTab(url));

  // Load should be prevented, so no tab created or loaded.
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that when an interceptor fails (returns false), the normal URL loading
// flow is NOT prevented, even if prevent_normal_flow is configured to true.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorFailsAndDoesNotPreventLoad) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();
  TestInterceptor* interceptor_ptr = interceptor.get();

  interceptor->set_prevent_normal_flow(true);
  interceptor->should_succeed_ = false;
  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  loader_->Load(UrlLoadParams::InNewTab(url));

  // Load should NOT be prevented, so a tab should be created.
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that an interceptor does not prevent the normal URL loading flow if
// configured not to.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorDoesNotPreventLoad) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();
  TestInterceptor* interceptor_ptr = interceptor.get();
  interceptor->set_prevent_normal_flow(false);

  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(0, web_state_list->count());

  loader_->Load(UrlLoadParams::InNewTab(url));

  // Load should NOT be prevented, so a tab should be created.
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that an interceptor can deactivate itself after a successful match.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorDeactivatesOnMatch) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();
  interceptor->set_deactivates_on_match(true);

  TestInterceptor* interceptor_ptr = interceptor.get();
  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  loader_->Load(UrlLoadParams::InCurrentTab(url));

  // The interceptor should deactivate itself after a successful match.
  EXPECT_FALSE(interceptor_ptr->active());
  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that an interceptor does NOT deactivate itself if it matches but fails.
TEST_F(URLLoadingBrowserAgentTest,
       TestInterceptorDoesNotDeactivateOnFailedMatch) {
  GURL url("http://test/1");
  auto interceptor = std::make_unique<TestInterceptor>();
  interceptor->set_deactivates_on_match(true);
  interceptor->should_succeed_ = false;

  TestInterceptor* interceptor_ptr = interceptor.get();
  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor)));

  loader_->Load(UrlLoadParams::InNewTab(url));

  // The interceptor should NOT deactivate itself since it failed.
  EXPECT_TRUE(interceptor_ptr->active());
  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that interception works correctly when multiple interceptors are
// configured for different URLs.
TEST_F(URLLoadingBrowserAgentTest, TestMultipleInterceptors) {
  GURL url1("http://test/1");
  GURL url2("http://test/2");

  auto interceptor1 = std::make_unique<TestInterceptor>();
  auto interceptor2 = std::make_unique<TestInterceptor>();

  TestInterceptor* interceptor1_ptr = interceptor1.get();
  TestInterceptor* interceptor2_ptr = interceptor2.get();

  EXPECT_TRUE(loader_->AddInterceptor(url1, std::move(interceptor1)));
  EXPECT_TRUE(loader_->AddInterceptor(url2, std::move(interceptor2)));

  loader_->Load(UrlLoadParams::InCurrentTab(url1));
  EXPECT_TRUE(interceptor1_ptr->intercepted_);
  EXPECT_FALSE(interceptor2_ptr->intercepted_);

  loader_->Load(UrlLoadParams::InCurrentTab(url2));
  EXPECT_TRUE(interceptor2_ptr->intercepted_);
}

// Tests that loading a URL without a registered interceptor does not
// trigger unrelated interceptors.
TEST_F(URLLoadingBrowserAgentTest, TestNoInterceptor) {
  GURL url("http://test/1");
  GURL other_url("http://test/2");

  auto interceptor = std::make_unique<TestInterceptor>();
  TestInterceptor* interceptor_ptr = interceptor.get();

  EXPECT_TRUE(loader_->AddInterceptor(other_url, std::move(interceptor)));

  loader_->Load(UrlLoadParams::InCurrentTab(url));

  EXPECT_FALSE(interceptor_ptr->intercepted_);
}

// Tests that an interceptor matches when the loaded URL has the registered
// URL as a prefix.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorPrefixMatching) {
  GURL prefix_url("http://test.example/prefix");
  GURL loaded_url("http://test.example/prefix/path?param=1");

  auto interceptor = std::make_unique<TestInterceptor>();
  TestInterceptor* interceptor_ptr = interceptor.get();

  EXPECT_TRUE(loader_->AddInterceptor(prefix_url, std::move(interceptor)));
  loader_->Load(UrlLoadParams::InCurrentTab(loaded_url));

  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that an interceptor matches even when the loaded URL only shares a
// raw string prefix and is not a valid sub-path.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorRawStringPrefixMatching) {
  GURL prefix_url("http://test.example/prefix");
  GURL loaded_url("http://test.example/prefix-other");

  auto interceptor = std::make_unique<TestInterceptor>();
  TestInterceptor* interceptor_ptr = interceptor.get();

  EXPECT_TRUE(loader_->AddInterceptor(prefix_url, std::move(interceptor)));
  loader_->Load(UrlLoadParams::InCurrentTab(loaded_url));

  EXPECT_TRUE(interceptor_ptr->intercepted_);
}

// Tests that adding an exact duplicate interceptor fails registration.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorExactDuplicateOverlap) {
  GURL url("http://test/exact");

  auto interceptor1 = std::make_unique<TestInterceptor>();
  auto interceptor2 = std::make_unique<TestInterceptor>();

  EXPECT_TRUE(loader_->AddInterceptor(url, std::move(interceptor1)));
  EXPECT_FALSE(loader_->AddInterceptor(url, std::move(interceptor2)));
}

// Tests that adding a narrower prefix interceptor over an existing broader
// prefix interceptor fails registration.
TEST_F(URLLoadingBrowserAgentTest,
       TestInterceptorExistingBroadOverlapsNewNarrow) {
  GURL broad_url("http://test/prefix");
  GURL narrow_url("http://test/prefix/path");

  auto interceptor1 = std::make_unique<TestInterceptor>();
  auto interceptor2 = std::make_unique<TestInterceptor>();

  EXPECT_TRUE(loader_->AddInterceptor(broad_url, std::move(interceptor2)));
  EXPECT_FALSE(loader_->AddInterceptor(narrow_url, std::move(interceptor1)));
}

// Tests that adding an existing broader prefix interceptor over a narrower
// prefix interceptor fails registration.
TEST_F(URLLoadingBrowserAgentTest,
       TestInterceptorExistingNarrowOverlapsNewBroad) {
  GURL narrow_url("http://test/prefix/path");
  GURL broad_url("http://test/prefix");

  auto interceptor1 = std::make_unique<TestInterceptor>();
  auto interceptor2 = std::make_unique<TestInterceptor>();

  EXPECT_TRUE(loader_->AddInterceptor(narrow_url, std::move(interceptor1)));
  EXPECT_FALSE(loader_->AddInterceptor(broad_url, std::move(interceptor2)));
}

// Tests that registering a URL that shares a raw string prefix with an
// existing registered URL fails registration.
TEST_F(URLLoadingBrowserAgentTest, TestInterceptorRawStringPrefixOverlap) {
  GURL prefix_url("http://test.example/prefix");
  GURL other_url("http://test.example/prefix-other");

  auto interceptor1 = std::make_unique<TestInterceptor>();
  auto interceptor2 = std::make_unique<TestInterceptor>();

  EXPECT_TRUE(loader_->AddInterceptor(prefix_url, std::move(interceptor1)));
  EXPECT_FALSE(loader_->AddInterceptor(other_url, std::move(interceptor2)));
}

}  // namespace

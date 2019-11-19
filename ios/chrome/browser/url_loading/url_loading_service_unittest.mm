// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_service.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/test_app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ULSTestTabModel : OCMockComplexTypeHelper
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, readonly) WebStateList* webStateList;
@end

@implementation ULSTestTabModel {
  FakeWebStateListDelegate _webStateListDelegate;
  std::unique_ptr<WebStateList> _webStateList;
}
@synthesize browserState = _browserState;

- (instancetype)init {
  if ((self = [super
           initWithRepresentedObject:[OCMockObject
                                         niceMockForClass:[TabModel class]]])) {
    _webStateList = std::make_unique<WebStateList>(&_webStateListDelegate);
  }
  return self;
}

- (WebStateList*)webStateList {
  return _webStateList.get();
}

- (web::WebState*)insertWebStateWithLoadParams:
                      (const web::NavigationManager::WebLoadParams&)loadParams
                                        opener:(web::WebState*)parentWebState
                                   openedByDOM:(BOOL)openedByDOM
                                       atIndex:(NSUInteger)index
                                  inBackground:(BOOL)inBackground {
  int insertionIndex = WebStateList::kInvalidIndex;
  int insertionFlags = WebStateList::INSERT_NO_FLAGS;
  if (index != TabModelConstants::kTabPositionAutomatically) {
    DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
    insertionIndex = static_cast<int>(index);
    insertionFlags |= WebStateList::INSERT_FORCE_INDEX;
  } else if (!ui::PageTransitionCoreTypeIs(loadParams.transition_type,
                                           ui::PAGE_TRANSITION_LINK)) {
    insertionIndex = _webStateList->count();
    insertionFlags |= WebStateList::INSERT_FORCE_INDEX;
  }

  if (!inBackground) {
    insertionFlags |= WebStateList::INSERT_ACTIVATE;
  }

  web::WebState::CreateParams createParams(self.browserState);
  createParams.created_with_opener = openedByDOM;

  std::unique_ptr<web::WebState> webState = web::WebState::Create(createParams);
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  _webStateList->InsertWebState(insertionIndex, std::move(webState),
                                insertionFlags, WebStateOpener(parentWebState));

  return nil;
}
@end

@interface URLLoadingServiceTestDelegate : NSObject <URLLoadingServiceDelegate>
@end

@implementation URLLoadingServiceTestDelegate

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - URLLoadingServiceDelegate

- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command {
}

- (void)animateOpenBackgroundTabFromParams:(const UrlLoadParams&)params
                                completion:(void (^)())completion {
}

@end

#pragma mark -

namespace {
class URLLoadingServiceTest : public BlockCleanupTest {
 public:
  URLLoadingServiceTest() {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    app_service_ = new TestAppUrlLoadingService();
    app_service_->currentBrowserState = chrome_browser_state_.get();
    url_loading_delegate_ = [[URLLoadingServiceTestDelegate alloc] init];

    id tabModel = CreateTestTabModel(chrome_browser_state_.get());
    tab_model_ = tabModel;
    browser_ = new TestBrowser(chrome_browser_state_.get(), tabModel);
    service_ = UrlLoadingServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    service_->SetDelegate(url_loading_delegate_);
    service_->SetBrowser(browser_);
    service_->SetAppService(app_service_);

    ios::ChromeBrowserState* otr_browser_state =
        chrome_browser_state_.get()->GetOffTheRecordChromeBrowserState();

    id otrTabModel = CreateTestTabModel(otr_browser_state);
    otr_tab_model_ = otrTabModel;
    otr_browser_ = new TestBrowser(otr_browser_state, otrTabModel);
    otr_service_ =
        UrlLoadingServiceFactory::GetForBrowserState(otr_browser_state);
    otr_service_->SetDelegate(url_loading_delegate_);
    otr_service_->SetBrowser(otr_browser_);
    otr_service_->SetAppService(app_service_);
  }

  void TearDown() override {
    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = tab_model_.webStateList;
    web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
    otr_web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);

    BlockCleanupTest::TearDown();
  }

  // Returns a new unique_ptr containing a test webstate.
  std::unique_ptr<web::TestWebState> CreateTestWebState() {
    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetBrowserState(chrome_browser_state_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    return web_state;
  }

  id CreateTestTabModel(ios::ChromeBrowserState* browser_state) {
    id tabModel = [[ULSTestTabModel alloc] init];
    [tabModel setBrowserState:browser_state];

    // Enable web usage for the mock TabModel's WebStateList.
    WebStateListWebUsageEnabler* enabler =
        WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
            browser_state);
    enabler->SetWebStateList([tabModel webStateList]);
    enabler->SetWebUsageEnabled(false);

    return tabModel;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<web::WebState> webState_;
  URLLoadingServiceTestDelegate* url_loading_delegate_;
  TestAppUrlLoadingService* app_service_;
  TabModel* tab_model_;
  Browser* browser_;
  UrlLoadingService* service_;
  TabModel* otr_tab_model_;
  Browser* otr_browser_;
  UrlLoadingService* otr_service_;
};

TEST_F(URLLoadingServiceTest, TestSwitchToTab) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("http://test/1"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::TestWebState> web_state_2 = CreateTestWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  service_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests that switch to open tab from the NTP close it if it doesn't have
// navigation history.
TEST_F(URLLoadingServiceTest, TestSwitchToTabFromNTP) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::TestWebState> web_state_2 = CreateTestWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  service_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests that trying to switch to a closed tab open from the NTP opens it in the
// NTP.
TEST_F(URLLoadingServiceTest, TestSwitchToClosedTab) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web::WebState* web_state_ptr = web_state.get();
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  web_state_list->ActivateWebStateAt(0);

  GURL url("http://test/2");

  service_->Load(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(web_state_ptr, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests open a new url in the NTP or the current tab.
TEST_F(URLLoadingServiceTest, TestOpenInCurrentTab) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab, so we can open in it.
  GURL newtab("chrome://newtab");
  service_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Test opening this url over NTP.
  GURL url1("http://test/1");
  service_->Load(
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
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests opening a url in a new tab.
TEST_F(URLLoadingServiceTest, TestOpenInNewTab) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  // Set a new tab.
  GURL newtab("chrome://newtab");
  service_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab)));
  EXPECT_EQ(1, web_state_list->count());

  // Open another one.
  GURL url("http://test/2");
  service_->Load(
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(2, web_state_list->count());

  // Check that we had no app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests open a new url in the current incognito tab.
TEST_F(URLLoadingServiceTest, TestOpenInCurrentIncognitoTab) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  ios::ChromeBrowserState* otr_browser_state =
      chrome_browser_state_.get()->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  // Set a new tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_service_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Open otr request with otr service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_service_->Load(params1);

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
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests opening a url in a new incognito tab.
TEST_F(URLLoadingServiceTest, TestOpenInNewIncognitoTab) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
  ASSERT_EQ(0, otr_web_state_list->count());

  ios::ChromeBrowserState* otr_browser_state =
      chrome_browser_state_.get()->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = YES;
  otr_service_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(2, otr_web_state_list->count());

  // Check if we had any app level redirection.
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Test opening a normal url in new tab with incognito service.
TEST_F(URLLoadingServiceTest, TestOpenNormalInNewTabWithIncognitoService) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
  ASSERT_EQ(0, otr_web_state_list->count());

  ios::ChromeBrowserState* otr_browser_state =
      chrome_browser_state_.get()->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  // Send to right service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  otr_service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to wrong service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  otr_service_->Load(params2);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Check that had one app level redirection.
  EXPECT_EQ(1, app_service_->load_new_tab_call_count);
}

// Test opening an incognito url in new tab with normal service.
TEST_F(URLLoadingServiceTest, TestOpenIncognitoInNewTabWithNormalService) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
  ASSERT_EQ(0, otr_web_state_list->count());

  app_service_->currentBrowserState = chrome_browser_state_.get();

  // Send to wrong service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.in_incognito = YES;
  service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Send to right service.
  GURL url2("http://test/2");
  UrlLoadParams params2 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url2));
  params2.in_incognito = NO;
  service_->Load(params2);
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, app_service_->load_new_tab_call_count);
}

// Test opening an incognito url in new tab with normal service using load
// strategy.
TEST_F(URLLoadingServiceTest, TestOpenIncognitoInNewTabWithLoadStrategy) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
  ASSERT_EQ(0, otr_web_state_list->count());

  app_service_->currentBrowserState = chrome_browser_state_.get();

  // Send to normal service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(url1));
  params1.load_strategy = UrlLoadStrategy::ALWAYS_IN_INCOGNITO;
  service_->Load(params1);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(0, otr_web_state_list->count());

  // Check that we had one app level redirection.
  EXPECT_EQ(1, app_service_->load_new_tab_call_count);
}

// Test opening an incognito url in current tab with normal service using load
// strategy.
TEST_F(URLLoadingServiceTest, TestOpenIncognitoInCurrentTabWithLoadStrategy) {
  WebStateList* web_state_list = tab_model_.webStateList;
  ASSERT_EQ(0, web_state_list->count());
  WebStateList* otr_web_state_list = otr_tab_model_.webStateList;
  ASSERT_EQ(0, otr_web_state_list->count());

  // Make app level to be otr.
  ios::ChromeBrowserState* otr_browser_state =
      chrome_browser_state_.get()->GetOffTheRecordChromeBrowserState();
  app_service_->currentBrowserState = otr_browser_state;

  // Set a new incognito tab.
  GURL newtab("chrome://newtab");
  UrlLoadParams new_tab_params =
      UrlLoadParams::InNewTab(web::NavigationManager::WebLoadParams(newtab));
  new_tab_params.in_incognito = YES;
  otr_service_->Load(new_tab_params);
  EXPECT_EQ(0, web_state_list->count());
  EXPECT_EQ(1, otr_web_state_list->count());

  // Send to normal service.
  GURL url1("http://test/1");
  UrlLoadParams params1 =
      UrlLoadParams::InCurrentTab(web::NavigationManager::WebLoadParams(url1));
  params1.load_strategy = UrlLoadStrategy::ALWAYS_IN_INCOGNITO;
  service_->Load(params1);

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
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

}  // namespace

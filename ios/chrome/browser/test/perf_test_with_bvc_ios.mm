// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/test/perf_test_with_bvc_ios.h"

#import <UIKit/UIKit.h>

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller+private.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Subclass of PrerenderController so it isn't actually used. Using a mock for
// this makes cleanup on shutdown simpler, by minimizing the number of profile
// observers registered with the profiles.  The profile observers have to be
// deallocated before the profiles themselves, but in practice it is very hard
// to ensure that happens.  Also, for performance testing, not having the
// PrerenderController makes the test far simpler to analyze.
namespace {
static GURL emptyGurl_ = GURL("foo", 3, url::Parsed(), false);
}

PerfTestWithBVC::PerfTestWithBVC(std::string testGroup)
    : PerfTest(testGroup),
      slow_teardown_(false),
      web_client_(std::make_unique<ChromeWebClient>()),
      provider_(ios::CreateChromeBrowserProvider()),
      browser_state_manager_(
          std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
      web_state_list_(&web_state_list_delegate_),
      otr_web_state_list_(&web_state_list_delegate_) {}

PerfTestWithBVC::PerfTestWithBVC(std::string testGroup,
                                 std::string firstLabel,
                                 std::string averageLabel,
                                 bool isWaterfall,
                                 bool verbose,
                                 bool slowTeardown,
                                 int repeat)
    : PerfTest(testGroup,
               firstLabel,
               averageLabel,
               isWaterfall,
               verbose,
               repeat),
      slow_teardown_(slowTeardown),
      web_client_(std::make_unique<ChromeWebClient>()),
      provider_(ios::CreateChromeBrowserProvider()),
      browser_state_manager_(
          std::make_unique<TestChromeBrowserStateManager>(base::FilePath())),
      web_state_list_(&web_state_list_delegate_),
      otr_web_state_list_(&web_state_list_delegate_) {}

PerfTestWithBVC::~PerfTestWithBVC() {}

void PerfTestWithBVC::SetUp() {
  PerfTest::SetUp();

  // Set up the ChromeBrowserState instances.
  TestChromeBrowserState::Builder test_cbs_builder;
  test_cbs_builder.AddTestingFactory(
      ios::TemplateURLServiceFactory::GetInstance(),
      ios::TemplateURLServiceFactory::GetDefaultFactory());
  test_cbs_builder.AddTestingFactory(
      ios::AutocompleteClassifierFactory::GetInstance(),
      ios::AutocompleteClassifierFactory::GetDefaultFactory());
  chrome_browser_state_ = test_cbs_builder.Build();
  chrome_browser_state_->CreateBookmarkModel(false);
  bookmarks::test::WaitForBookmarkModelToLoad(
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_browser_state_.get()));
  ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(false));

  // Force creation of AutocompleteClassifier instance.
  ios::AutocompleteClassifierFactory::GetForBrowserState(
      chrome_browser_state_.get());

  // Use the session to create a window which will contain the tab models.
  NSString* state_path = base::SysUTF8ToNSString(
      chrome_browser_state_->GetStatePath().AsUTF8Unsafe());
  SessionIOS* session =
      [[SessionServiceIOS sharedService] loadSessionFromDirectory:state_path];
  DCHECK_EQ(session.sessionWindows.count, 1u);

  // Tab models. The off-the-record (OTR) tab model is required for the stack
  // view controller, which is created in OpenStackView().
  tab_model_ =
      [[TabModel alloc] initWithSessionService:[SessionServiceIOS sharedService]
                                  browserState:chrome_browser_state_.get()
                                  webStateList:&web_state_list_];
  [tab_model_ restoreSessionWindow:session.sessionWindows[0]
                 forInitialRestore:YES];
  otr_tab_model_ = [[TabModel alloc]
      initWithSessionService:[SessionServiceIOS sharedService]
                browserState:chrome_browser_state_
                                 ->GetOffTheRecordChromeBrowserState()
                webStateList:&otr_web_state_list_];
  [otr_tab_model_ restoreSessionWindow:session.sessionWindows[0]
                     forInitialRestore:YES];

  browser_ =
      std::make_unique<TestBrowser>(chrome_browser_state_.get(), tab_model_);
  otr_browser_ = std::make_unique<TestBrowser>(
      incognito_chrome_browser_state_.get(), otr_tab_model_);

  command_dispatcher_ = [[CommandDispatcher alloc] init];
  // Create the browser view controller with its testing factory.
  bvc_factory_ = [[BrowserViewControllerDependencyFactory alloc]
      initWithBrowserState:chrome_browser_state_.get()
              webStateList:[tab_model_ webStateList]];
  bvc_ = [[BrowserViewController alloc]
                     initWithBrowser:browser_.get()
                   dependencyFactory:bvc_factory_
          applicationCommandEndpoint:nil
                   commandDispatcher:command_dispatcher_
      browserContainerViewController:[[BrowserContainerViewController alloc]
                                         init]];
  [bvc_ setActive:YES];

  // Create a real window to give to the browser view controller.
  window_ = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [window_ makeKeyAndVisible];
  [window_ addSubview:[bvc_ view]];
  [[bvc_ view] setFrame:[[UIScreen mainScreen] bounds]];
}

void PerfTestWithBVC::TearDown() {
  [[bvc_ tabModel] closeAllTabs];
  [[bvc_ view] removeFromSuperview];

  // Documented example of how to clear out the browser view controller
  // and its associated data.
  window_ = nil;
  [bvc_ shutdown];
  bvc_ = nil;
  bvc_factory_ = nil;
  tab_model_ = nil;
  [otr_tab_model_ disconnect];
  otr_tab_model_ = nil;

  // The base class |TearDown| method calls the run loop so the
  // NSAutoreleasePool can drain. This needs to be done before
  // |chrome_browser_state_| can be cleared. For tests that allocate more
  // objects, more runloop time may be required.
  if (slow_teardown_)
    SpinRunLoop(.5);
  PerfTest::TearDown();

  // The profiles can be deallocated only after the BVC has been deallocated.
  chrome_browser_state_.reset();
}

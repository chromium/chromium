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
#include "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
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
  ASSERT_TRUE(chrome_browser_state_->CreateHistoryService());

  // Force creation of AutocompleteClassifier instance.
  ios::AutocompleteClassifierFactory::GetForBrowserState(
      chrome_browser_state_.get());

  // Use the session to create a window which will contain the tabs.
  NSString* state_path = base::SysUTF8ToNSString(
      chrome_browser_state_->GetStatePath().AsUTF8Unsafe());
  SessionIOS* session =
      [[SessionServiceIOS sharedService] loadSessionWithSessionID:nil
                                                        directory:state_path];
  DCHECK_EQ(session.sessionWindows.count, 1u);

  browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                           &web_state_list_);
  otr_browser_ = std::make_unique<TestBrowser>(
      chrome_browser_state_->GetOffTheRecordChromeBrowserState(),
      &otr_web_state_list_);
  SessionRestorationBrowserAgent::CreateForBrowser(
      browser_.get(), [SessionServiceIOS sharedService]);
  SessionRestorationBrowserAgent::CreateForBrowser(
      otr_browser_.get(), [SessionServiceIOS sharedService]);
  SessionRestorationBrowserAgent::FromBrowser(browser_.get())
      ->RestoreSessionWindow(session.sessionWindows[0]);
  SessionRestorationBrowserAgent::FromBrowser(otr_browser_.get())
      ->RestoreSessionWindow(session.sessionWindows[0]);

  // Create the browser view controller with its testing factory.
  bvc_factory_ = [[BrowserViewControllerDependencyFactory alloc]
      initWithBrowser:browser_.get()];
  bvc_ = [[BrowserViewController alloc]
                     initWithBrowser:browser_.get()
                   dependencyFactory:bvc_factory_
      browserContainerViewController:[[BrowserContainerViewController alloc]
                                         init]
                          dispatcher:browser_->GetCommandDispatcher()];
  [bvc_ setActive:YES];

  // Create a real window to give to the browser view controller.
  window_ = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [window_ makeKeyAndVisible];
  [window_ addSubview:[bvc_ view]];
  [[bvc_ view] setFrame:[[UIScreen mainScreen] bounds]];
}

void PerfTestWithBVC::TearDown() {
  browser_.get()->GetWebStateList()->CloseAllWebStates(
      WebStateList::CLOSE_NO_FLAGS);
  [[bvc_ view] removeFromSuperview];

  // Documented example of how to clear out the browser view controller
  // and its associated data.
  window_ = nil;
  [bvc_ shutdown];
  bvc_ = nil;
  bvc_factory_ = nil;

  // The base class |TearDown| method calls the run loop so the
  // NSAutoreleasePool can drain. This needs to be done before
  // |chrome_browser_state_| can be cleared. For tests that allocate more
  // objects, more runloop time may be required.
  if (slow_teardown_)
    SpinRunLoop(.5);
  PerfTest::TearDown();

  // Before destroying chrome_browser_state_ we need to make sure that no tasks
  // are left on the ThreadPool since they might depend on it.
  task_environment_.RunUntilIdle();

  // The profiles can be deallocated only after the BVC has been deallocated.
  chrome_browser_state_.reset();
}

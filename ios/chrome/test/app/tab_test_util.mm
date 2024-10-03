// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/tab_test_util.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/metrics/model/tab_usage_recorder_browser_agent.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller_testing.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/model/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/open_url_context.h"

namespace chrome_test_util {

namespace {

// Returns the WebStateList for the current mode. Or nullptr of there is no
// browser.
WebStateList* GetCurrentWebStateList() {
  Browser* browser = GetCurrentBrowser();
  return browser ? browser->GetWebStateList() : nullptr;
}

// Close all tabs for `browser` and request the session to be saved.
void CloseAllTabsForBrowser(Browser* browser) {
  DCHECK(browser);
  const int close_flags = WebStateList::CLOSE_USER_ACTION;
  CloseAllWebStates(*browser->GetWebStateList(), close_flags);
  ProfileIOS* profile = browser->GetProfile();
  SessionRestorationServiceFactory::GetForProfile(profile)->SaveSessions();
}

}  // namespace

BOOL IsIncognitoMode() {
  return GetForegroundActiveScene()
      .browserProviderInterface.currentBrowserProvider.browser->GetProfile()
      ->IsOffTheRecord();
}

void OpenNewTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand command];
    if (GetForegroundActiveSceneController().mainCoordinator.isTabGridActive) {
      // The TabGrid is currently presented.
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
      [GetForegroundActiveSceneController()
          addANewTabAndPresentBrowser:GetMainBrowser()
                    withURLLoadParams:params];
      return;
    }
    id<ApplicationCommands, BrowserCommands> handler =
        chrome_test_util::HandlerForActiveBrowser();
    [handler openURLInNewTab:command];
  }
}

void SimulateExternalAppURLOpeningWithURL(NSURL* URL) {
  TestOpenURLContext* context = [[TestOpenURLContext alloc] init];
  context.URL = URL;

  UIApplication* application = UIApplication.sharedApplication;
  UIScene* scene = application.connectedScenes.anyObject;
  [scene.delegate scene:scene openURLContexts:[NSSet setWithObject:context]];
}

void SimulateAddAccountFromWeb() {
  id<ApplicationCommands, BrowserCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN];
  UIViewController* baseViewController =
      GetForegroundActiveScene()
          .browserProviderInterface.mainBrowserProvider.viewController;
  [handler showSignin:command baseViewController:baseViewController];
}

void OpenNewIncognitoTab() {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand incognitoTabCommand];
    if (GetForegroundActiveSceneController().mainCoordinator.isTabGridActive) {
      // The TabGrid is currently presented.
      Browser* browser =
          GetForegroundActiveScene()
              .browserProviderInterface.incognitoBrowserProvider.browser;
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
      [GetForegroundActiveSceneController() addANewTabAndPresentBrowser:browser
                                                      withURLLoadParams:params];
      return;
    }
    id<ApplicationCommands, BrowserCommands> handler =
        chrome_test_util::HandlerForActiveBrowser();
    [handler openURLInNewTab:command];
  }
}

web::WebState* GetCurrentWebState() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  return web_state_list ? web_state_list->GetActiveWebState() : nullptr;
}

web::WebState* GetNextWebState() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list || web_state_list->count() < 2)
    return nullptr;
  int next_index = web_state_list->active_index() + 1;
  if (next_index >= web_state_list->count())
    next_index = 0;
  return web_state_list->GetWebStateAt(next_index);
}

web::WebState* GetWebStateAtIndexInCurrentMode(int index) {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list || !web_state_list->ContainsIndex(index))
    return nullptr;
  return web_state_list->GetWebStateAt(index);
}

NSString* GetCurrentTabTitle() {
  return tab_util::GetTabTitle(GetCurrentWebState());
}

NSString* GetNextTabTitle() {
  return tab_util::GetTabTitle(GetNextWebState());
}

void CloseCurrentTab() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list ||
      web_state_list->active_index() == WebStateList::kInvalidIndex)
    return;
  web_state_list->CloseWebStateAt(web_state_list->active_index(),
                                  WebStateList::CLOSE_USER_ACTION);
}

void PinCurrentTab() {
  WebStateList* web_state_list = GetCurrentWebStateList();
  if (!web_state_list ||
      web_state_list->active_index() == WebStateList::kInvalidIndex) {
    return;
  }
  web_state_list->SetWebStatePinnedAt(web_state_list->active_index(),
                                      /*pinned =*/true);
}

void CloseTabAtIndex(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    DCHECK_LE(index, static_cast<NSUInteger>(INT_MAX));
    GetCurrentWebStateList()->CloseWebStateAt(static_cast<int>(index),
                                              WebStateList::CLOSE_USER_ACTION);
  }
}

NSUInteger GetIndexOfActiveNormalTab() {
  return GetMainBrowser()->GetWebStateList()->active_index();
}

void CloseAllTabsInCurrentMode() {
  CloseAllWebStates(*GetCurrentWebStateList(), WebStateList::CLOSE_USER_ACTION);
}

void CloseAllTabs() {
  if (GetIncognitoTabCount() && GetForegroundActiveSceneController()) {
    CloseAllTabsForBrowser(
        GetForegroundActiveScene()
            .browserProviderInterface.incognitoBrowserProvider.browser);
  }
  if (GetMainTabCount() && GetForegroundActiveScene()) {
    CloseAllTabsForBrowser(GetMainBrowser());
  }
  if (GetInactiveTabCount() && GetForegroundActiveScene()) {
    CloseAllTabsForBrowser(
        GetForegroundActiveScene()
            .browserProviderInterface.mainBrowserProvider.inactiveBrowser);
  }
}

void SelectTabAtIndexInCurrentMode(NSUInteger index) {
  @autoreleasepool {  // Make sure that all internals are deallocated.

    WebStateList* web_state_list = GetCurrentWebStateList();
    web_state_list->ActivateWebStateAt(static_cast<int>(index));
  }
}

NSUInteger GetMainTabCount() {
  return GetMainBrowser() ? GetMainBrowser()->GetWebStateList()->count() : 0;
}

NSUInteger GetInactiveTabCount() {
  return GetForegroundActiveScene()
      .browserProviderInterface.mainBrowserProvider.inactiveBrowser
      ->GetWebStateList()
      ->count();
}

NSUInteger GetIncognitoTabCount() {
  return GetForegroundActiveScene()
      .browserProviderInterface.incognitoBrowserProvider.browser
      ->GetWebStateList()
      ->count();
}

BOOL ResetTabUsageRecorder() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetCurrentBrowser());
  if (!tab_usage_recorder)
    return NO;
  tab_usage_recorder->ResetAll();
  return YES;
}

BOOL SetCurrentTabsToBeColdStartTabs() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetCurrentBrowser());

  if (!tab_usage_recorder)
    return NO;
  WebStateList* web_state_list = GetCurrentWebStateList();

  std::vector<web::WebState*> web_states;
  web_states.reserve(web_state_list->count());
  for (int index = 0; index < web_state_list->count(); ++index) {
    web_states.push_back(web_state_list->GetWebStateAt(index));
  }
  tab_usage_recorder->InitialRestoredTabs(web_state_list->GetActiveWebState(),
                                          web_states);
  return YES;
}

BOOL SimulateTabsBackgrounding() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetCurrentBrowser());
  if (!tab_usage_recorder)
    return NO;
  tab_usage_recorder->AppDidEnterBackground();
  return YES;
}

void EvictOtherBrowserTabs() {
  id<BrowserProviderInterface> provider =
      GetForegroundActiveScene().browserProviderInterface;
  Browser* otherBrowser = IsIncognitoMode()
                              ? provider.mainBrowserProvider.browser
                              : provider.incognitoBrowserProvider.browser;
  // Disabling and enabling web usage will evict all web views.
  WebUsageEnablerBrowserAgent* enabler =
      WebUsageEnablerBrowserAgent::FromBrowser(otherBrowser);
  DCHECK(enabler);
  enabler->SetWebUsageEnabled(false);
  enabler->SetWebUsageEnabled(true);
}

BOOL CloseAllNormalTabs() {
  Browser* browser = GetMainBrowser();
  DCHECK(browser);
  CloseAllWebStates(*browser->GetWebStateList(),
                    WebStateList::CLOSE_USER_ACTION);
  return YES;
}

BOOL CloseAllIncognitoTabs() {
  SceneState* scene_state = GetForegroundActiveScene();
  DCHECK(scene_state);

  Browser* browser =
      scene_state.browserProviderInterface.incognitoBrowserProvider.browser;
  DCHECK(browser);
  CloseAllWebStates(*browser->GetWebStateList(),
                    WebStateList::CLOSE_USER_ACTION);
  return YES;
}

NSUInteger GetEvictedMainTabCount() {
  TabUsageRecorderBrowserAgent* tab_usage_recorder =
      TabUsageRecorderBrowserAgent::FromBrowser(GetMainBrowser());
  if (!tab_usage_recorder)
    return 0;
  return tab_usage_recorder->EvictedTabsMapSize();
}

}  // namespace chrome_test_util

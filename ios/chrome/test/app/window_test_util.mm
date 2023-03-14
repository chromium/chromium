// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/window_test_util.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/main/scene_controller_testing.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

SceneState* GetSceneStateForWindowWithNumber(int windowNumber) {
  NSArray<SceneState*>* connected_scenes =
      GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connected_scenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      return state;
    }
  }
  return nil;
}

id<BrowserInterfaceProvider> GetInterfaceProviderForWindowWithNumber(
    int windowNumber) {
  return GetSceneStateForWindowWithNumber(windowNumber).interfaceProvider;
}

// Returns the browser for the current mode.
Browser* GetCurrentBrowserForWindowWithNumber(int windowNumber) {
  return GetInterfaceProviderForWindowWithNumber(windowNumber)
      .currentInterface.browser;
}

// Returns the WebStateList for the current mode. Or nullptr of there is no
// browser.
WebStateList* GetCurrentWebStateListForWindowWithNumber(int windowNumber) {
  Browser* browser = GetCurrentBrowserForWindowWithNumber(windowNumber);
  return browser ? browser->GetWebStateList() : nullptr;
}

}  // namespace

web::WebState* GetCurrentWebStateForWindowWithNumber(int windowNumber) {
  WebStateList* web_state_list =
      GetCurrentWebStateListForWindowWithNumber(windowNumber);
  return web_state_list ? web_state_list->GetActiveWebState() : nullptr;
}

NSUInteger GetMainTabCountForWindowWithNumber(int windowNumber) {
  return GetInterfaceProviderForWindowWithNumber(windowNumber)
      .mainInterface.browser->GetWebStateList()
      ->count();
}

NSUInteger GetIncognitoTabCountForWindowWithNumber(int windowNumber) {
  return GetInterfaceProviderForWindowWithNumber(windowNumber)
      .incognitoInterface.browser->GetWebStateList()
      ->count();
}

void OpenNewTabInWindowWithNumber(int windowNumber) {
  @autoreleasepool {  // Make sure that all internals are deallocated.
    OpenNewTabCommand* command = [OpenNewTabCommand command];
    SceneController* controller =
        GetSceneStateForWindowWithNumber(windowNumber).controller;
    if (controller.mainCoordinator.isTabGridActive) {
      // The TabGrid is currently presented.
      Browser* browser = GetCurrentBrowserForWindowWithNumber(windowNumber);
      UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
      [controller addANewTabAndPresentBrowser:browser withURLLoadParams:params];
      return;
    }
    id<ApplicationCommands, BrowserCommands> handler =
        static_cast<id<ApplicationCommands, BrowserCommands>>(
            GetCurrentBrowserForWindowWithNumber(windowNumber)
                ->GetCommandDispatcher());
    [handler openURLInNewTab:command];
  }
}

}  // namespace chrome_test_util

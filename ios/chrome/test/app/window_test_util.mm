// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/window_test_util.h"

#import <Foundation/Foundation.h>

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace chrome_test_util {

namespace {

id<BrowserInterfaceProvider> GetInterfaceProviderForWindowWithNumber(
    int windowNumber) {
  NSArray<SceneState*>* connected_scenes =
      GetMainController().appState.connectedScenes;
  NSString* accessibilityIdentifier =
      [NSString stringWithFormat:@"%ld", (long)windowNumber];
  for (SceneState* state in connected_scenes) {
    if ([state.window.accessibilityIdentifier
            isEqualToString:accessibilityIdentifier]) {
      return state.interfaceProvider;
    }
  }
  return nil;
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

}  // namespace chrome_test_util

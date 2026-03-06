// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_shortcut_item.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/browser/intents/model/user_activity_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

@implementation TaskRequestForShortcutItem {
  UIApplicationShortcutItem* _shortcutItem;
  ShortcutCompletionHandler _shortcutHandler;
}

- (instancetype)initWithShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                          sceneState:(SceneState*)sceneState
                             handler:(ShortcutCompletionHandler)handler
                         isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _shortcutItem = shortcutItem;
    _shortcutHandler = handler;
  }
  return self;
}

- (void)execute {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(browser);
  BOOL handledShortcutItem =
      userActivityBrowserAgent->Handle3DTouchApplicationShortcuts(
          _shortcutItem);
  if (_shortcutHandler) {
    _shortcutHandler(handledShortcutItem);
  }
}

@end

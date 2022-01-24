// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarButtonActionsHandler

- (void)backAction {
  self.navigationAgent->GoBack();
}

- (void)forwardAction {
  self.navigationAgent->GoForward();
}

- (void)tabGridTouchDown {
  [self.dispatcher prepareTabSwitcher];
}

- (void)tabGridTouchUp {
  [self.dispatcher displayTabSwitcherInGridLayout];
}

- (void)toolsMenuAction {
  [self.dispatcher showToolsMenuPopup];
}

- (void)shareAction {
  [self.dispatcher sharePage];
}

- (void)reloadAction {
  self.navigationAgent->Reload();
}

- (void)stopAction {
  self.navigationAgent->StopLoading();
}

- (void)searchAction:(id)sender {
  [self.dispatcher closeFindInPage];
  UIView* senderView = base::mac::ObjCCastStrict<UIView>(sender);
  CGPoint center = [senderView.superview convertPoint:senderView.center
                                               toView:nil];
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithIncognito:self.incognito
                                  originPoint:center];
  [self.dispatcher openURLInNewTab:command];
}

- (void)cancelOmniboxFocusAction {
  [self.dispatcher cancelOmniboxEdit];
}

@end

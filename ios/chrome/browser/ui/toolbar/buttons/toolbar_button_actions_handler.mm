// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/toolbar/public/features.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarButtonActionsHandler

- (void)backAction {
  [self.dispatcher goBack];
}

- (void)forwardAction {
  [self.dispatcher goForward];
}

- (void)tabGridTouchDown {
  [self.dispatcher prepareTabSwitcher];
}

- (void)tabGridTouchUp {
  [self.dispatcher displayTabSwitcher];
}

- (void)toolsMenuAction {
  [self.dispatcher showToolsMenuPopup];
}

- (void)shareAction {
  [self.dispatcher sharePage];
}

- (void)reloadAction {
  [self.dispatcher reload];
}

- (void)stopAction {
  [self.dispatcher stopLoading];
}

- (void)bookmarkAction {
  [self.dispatcher bookmarkPage];
}

- (void)searchAction:(id)sender {
  [self.dispatcher closeFindInPage];
  if (base::FeatureList::IsEnabled(kToolbarNewTabButton)) {
    UIView* senderView = base::mac::ObjCCastStrict<UIView>(sender);
    CGPoint center = [senderView.superview convertPoint:senderView.center
                                                 toView:nil];
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithIncognito:self.incognito
                                    originPoint:center];
    [self.dispatcher openURLInNewTab:command];
  } else {
    [self.dispatcher focusOmniboxFromSearchButton];
  }
}

- (void)cancelOmniboxFocusAction {
  [self.dispatcher cancelOmniboxEdit];
}

@end

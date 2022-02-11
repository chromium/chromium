// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/browser_view_controller_dependency_factory.h"

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller_helper.h"
#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowserViewControllerDependencyFactory {
  Browser* _browser;
  ChromeBrowserState* _browserState;
  WebStateList* _webStateList;
}

- (id)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
    _browserState = browser->GetBrowserState();
    _webStateList = browser->GetWebStateList();
  }
  return self;
}

- (BrowserViewControllerHelper*)newBrowserViewControllerHelper {
  return [[BrowserViewControllerHelper alloc] init];
}

- (KeyCommandsProvider*)newKeyCommandsProvider {
  return [[KeyCommandsProvider alloc] init];
}

@end

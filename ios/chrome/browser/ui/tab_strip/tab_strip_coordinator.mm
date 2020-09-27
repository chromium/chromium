// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_strip/tab_strip_coordinator.h"

#include "base/check_op.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/tab_strip/tab_strip_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabStripCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.viewController)
    return;

  self.viewController = [[TabStripViewController alloc] init];
  if (@available(iOS 13, *)) {
    self.viewController.overrideUserInterfaceStyle =
        self.browser->GetBrowserState()->IsOffTheRecord()
            ? UIUserInterfaceStyleDark
            : UIUserInterfaceStyleUnspecified;
  }
}

- (void)stop {
  self.viewController = nil;
}

#pragma mark - Properties

- (void)setLongPressDelegate:(id<PopupMenuLongPressDelegate>)longPressDelegate {
  _longPressDelegate = longPressDelegate;
}

@end

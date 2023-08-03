// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_controller.h"

@protocol TabStripContaining;

@interface TabStripCoordinator ()

// Mediator for updating the TabStrip when the WebStateList changes.
@property(nonatomic, strong) TabStripMediator* mediator;

@property TabStripViewController* tabStripViewController;

@end

@implementation TabStripCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.tabStripViewController)
    return;

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  CHECK(browserState);

  self.tabStripViewController = [[TabStripViewController alloc] init];
  self.tabStripViewController.overrideUserInterfaceStyle =
      browserState->IsOffTheRecord() ? UIUserInterfaceStyleDark
                                     : UIUserInterfaceStyleUnspecified;
  self.tabStripViewController.isOffTheRecord = browserState->IsOffTheRecord();

  self.mediator =
      [[TabStripMediator alloc] initWithConsumer:self.tabStripViewController];
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.browserState = browserState;

  self.tabStripViewController.delegate = self.mediator;
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  self.tabStripViewController = nil;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.tabStripViewController;
}

- (UIView<TabStripContaining>*)view {
  return static_cast<UIView<TabStripContaining>*>(self.viewController.view);
}

#pragma mark - Public

- (void)hideTabStrip:(BOOL)hidden {
  self.tabStripViewController.view.hidden = hidden;
}

@end

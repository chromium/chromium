// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  self.tabStripViewController = [[TabStripViewController alloc] init];
  self.tabStripViewController.overrideUserInterfaceStyle =
      self.browser->GetBrowserState()->IsOffTheRecord()
          ? UIUserInterfaceStyleDark
          : UIUserInterfaceStyleUnspecified;
  self.tabStripViewController.isOffTheRecord =
      self.browser->GetBrowserState()->IsOffTheRecord();

  self.mediator =
      [[TabStripMediator alloc] initWithConsumer:self.tabStripViewController];
  self.mediator.webStateList = self.browser->GetWebStateList();

  self.tabStripViewController.faviconDataSource = self.mediator;
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

- (void)setLongPressDelegate:(id<PopupMenuLongPressDelegate>)longPressDelegate {
  _longPressDelegate = longPressDelegate;
}

- (UIView<TabStripContaining>*)view {
  return static_cast<UIView<TabStripContaining>*>(self.viewController.view);
}

#pragma mark - Public

- (void)hideTabStrip:(BOOL)hidden {
  self.tabStripViewController.view.hidden = hidden;
}

@end

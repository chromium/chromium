// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

@interface TabStripCoordinator () <TabStripViewControllerDelegate>

// Mediator for updating the TabStrip when the WebStateList changes.
@property(nonatomic, strong) TabStripMediator* mediator;

@property TabStripViewController* tabStripViewController;

@end

@implementation TabStripCoordinator {
  SharingCoordinator* _sharingCoordinator;
}

@synthesize baseViewController = _baseViewController;

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
  self.tabStripViewController.delegate = self;
  self.tabStripViewController.overrideUserInterfaceStyle =
      browserState->IsOffTheRecord() ? UIUserInterfaceStyleDark
                                     : UIUserInterfaceStyleUnspecified;

  self.mediator =
      [[TabStripMediator alloc] initWithConsumer:self.tabStripViewController];
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.browserState = browserState;

  self.tabStripViewController.mutator = self.mediator;
}

- (void)stop {
  [_sharingCoordinator stop];
  _sharingCoordinator = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  self.tabStripViewController = nil;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.tabStripViewController;
}

#pragma mark - Public

- (void)hideTabStrip:(BOOL)hidden {
  self.tabStripViewController.view.hidden = hidden;
}

#pragma mark - TabStripViewControllerDelegate

- (void)tabStrip:(TabStripViewController*)tabStrip
       shareItem:(TabSwitcherItem*)item
      originView:(UIView*)originView {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:item.URL
                                   title:item.title
                                scenario:SharingScenario::TabStripItem];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                          params:params
                      originView:originView];
  [_sharingCoordinator start];
}

@end

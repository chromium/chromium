// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_clear_browsing_data_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/history/ui_bundled/history_clear_browsing_data_coordinator_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/referrer.h"

@interface HistoryClearBrowsingDataCoordinator ()

// ViewControllers being managed by this Coordinator.
@property(strong, nonatomic)
    TableViewNavigationController* historyClearBrowsingDataNavigationController;
@property(strong, nonatomic)
    ClearBrowsingDataTableViewController* clearBrowsingDataTableViewController;

@end

@implementation HistoryClearBrowsingDataCoordinator
@synthesize clearBrowsingDataTableViewController =
    _clearBrowsingDataTableViewController;
@synthesize historyClearBrowsingDataNavigationController =
    _historyClearBrowsingDataNavigationController;
@synthesize delegate = _delegate;
@synthesize presentationDelegate = _presentationDelegate;

- (void)start {
  self.clearBrowsingDataTableViewController =
      [[ClearBrowsingDataTableViewController alloc]
          initWithBrowser:self.browser];
  self.clearBrowsingDataTableViewController.extendedLayoutIncludesOpaqueBars =
      YES;
  self.clearBrowsingDataTableViewController.delegate = self;
  // TODO(crbug.com/40670043): Use HandlerForProtocol after commands protocol
  // clean up.
  self.clearBrowsingDataTableViewController.dispatcher =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  // Configure and present ClearBrowsingDataNavigationController.
  self.historyClearBrowsingDataNavigationController =
      [[TableViewNavigationController alloc]
          initWithTable:self.clearBrowsingDataTableViewController];
  self.historyClearBrowsingDataNavigationController.toolbarHidden = YES;

  [self.historyClearBrowsingDataNavigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.historyClearBrowsingDataNavigationController.presentationController
      .delegate = self.clearBrowsingDataTableViewController;

  [self.baseViewController
      presentViewController:self.historyClearBrowsingDataNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stopWithCompletion:(ProceduralBlock)completionHandler {
  if (self.historyClearBrowsingDataNavigationController) {
    [self.clearBrowsingDataTableViewController prepareForDismissal];
    [self.historyClearBrowsingDataNavigationController
        dismissViewControllerAnimated:YES
                           completion:^() {
                             // completionHandler might trigger
                             // dismissHistory:withCompletion:, which will
                             // call stopWithCompletion:, so
                             // historyClearBrowsingDataNavigationController
                             // needs to be nil, otherwise stopWithCompletion:
                             // will call dismiss with nothing to dismiss and
                             // therefore not trigger its own completionHandler.
                             [self.clearBrowsingDataTableViewController stop];
                             self.clearBrowsingDataTableViewController = nil;
                             self.historyClearBrowsingDataNavigationController =
                                 nil;
                             if (completionHandler) {
                               completionHandler();
                             }
                           }];
  } else if (completionHandler) {
    completionHandler();
  }
}

#pragma mark - ClearBrowsingDataUIDelegate

- (void)clearBrowsingDataTableViewController:
            (ClearBrowsingDataTableViewController*)controller
                              wantsToOpenURL:(const GURL&)URL {
  CHECK_EQ(controller, self.clearBrowsingDataTableViewController);
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.load_strategy = self.loadStrategy;
  [self.delegate
      dismissHistoryClearBrowsingData:self
                       withCompletion:^{
                         UrlLoadingBrowserAgent::FromBrowser(self.browser)
                             ->Load(params);
                         [self.presentationDelegate
                                 showActiveRegularTabFromHistory];
                       }];
}

- (void)clearBrowsingDataTableViewControllerWantsDismissal:
    (ClearBrowsingDataTableViewController*)controller {
  CHECK_EQ(controller, self.clearBrowsingDataTableViewController);
  [self.delegate dismissHistoryClearBrowsingData:self withCompletion:nil];
}

- (void)clearBrowsingDataTableViewControllerWasRemoved:
    (ClearBrowsingDataTableViewController*)controller {
  DCHECK_EQ(self.clearBrowsingDataTableViewController, controller);
  [self.delegate dismissHistoryClearBrowsingData:self withCompletion:nil];
}

@end

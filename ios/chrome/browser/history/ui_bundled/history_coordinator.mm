// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/ui_bundled/history_coordinator.h"

#import "ios/chrome/browser/history/ui_bundled/base_history_coordinator+subclassing.h"
#import "ios/chrome/browser/history/ui_bundled/history_clear_browsing_data_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/history_clear_browsing_data_coordinator_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/history_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"

@interface HistoryCoordinator () <HistoryClearBrowsingDataCoordinatorDelegate> {
  // The coordinator that will present Clear Browsing Data.
  HistoryClearBrowsingDataCoordinator* _historyClearBrowsingDataCoordinator;
  // ViewController being managed by this Coordinator.
  TableViewNavigationController* _historyNavigationController;
  HistoryTableViewController* _viewController;
}
@end

@implementation HistoryCoordinator

- (void)start {
  // Initialize and configure HistoryTableViewController.
  _viewController = [[HistoryTableViewController alloc] init];
  _viewController.searchTerms = self.searchTerms;
  _viewController.delegate = self;

  // Configure and present HistoryNavigationController.
  _historyNavigationController =
      [[TableViewNavigationController alloc] initWithTable:_viewController];
  _historyNavigationController.toolbarHidden = NO;

  [_historyNavigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  _historyNavigationController.presentationController.delegate =
      _viewController;

  [super start];

  [self.baseViewController presentViewController:_historyNavigationController
                                        animated:YES
                                      completion:nil];
}

// This method should always execute the `completionHandler`.
- (void)dismissWithCompletion:(ProceduralBlock)completionHandler {
  if (_historyNavigationController) {
    if (_historyClearBrowsingDataCoordinator) {
      [_historyClearBrowsingDataCoordinator stopWithCompletion:^{
        [self dismissHistoryNavigationWithCompletion:completionHandler];
      }];
    } else {
      [self dismissHistoryNavigationWithCompletion:completionHandler];
    }
  } else if (completionHandler) {
    completionHandler();
  }
  [super dismissWithCompletion:completionHandler];
}

- (void)dismissHistoryNavigationWithCompletion:(ProceduralBlock)completion {
  [_historyNavigationController dismissViewControllerAnimated:YES
                                                   completion:completion];
  _historyNavigationController = nil;
  _historyClearBrowsingDataCoordinator = nil;
  _viewController.historyService = nullptr;
}

#pragma mark - HistoryClearBrowsingDataCoordinatorDelegate

- (void)dismissHistoryClearBrowsingData:
            (HistoryClearBrowsingDataCoordinator*)coordinator
                         withCompletion:(ProceduralBlock)completionHandler {
  DCHECK_EQ(_historyClearBrowsingDataCoordinator, coordinator);
  __weak HistoryCoordinator* weakSelf = self;
  [coordinator stopWithCompletion:^() {
    if (completionHandler) {
      completionHandler();
    }
    [weakSelf setHistoryClearBrowsingDataCoordinator:nil];
  }];
}

- (void)displayClearHistoryData {
  CHECK(!IsIosQuickDeleteEnabled());
  if (_historyClearBrowsingDataCoordinator) {
    return;
  }
  _historyClearBrowsingDataCoordinator =
      [[HistoryClearBrowsingDataCoordinator alloc]
          initWithBaseViewController:_historyNavigationController
                             browser:self.browser];
  _historyClearBrowsingDataCoordinator.delegate = self;
  _historyClearBrowsingDataCoordinator.presentationDelegate =
      self.presentationDelegate;
  _historyClearBrowsingDataCoordinator.loadStrategy = self.loadStrategy;
  [_historyClearBrowsingDataCoordinator start];
}

#pragma mark - Setters & Getters

- (BaseHistoryViewController*)viewController {
  return _viewController;
}

- (MenuScenarioHistogram)scenario {
  return kMenuScenarioHistogramHistoryEntry;
}

- (void)setHistoryClearBrowsingDataCoordinator:
    (HistoryClearBrowsingDataCoordinator*)historyClearBrowsingDataCoordinator {
  _historyClearBrowsingDataCoordinator = historyClearBrowsingDataCoordinator;
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_last_visited_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/page_info/core/page_info_action.h"
#import "ios/chrome/browser/history/ui_bundled/base_history_coordinator+subclassing.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/page_info/features.h"
#import "ios/chrome/browser/ui/page_info/page_info_last_visited_view_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_last_visited_view_controller_delegate.h"

@interface PageInfoLastVisitedCoordinator () <
    PageInfoLastVisitedViewControllerDelegate,
    HistoryCoordinatorDelegate> {
  // Only history entries matching "_hostName" will be displayed.
  NSString* _hostName;
  // View controller.
  PageInfoLastVisitedViewController* _viewController;
}

// Coordinator for displaying history.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
@end

@implementation PageInfoLastVisitedCoordinator

#pragma mark - Public

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        hostName:(NSString*)hostName {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _hostName = hostName;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Initialize and configure PageInfoLastVisitedViewController.
  _viewController =
      [[PageInfoLastVisitedViewController alloc] initWithHostName:_hostName];
  _viewController.delegate = self;
  _viewController.lastVisitedDelegate = self;

  [super start];

  CommandDispatcher* _dispatcher = self.browser->GetCommandDispatcher();
  _viewController.pageInfoCommandsHandler =
      HandlerForProtocol(_dispatcher, PageInfoCommands);

  // Display toolbar on the navigation controller.
  _baseNavigationController.toolbarHidden = NO;

  // View Controller needs to be pushed on top of the navigation stack.
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  // Stop the history coordinator.
  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  [super stop];
}

#pragma mark - PageInfoLastVisitedViewControllerDelegate

- (void)displayFullHistory {
  CHECK(IsPageInfoLastVisitedIOSEnabled());
  base::RecordAction(
      base::UserMetricsAction("PageInfo.History.ShowFullHistoryClicked"));
  base::UmaHistogramEnumeration(page_info::kWebsiteSettingsActionHistogram,
                                page_info::PAGE_INFO_SHOW_FULL_HISTORY_CLICKED);
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:_baseNavigationController
                         browser:self.browser];
  self.historyCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  self.historyCoordinator.delegate = self;
  [self.historyCoordinator start];
}

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  // Dismiss both Page Info and Full History.
  [self.delegate closeHistoryWithCompletion:completion];
}

- (void)closeHistory {
  // Dismiss only Full History.
  __weak __typeof(self) weakSelf = self;
  [self.historyCoordinator dismissWithCompletion:^{
    [weakSelf.historyCoordinator stop];
    weakSelf.historyCoordinator = nil;
  }];
}

#pragma mark - Setters & Getters

- (BaseHistoryViewController*)viewController {
  return _viewController;
}

- (MenuScenarioHistogram)scenario {
  return kMenuScenarioHistogramLastVisitedHistoryEntry;
}

@end

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_coordinator.h"

#include "components/history/core/browser/browsing_history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"
#import "ios/chrome/browser/ui/history/history_clear_browsing_data_coordinator.h"
#include "ios/chrome/browser/ui/history/history_local_commands.h"
#import "ios/chrome/browser/ui/history/history_mediator.h"
#include "ios/chrome/browser/ui/history/history_table_view_controller.h"
#import "ios/chrome/browser/ui/history/history_transitioning_delegate.h"
#include "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryCoordinator ()<HistoryLocalCommands> {
  // Provides dependencies and funnels callbacks from BrowsingHistoryService.
  std::unique_ptr<IOSBrowsingHistoryDriver> _browsingHistoryDriver;
  // Abstraction to communicate with HistoryService and WebHistoryService.
  std::unique_ptr<history::BrowsingHistoryService> _browsingHistoryService;
}
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    TableViewNavigationController* historyNavigationController;

@property(nonatomic, strong)
    HistoryTableViewController* historyTableViewController;

// Mediator being managed by this Coordinator.
@property(nonatomic, strong) HistoryMediator* mediator;

// The transitioning delegate used by the history view controller.
@property(nonatomic, strong)
    HistoryTransitioningDelegate* historyTransitioningDelegate;

// The coordinator that will present Clear Browsing Data.
@property(nonatomic, strong)
    HistoryClearBrowsingDataCoordinator* historyClearBrowsingDataCoordinator;
@end

@implementation HistoryCoordinator
@synthesize dispatcher = _dispatcher;
@synthesize historyClearBrowsingDataCoordinator =
    _historyClearBrowsingDataCoordinator;
@synthesize historyNavigationController = _historyNavigationController;
@synthesize historyTransitioningDelegate = _historyTransitioningDelegate;
@synthesize mediator = _mediator;
@synthesize presentationDelegate = _presentationDelegate;

- (void)start {
  // Initialize and configure HistoryTableViewController.
  self.historyTableViewController = [[HistoryTableViewController alloc] init];
  self.historyTableViewController.browserState = self.browserState;
  self.historyTableViewController.loadStrategy = self.loadStrategy;

  // Initialize and set HistoryMediator
  self.mediator =
      [[HistoryMediator alloc] initWithBrowserState:self.browserState];
  self.historyTableViewController.imageDataSource = self.mediator;

  // Initialize and configure HistoryServices.
  _browsingHistoryDriver = std::make_unique<IOSBrowsingHistoryDriver>(
      self.browserState, self.historyTableViewController);
  _browsingHistoryService = std::make_unique<history::BrowsingHistoryService>(
      _browsingHistoryDriver.get(),
      ios::HistoryServiceFactory::GetForBrowserState(
          self.browserState, ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState));
  self.historyTableViewController.historyService =
      _browsingHistoryService.get();

  // Configure and present HistoryNavigationController.
  self.historyNavigationController = [[TableViewNavigationController alloc]
      initWithTable:self.historyTableViewController];
  self.historyNavigationController.toolbarHidden = NO;
  self.historyTableViewController.localDispatcher = self;
  self.historyTableViewController.presentationDelegate =
      self.presentationDelegate;

  BOOL useCustomPresentation = YES;
  if (IsCollectionsCardPresentationStyleEnabled()) {
    if (@available(iOS 13, *)) {
      [self.historyNavigationController
          setModalPresentationStyle:UIModalPresentationFormSheet];
      self.historyNavigationController.presentationController.delegate =
          self.historyTableViewController;
      useCustomPresentation = NO;
    }
  }

  if (useCustomPresentation) {
    self.historyTransitioningDelegate =
        [[HistoryTransitioningDelegate alloc] init];
    self.historyNavigationController.transitioningDelegate =
        self.historyTransitioningDelegate;
    [self.historyNavigationController
        setModalPresentationStyle:UIModalPresentationCustom];
  }
  [self.baseViewController
      presentViewController:self.historyNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

// This method should always execute the |completionHandler|.
- (void)stopWithCompletion:(ProceduralBlock)completionHandler {
  if (self.historyNavigationController) {
    void (^dismissHistoryNavigation)(void) = ^void() {
      // Make sure to stop
      // |self.historyTableViewController.contextMenuCoordinator| before
      // dismissing, or |self.historyNavigationController| will dismiss that
      // instead of itself.
      [self.historyTableViewController.contextMenuCoordinator stop];
      [self.historyNavigationController
          dismissViewControllerAnimated:YES
                             completion:completionHandler];
      self.historyNavigationController = nil;
      _browsingHistoryDriver = nullptr;
      _browsingHistoryService = nullptr;
    };
    if (self.historyClearBrowsingDataCoordinator) {
      [self.historyClearBrowsingDataCoordinator stopWithCompletion:^() {
        dismissHistoryNavigation();
        self.historyClearBrowsingDataCoordinator = nil;
      }];

    } else {
      dismissHistoryNavigation();
    }
  } else if (completionHandler) {
    completionHandler();
  }
}

#pragma mark - HistoryLocalCommands

- (void)dismissHistoryWithCompletion:(ProceduralBlock)completionHandler {
  [self stopWithCompletion:completionHandler];
}

- (void)displayPrivacySettings {
    self.historyClearBrowsingDataCoordinator =
        [[HistoryClearBrowsingDataCoordinator alloc]
            initWithBaseViewController:self.historyNavigationController
                          browserState:self.browserState];
    self.historyClearBrowsingDataCoordinator.localDispatcher = self;
    self.historyClearBrowsingDataCoordinator.presentationDelegate =
        self.presentationDelegate;
    self.historyClearBrowsingDataCoordinator.loadStrategy = self.loadStrategy;
    self.historyClearBrowsingDataCoordinator.dispatcher = self.dispatcher;
    [self.historyClearBrowsingDataCoordinator start];
}

@end

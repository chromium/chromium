// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_clear_browsing_data_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/ui/history/history_local_commands.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_local_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#import "ios/web/public/navigation/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistoryClearBrowsingDataCoordinator ()<
    UIViewControllerTransitioningDelegate,
    TableViewPresentationControllerDelegate>

// ViewControllers being managed by this Coordinator.
@property(strong, nonatomic)
    TableViewNavigationController* historyClearBrowsingDataNavigationController;
@property(strong, nonatomic)
    ClearBrowsingDataTableViewController* clearBrowsingDataTableViewController;

@end

@implementation HistoryClearBrowsingDataCoordinator
@synthesize clearBrowsingDataTableViewController =
    _clearBrowsingDataTableViewController;
@synthesize dispatcher = _dispatcher;
@synthesize historyClearBrowsingDataNavigationController =
    _historyClearBrowsingDataNavigationController;
@synthesize localDispatcher = _localDispatcher;
@synthesize presentationDelegate = _presentationDelegate;

- (void)start {
  self.clearBrowsingDataTableViewController =
      [[ClearBrowsingDataTableViewController alloc]
          initWithBrowserState:self.browserState];
  self.clearBrowsingDataTableViewController.extendedLayoutIncludesOpaqueBars =
      YES;
  self.clearBrowsingDataTableViewController.localDispatcher = self;
  self.clearBrowsingDataTableViewController.dispatcher = self.dispatcher;
  // Configure and present ClearBrowsingDataNavigationController.
  self.historyClearBrowsingDataNavigationController =
      [[TableViewNavigationController alloc]
          initWithTable:self.clearBrowsingDataTableViewController];
  self.historyClearBrowsingDataNavigationController.toolbarHidden = YES;

  BOOL useCustomPresentation = YES;
  if (IsCollectionsCardPresentationStyleEnabled()) {
    if (@available(iOS 13, *)) {
      [self.historyClearBrowsingDataNavigationController
          setModalPresentationStyle:UIModalPresentationFormSheet];
      self.historyClearBrowsingDataNavigationController.presentationController
          .delegate = self.clearBrowsingDataTableViewController;
      useCustomPresentation = NO;
    }
  }

  if (useCustomPresentation) {
    // Stacks on top of history "bubble" for non-compact devices.
    self.historyClearBrowsingDataNavigationController.transitioningDelegate =
        self;
    self.historyClearBrowsingDataNavigationController.modalPresentationStyle =
        UIModalPresentationCustom;
    self.historyClearBrowsingDataNavigationController.modalTransitionStyle =
        UIModalTransitionStyleCoverVertical;
  }

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
                             // dismissHistoryWithCompletion, which will call
                             // stopWithCompletion:, so
                             // historyClearBrowsingDataNavigationController
                             // needs to be nil, otherwise stopWithCompletion:
                             // will call dismiss with nothing to dismiss and
                             // therefore not trigger its own completionHandler.
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

#pragma mark - ClearBrowsingDataLocalCommands

- (void)openURL:(const GURL&)URL {
  DCHECK(self.historyClearBrowsingDataNavigationController);
  UrlLoadParams params = UrlLoadParams::InNewTab(URL);
  params.load_strategy = self.loadStrategy;
  [self stopWithCompletion:^() {
    [self.localDispatcher dismissHistoryWithCompletion:^{
      UrlLoadingServiceFactory::GetForBrowserState(self.browserState)
          ->Load(params);
      [self.presentationDelegate showActiveRegularTabFromHistory];
    }];
  }];
}

- (void)dismissClearBrowsingData {
  DCHECK(self.historyClearBrowsingDataNavigationController);
  [self stopWithCompletion:nil];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  TableViewPresentationController* controller =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  controller.modalDelegate = self;
  return controller;
}

#pragma mark - TableViewPresentationControllerDelegate

- (BOOL)presentationControllerShouldDismissOnTouchOutside:
    (TableViewPresentationController*)controller {
  return YES;
}

- (void)presentationControllerWillDismiss:
    (TableViewPresentationController*)controller {
  [self stopWithCompletion:nil];
}

@end

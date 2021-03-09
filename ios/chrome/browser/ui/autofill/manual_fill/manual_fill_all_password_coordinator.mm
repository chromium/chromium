// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_all_password_coordinator.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_animator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillAllPasswordCoordinator () <
    ManualFillPasswordMediatorDelegate,
    PasswordViewControllerDelegate>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

@end

@implementation ManualFillAllPasswordCoordinator

- (void)start {
  [super start];
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.passwordViewController = [[PasswordViewController alloc]
      initWithSearchController:searchController];
  self.passwordViewController.contentInsetsAlwaysEqualToSafeArea = YES;
  self.passwordViewController.delegate = self;

  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      self.browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS);
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.passwordMediator =
      [[ManualFillPasswordMediator alloc] initWithPasswordStore:passwordStore
                                                  faviconLoader:faviconLoader];
  [self.passwordMediator fetchPasswordsForURL:GURL::EmptyGURL()];
  self.passwordMediator.actionSectionEnabled = NO;
  self.passwordMediator.consumer = self.passwordViewController;
  self.passwordMediator.contentInjector = self.injectionHandler;
  self.passwordMediator.delegate = self;

  self.passwordViewController.imageDataSource = self.passwordMediator;

  searchController.searchResultsUpdater = self.passwordMediator;

  TableViewNavigationController* navigationController =
      [[TableViewNavigationController alloc]
          initWithTable:self.passwordViewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  navigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.passwordViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.passwordViewController = nil;
  self.passwordMediator = nil;
  [super stop];
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.passwordViewController;
}

#pragma mark - ManualFillPasswordMediatorDelegate

- (void)manualFillPasswordMediatorWillInjectContent:
    (ManualFillPasswordMediator*)mediator {
  [self stop];  // The job is done.
}

#pragma mark - PasswordViewControllerDelegate

- (void)passwordViewControllerDidTapDoneButton:
    (PasswordViewController*)passwordViewController {
  [self stop];  // The job is done.
}

@end

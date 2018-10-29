// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_coordinator.h"

#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_animator.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace manual_fill {

NSString* const PasswordDoneButtonAccessibilityIdentifier =
    @"kManualFillPasswordDoneButtonAccessibilityIdentifier";

}  // namespace manual_fill

@interface ManualFillPasswordCoordinator ()<
    PasswordListDelegate,
    UIViewControllerTransitioningDelegate,
    UIPopoverPresentationControllerDelegate>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

// The view controller modally presented where the user can select one of their
// passwords. Owned by the view controllers hierarchy.
@property(nonatomic, weak) PasswordViewController* allPasswordsViewController;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong)
    ManualFillInjectionHandler* manualFillInjectionHandler;

// Button presenting this coordinator in a popover. Used for continuation after
// dismissing any presented view controller. iPad only.
@property(nonatomic, weak) UIButton* presentingButton;

@end

@implementation ManualFillPasswordCoordinator

@synthesize allPasswordsViewController = _allPasswordsViewController;
@synthesize manualFillInjectionHandler = _manualFillInjectionHandler;
@synthesize passwordMediator = _passwordMediator;
@synthesize passwordViewController = _passwordViewController;

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
              webStateList:(WebStateList*)webStateList
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _passwordViewController =
        [[PasswordViewController alloc] initWithSearchController:nil];
    _passwordViewController.contentInsetsAlwaysEqualToSafeArea = YES;
    _manualFillInjectionHandler = injectionHandler;

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState, ServiceAccessType::EXPLICIT_ACCESS);
    _passwordMediator =
        [[ManualFillPasswordMediator alloc] initWithWebStateList:webStateList
                                                   passwordStore:passwordStore];
    _passwordMediator.consumer = _passwordViewController;
    _passwordMediator.navigationDelegate = self;
    _passwordMediator.contentDelegate = _manualFillInjectionHandler;
  }
  return self;
}

- (void)stop {
  if (IsIPadIdiom() && self.passwordViewController.presentingViewController) {
    [self.passwordViewController dismissViewControllerAnimated:true
                                                    completion:nil];
  } else {
    [self.passwordViewController.view removeFromSuperview];
  }
  [self.allPasswordsViewController dismissViewControllerAnimated:YES
                                                      completion:nil];
}

- (UIViewController*)viewController {
  return self.passwordViewController;
}

- (void)presentFromButton:(UIButton*)button {
  self.presentingButton = button;
  self.passwordViewController.modalPresentationStyle =
      UIModalPresentationPopover;

  // The |button.window.rootViewController| is used in order to present above
  // the keyboard. This way the popover will be dismissed on keyboard
  // interaction and it won't be covered when the keyboard is near the top of
  // the screen.
  [button.window.rootViewController
      presentViewController:self.passwordViewController
                   animated:YES
                 completion:nil];

  UIPopoverPresentationController* popoverPresentationController =
      self.passwordViewController.popoverPresentationController;
  popoverPresentationController.sourceView = button;
  popoverPresentationController.sourceRect = button.bounds;
  popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp | UIMenuControllerArrowDown;
  popoverPresentationController.delegate = self;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  TableViewPresentationController* presentationController =
      [[TableViewPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  presentationController.position = TablePresentationPositionLeading;
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  UITraitCollection* traitCollection = presenting.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = YES;
  animator.direction = TableAnimatorDirectionFromLeading;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  UITraitCollection* traitCollection = dismissed.traitCollection;
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    // Use the default animator for fullscreen presentations.
    return nil;
  }

  TableViewAnimator* animator = [[TableViewAnimator alloc] init];
  animator.presenting = NO;
  animator.direction = TableAnimatorDirectionFromLeading;
  return animator;
}

#pragma mark - PasswordListDelegate

- (void)openAllPasswordsList {
  // On iPad, first dismiss the popover before the new view is presented.
  __weak __typeof(self) weakSelf = self;
  if (IsIPadIdiom() && self.passwordViewController.presentingViewController) {
    [self.passwordViewController
        dismissViewControllerAnimated:true
                           completion:^{
                             [weakSelf openAllPasswordsList];
                           }];
    return;
  }
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  searchController.searchResultsUpdater = self.passwordMediator;

  PasswordViewController* allPasswordsViewController = [
      [PasswordViewController alloc] initWithSearchController:searchController];
  self.passwordMediator.disableFilter = YES;
  self.passwordMediator.consumer = allPasswordsViewController;
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissPresentedViewController)];
  doneButton.accessibilityIdentifier =
      manual_fill::PasswordDoneButtonAccessibilityIdentifier;
  allPasswordsViewController.navigationItem.rightBarButtonItem = doneButton;
  self.allPasswordsViewController = allPasswordsViewController;

  TableViewNavigationController* navigationController =
      [[TableViewNavigationController alloc]
          initWithTable:allPasswordsViewController];
  navigationController.transitioningDelegate = self;
  [navigationController setModalPresentationStyle:UIModalPresentationCustom];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)dismissPresentedViewController {
  // Dismiss the full screen view controller and present the pop over.
  __weak __typeof(self) weakSelf = self;
  [self.allPasswordsViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           if (weakSelf.presentingButton) {
                             [weakSelf
                                 presentFromButton:weakSelf.presentingButton];
                           }
                         }];
}

- (void)openPasswordSettings {
  // On iPad, dismiss the popover before the settings are presented.
  if (IsIPadIdiom() && self.passwordViewController.presentingViewController) {
    [self.passwordViewController
        dismissViewControllerAnimated:true
                           completion:^{
                             [self openPasswordSettings];
                           }];
    return;
  }
  [self.delegate openPasswordSettings];
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  [self.delegate resetAccessoryView];
}

@end

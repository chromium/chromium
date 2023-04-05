// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_user_education_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The duration for the presentation/dismissal animation of the inactive tabs
// view.
const NSTimeInterval kDuration = 0.2;

// NSUserDefaults key to check whether the user education screen has ever been
// shown. The associated value in user defaults is a BOOL.
NSString* const kInactiveTabsUserEducationShownOnce =
    @"InactiveTabsUserEducationShownOnce";

}  // namespace

@interface InactiveTabsCoordinator () <
    GridViewControllerDelegate,
    InactiveTabsCommands,
    InactiveTabsUserEducationCoordinatorDelegate,
    InactiveTabsViewControllerDelegate,
    SettingsNavigationControllerDelegate>

// The view controller displaying the inactive tabs.
@property(nonatomic, strong) InactiveTabsViewController* viewController;

// The mediator handling the inactive tabs.
@property(nonatomic, strong) InactiveTabsMediator* mediator;

// The mutually exclusive constraints for placing `viewController`.
@property(nonatomic, strong) NSLayoutConstraint* hiddenConstraint;
@property(nonatomic, strong) NSLayoutConstraint* visibleConstraint;

// The potential user education coordinator shown the first time Inactive Tabs
// are displayed.
@property(nonatomic, strong)
    InactiveTabsUserEducationCoordinator* userEducationCoordinator;

@end

@implementation InactiveTabsCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  DCHECK(IsInactiveTabsEnabled());
  return [super initWithBaseViewController:viewController browser:browser];
}

- (void)start {
  [super start];

  // Create the view controller.
  self.viewController = [[InactiveTabsViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.gridViewController.delegate = self;

  // Create the mediator.
  SnapshotCache* snapshotCache =
      SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_cache();
  self.mediator = [[InactiveTabsMediator alloc]
      initWithConsumer:self.viewController.gridViewController
        commandHandler:self
          webStateList:self.browser->GetWebStateList()
           prefService:GetApplicationContext()->GetLocalState()
         snapshotCache:snapshotCache];
  self.viewController.gridViewController.imageDataSource = self.mediator;
  self.viewController.gridViewController.menuProvider = self.menuProvider;
}

- (void)show {
  // Add the view controller to the hierarchy.
  UIView* baseView = self.baseViewController.view;
  UIView* view = self.viewController.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:self.viewController];
  [baseView addSubview:view];
  [self.viewController didMoveToParentViewController:self.baseViewController];

  self.hiddenConstraint =
      [baseView.trailingAnchor constraintEqualToAnchor:view.leadingAnchor];
  self.visibleConstraint =
      [baseView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [baseView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [baseView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [baseView.widthAnchor constraintEqualToAnchor:view.widthAnchor],
    self.hiddenConstraint,
  ]];

  [baseView layoutIfNeeded];
  [UIView animateWithDuration:kDuration
      animations:^{
        self.hiddenConstraint.active = NO;
        self.visibleConstraint.active = YES;
        [baseView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        [self startUserEducationIfNeeded];
      }];
}

- (void)hide {
  UIView* baseView = self.baseViewController.view;

  [baseView layoutIfNeeded];
  [UIView animateWithDuration:kDuration
      animations:^{
        self.visibleConstraint.active = NO;
        self.hiddenConstraint.active = YES;
        [baseView layoutIfNeeded];
      }
      completion:^(BOOL success) {
        [self.viewController willMoveToParentViewController:nil];
        [self.viewController.view removeFromSuperview];
        [self.viewController removeFromParentViewController];
        self.visibleConstraint = nil;
        self.hiddenConstraint = nil;
      }];
}

- (void)stop {
  [super stop];

  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;

  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
  self.visibleConstraint = nil;
  self.hiddenConstraint = nil;
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID {
  [self.delegate inactiveTabsCoordinator:self didSelectItemWithID:itemID];
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
}

- (void)gridViewController:(GridViewController*)gridViewController
        didCloseItemWithID:(NSString*)itemID {
  [self.mediator closeItemWithID:itemID];
}

- (void)didTapPlusSignInGridViewController:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)gridViewController:(GridViewController*)gridViewController
         didMoveItemWithID:(NSString*)itemID
                   toIndex:(NSUInteger)destinationIndex {
  NOTREACHED();
}

- (void)gridViewController:(GridViewController*)gridViewController
        didChangeItemCount:(NSUInteger)count {
  // No op.
}

- (void)gridViewController:(GridViewController*)gridViewController
       didRemoveItemWIthID:(NSString*)itemID {
  // No op.
}

- (void)didChangeLastItemVisibilityInGridViewController:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewController:(GridViewController*)gridViewController
    contentNeedsAuthenticationChanged:(BOOL)needsAuth {
  NOTREACHED();
}

- (void)gridViewControllerWillBeginDragging:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDragSessionWillBegin:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDragSessionDidEnd:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerScrollViewDidScroll:
    (GridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDropAnimationWillBegin:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)gridViewControllerDropAnimationDidEnd:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (GridViewController*)gridViewController {
  NOTREACHED();
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (GridViewController*)gridViewController {
  [self presentSettings];
}

#pragma mark - InactiveTabsCommands

- (void)inactiveTabsExplicitlyDisabledByUser {
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
}

#pragma mark - InactiveTabsUserEducationCoordinatorDelegate

- (void)inactiveTabsUserEducationCoordinatorDidTapSettingsButton:
    (InactiveTabsUserEducationCoordinator*)
        inactiveTabsUserEducationCoordinator {
  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;
  [self presentSettings];
}

- (void)inactiveTabsUserEducationCoordinatorDidFinish:
    (InactiveTabsUserEducationCoordinator*)
        inactiveTabsUserEducationCoordinator {
  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;
}

#pragma mark - InactiveTabsViewControllerDelegate

- (void)inactiveTabsViewControllerDidTapBackButton:
    (InactiveTabsViewController*)inactiveTabsViewController {
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
}

- (void)inactiveTabsViewController:
            (InactiveTabsViewController*)inactiveTabsViewController
    didTapCloseAllInactiveBarButtonItem:(UIBarButtonItem*)barButtonItem {
  NSInteger numberOfTabs = [self.mediator numberOfItems];
  DCHECK_GT(numberOfTabs, 0);

  NSString* title;
  if (numberOfTabs > 99) {
    title = l10n_util::GetNSString(
        IDS_IOS_INACTIVE_TABS_CLOSE_ALL_CONFIRMATION_MANY);
  } else {
    title = base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
        IDS_IOS_INACTIVE_TABS_CLOSE_ALL_CONFIRMATION, numberOfTabs));
  }
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_INACTIVE_TABS_CLOSE_ALL_CONFIRMATION_MESSAGE);

  ActionSheetCoordinator* actionSheetCoordinator =
      [[ActionSheetCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                               title:title
                             message:message
                       barButtonItem:barButtonItem];

  __weak __typeof(self) weakSelf = self;
  NSString* closeAllActionTitle = l10n_util::GetNSString(
      IDS_IOS_INACTIVE_TABS_CLOSE_ALL_CONFIRMATION_OPTION);
  [actionSheetCoordinator addItemWithTitle:closeAllActionTitle
                                    action:^{
                                      [weakSelf closeAllInactiveTabs];
                                    }
                                     style:UIAlertActionStyleDestructive];

  [actionSheetCoordinator start];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

- (void)settingsWasDismissed {
  // No-op.
}

- (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
    handlerForSettings {
  NOTREACHED();
  return nil;
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  NOTREACHED();
  return nil;
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  NOTREACHED();
  return nil;
}

#pragma mark - Private

// Called when the Inactive Tabs grid is shown, to start the user education
// coordinator. If the user education screen was ever presented, this is a
// no-op.
- (void)startUserEducationIfNeeded {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults boolForKey:kInactiveTabsUserEducationShownOnce]) {
    return;
  }

  // Start the user education coordinator.
  self.userEducationCoordinator = [[InactiveTabsUserEducationCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:nullptr];
  self.userEducationCoordinator.delegate = self;
  [self.userEducationCoordinator start];

  // Record the presentation.
  [defaults setBool:YES forKey:kInactiveTabsUserEducationShownOnce];
}

// Called when the user confirmed wanting to close all inactive tabs.
- (void)closeAllInactiveTabs {
  [self.delegate inactiveTabsCoordinatorDidFinish:self];
  [self.mediator closeAllItems];
}

// Presents the Inactive Tabs settings modally in their own navigation
// controller.
- (void)presentSettings {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          inactiveTabsControllerForBrowser:self.browser
                                  delegate:self];
  [self.baseViewController presentViewController:settingsController
                                        animated:YES
                                      completion:nil];
}

@end

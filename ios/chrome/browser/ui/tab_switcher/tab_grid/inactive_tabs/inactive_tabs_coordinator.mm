// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator.h"

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/tabs_closer.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_user_education_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state_id.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

// A view that can be dimmed continusouly between no dimming and being fully
// dimmed (the view is then fully black).
@interface DimmableSnapshot : UIView

// How much to dim the view.
// A value of 0.0 shows the snapshot with no dimming. A value of 1.0 shows a
// totally dimmed view.
// Default is 0.0.
@property(nonatomic) CGFloat dimming;

// Returns a dimmable view representing `view` as it is snapshot.
- (instancetype)initWithView:(UIView*)view;

@end

@implementation DimmableSnapshot {
  UIView* _snapshotView;
  UIView* _dimmingView;
}

- (instancetype)initWithView:(UIView*)view {
  self = [super initWithFrame:view.frame];
  if (self) {
    _snapshotView = [view snapshotViewAfterScreenUpdates:YES];
    _snapshotView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_snapshotView];
    AddSameConstraints(self, _snapshotView);

    _dimmingView = [[UIView alloc] init];
    _dimmingView.backgroundColor = UIColor.blackColor;
    _dimmingView.translatesAutoresizingMaskIntoConstraints = NO;
    _dimmingView.alpha = 0;
    [self addSubview:_dimmingView];
    AddSameConstraints(self, _dimmingView);
  }
  return self;
}

- (CGFloat)dimming {
  return _dimmingView.alpha;
}

- (void)setDimming:(CGFloat)dimming {
  _dimmingView.alpha = dimming;
}

@end

namespace {

// Presentation/dismissal animation constants for the inactive tabs view.
const NSTimeInterval kDuration = 0.5;
const CGFloat kSpringDamping = 1.0;
const CGFloat kInitialSpringVelocity = 1.0;
const CGFloat kDimming = 0.2;
const CGFloat kParallaxDisplacement = 100;
// The minimum horizontal velocity to the trailing edge that will dismiss the
// view controller, no matter it's current swiped position.
const CGFloat kMinForwardVelocityToDismiss = 100;
// The minimum horizontal velocity to the leading edge that will cancel the
// dismissal of the view controller, when the swiped position is already more
// than half of the screen's width.
const CGFloat kMinBackwardVelocityToCancelDismiss = 10;
// When the inactive tabs grid would be emptied (last inactive tab, or closing
// all inactive tabs via the confirmation dialog), the Inactive Tabs grid is
// popped, but to avoid having it emptied immediately (producing a glitch),
// delay the closing of the tab(s) in the mediator.
const base::TimeDelta kPopUIDelay = base::Seconds(0.3);

}  // namespace

@interface InactiveTabsCoordinator () <
    GridViewControllerDelegate,
    InactiveTabsUserEducationCoordinatorDelegate,
    InactiveTabsViewControllerDelegate,
    SettingsNavigationControllerDelegate>

// The view controller displaying the inactive tabs.
@property(nonatomic, strong) InactiveTabsViewController* viewController;

// The mediator handling the inactive tabs.
@property(nonatomic, strong) InactiveTabsMediator* mediator;

// The constraints for placing `viewController` horizontally.
@property(nonatomic, strong) NSLayoutConstraint* horizontalPosition;

// Whether the view controller is shown. It is true inbetween calls to `-show`
// and `-hide`.
@property(nonatomic, getter=isShowing) BOOL showing;

// The snapshot of the base view prior to showing Inactive Tabs.
@property(nonatomic, strong) DimmableSnapshot* baseViewSnapshot;

// The horizontal position of `baseViewSnapshot`. Change the constant to move
// `baseViewSnapshot`.
@property(nonatomic, strong)
    NSLayoutConstraint* baseViewSnapshotHorizontalPosition;

// Whether settings are currently presented.
@property(nonatomic, getter=isPresetingSettings) BOOL presentingSettings;

// The optional user education coordinator shown the first time Inactive Tabs
// are displayed.
@property(nonatomic, strong)
    InactiveTabsUserEducationCoordinator* userEducationCoordinator;

@end

@implementation InactiveTabsCoordinator {
  // Delegate for dismissing the coordinator.
  __weak id<InactiveTabsCoordinatorDelegate> _delegate;

  // Provides the context menu for the tabs on the grid.
  TabContextMenuHelper* _contextMenuProvider;

  // The navigation controller for inactive tabs settings.
  SettingsNavigationController* _settingsController;

  ActionSheetCoordinator* _actionSheetCoordinator;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  delegate:(id<InactiveTabsCoordinatorDelegate>)
                                               delegate {
  CHECK(IsInactiveTabsAvailable());
  CHECK(delegate);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (id<GridCommands>)gridCommandsHandler {
  return self.mediator;
}

- (id<GridToolbarsConfigurationProvider>)toolbarsConfigurationProvider {
  return self.mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  _contextMenuProvider = [[TabContextMenuHelper alloc]
             initWithProfile:self.browser->GetActiveBrowser()->GetProfile()
      tabContextMenuDelegate:self.tabContextMenuDelegate];

  Browser* browser = self.browser;
  SnapshotStorageWrapper* snapshotStorage =
      SnapshotBrowserAgent::FromBrowser(browser)->snapshot_storage();
  self.mediator = [[InactiveTabsMediator alloc]
      initWithWebStateList:browser->GetWebStateList()
               prefService:GetApplicationContext()->GetLocalState()
           snapshotStorage:snapshotStorage
                tabsCloser:std::make_unique<TabsCloser>(
                               browser, TabsCloser::ClosePolicy::kAllTabs)];
}

- (void)show {
  if (self.showing) {
    return;
  }
  self.showing = YES;
  base::RecordAction(base::UserMetricsAction("MobileInactiveTabGridEntered"));

  // Create the view controller.
  self.viewController = [[InactiveTabsViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.gridViewController.delegate = self;

  UIScreenEdgePanGestureRecognizer* edgeSwipeRecognizer =
      [[UIScreenEdgePanGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(onEdgeSwipe:)];
  edgeSwipeRecognizer.edges = UIRectEdgeLeft;
  [self.viewController.view addGestureRecognizer:edgeSwipeRecognizer];

  self.mediator.consumer = self.viewController.gridViewController;

  self.viewController.gridViewController.menuProvider = _contextMenuProvider;

  // Add the Inactive Tabs view controller to the hierarchy.
  UIView* baseView = self.baseViewController.view;
  UIView* view = self.viewController.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController addChildViewController:self.viewController];
  [baseView addSubview:view];
  [self.viewController didMoveToParentViewController:self.baseViewController];

  // Place the Inactive Tabs view controller.
  self.horizontalPosition = [view.leadingAnchor
      constraintEqualToAnchor:baseView.leadingAnchor
                     constant:CGRectGetWidth(baseView.bounds)];
  [NSLayoutConstraint activateConstraints:@[
    [view.topAnchor constraintEqualToAnchor:baseView.topAnchor],
    [view.bottomAnchor constraintEqualToAnchor:baseView.bottomAnchor],
    [view.widthAnchor constraintEqualToAnchor:baseView.widthAnchor],
    self.horizontalPosition,
  ]];

  // Add the dimmable snapshot of the base view.
  DimmableSnapshot* snapshot = [[DimmableSnapshot alloc] initWithView:baseView];
  snapshot.translatesAutoresizingMaskIntoConstraints = NO;
  [baseView insertSubview:snapshot belowSubview:view];
  self.baseViewSnapshot = snapshot;

  // Place the dimmable snapshot.
  self.baseViewSnapshotHorizontalPosition =
      [snapshot.centerXAnchor constraintEqualToAnchor:baseView.centerXAnchor];

  [NSLayoutConstraint activateConstraints:@[
    [snapshot.widthAnchor constraintEqualToAnchor:baseView.widthAnchor],
    [snapshot.heightAnchor constraintEqualToAnchor:baseView.heightAnchor],
    [snapshot.centerYAnchor constraintEqualToAnchor:baseView.centerYAnchor],
    self.baseViewSnapshotHorizontalPosition,
  ]];

  [self animateIn];
}

- (void)hide {
  if (!self.showing) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileInactiveTabGridExited"));

  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;
  if (self.presentingSettings) {
    [self closeSettings];
  }
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
  [self.viewController.gridViewController dismissModals];

  // Unhide the snapshot.
  self.baseViewSnapshot.hidden = NO;

  [self animateOut];
}

- (void)stop {
  [super stop];

  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;
  [self dismissActionSheetCoordinator];

  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didSelectItemWithID:(web::WebStateID)itemID {
  base::RecordAction(base::UserMetricsAction("MobileTabGridOpenInactiveTab"));
  [_delegate inactiveTabsCoordinator:self didSelectItemWithID:itemID];
  [self didFinish];
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
            didSelectGroup:(const TabGroup*)group {
  NOTREACHED();
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
        didCloseItemWithID:(web::WebStateID)itemID {
  __weak __typeof(self) weakSelf = self;
  auto closeItem = ^{
    [weakSelf.mediator closeItemWithID:itemID];
  };

  NSInteger numberOfTabs = [self.mediator numberOfItems];
  // If it is the latest item, pop the view (UI change), and defer the model
  // change after the UI is no longer visible.
  if (numberOfTabs <= 1) {
    // Pop the view controller.
    [self didFinish];
    // To prevent the Inactive Tabs grid from being immediately emptied, defer
    // the closing to after the view is popped.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(closeItem), kPopUIDelay);
  } else {
    // Otherwise, close the item immediately.
    closeItem();
  }
}

- (void)gridViewControllerDidMoveItem:
    (BaseGridViewController*)gridViewController {
  NOTREACHED_IN_MIGRATION();
}

- (void)gridViewController:(BaseGridViewController*)gridViewController
       didRemoveItemWithID:(web::WebStateID)itemID {
  // No op.
}

- (void)gridViewControllerDragSessionWillBeginForTab:
    (BaseGridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDragSessionWillBeginForTabGroup:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDragSessionDidEnd:
    (BaseGridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerScrollViewDidScroll:
    (BaseGridViewController*)gridViewController {
  // No op.
}

- (void)gridViewControllerDropAnimationWillBegin:
    (BaseGridViewController*)gridViewController {
  NOTREACHED_IN_MIGRATION();
}

- (void)gridViewControllerDropAnimationDidEnd:
    (BaseGridViewController*)gridViewController {
  NOTREACHED_IN_MIGRATION();
}

- (void)didTapInactiveTabsButtonInGridViewController:
    (BaseGridViewController*)gridViewController {
  NOTREACHED_IN_MIGRATION();
}

- (void)didTapInactiveTabsSettingsLinkInGridViewController:
    (BaseGridViewController*)gridViewController {
  [self presentSettings];
}

- (void)gridViewControllerDidRequestContextMenu:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDropSessionDidEnter:
    (BaseGridViewController*)gridViewController {
  // No-op.
}

- (void)gridViewControllerDropSessionDidExit:
    (BaseGridViewController*)gridViewController {
  // No-op.
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
  [self didFinish];
}

- (void)inactiveTabsViewController:
            (InactiveTabsViewController*)inactiveTabsViewController
    didTapCloseAllInactiveBarButtonItem:(UIBarButtonItem*)barButtonItem {
  NSInteger numberOfTabs = [self.mediator numberOfItems];
  if (numberOfTabs <= 0) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileInactiveTabsCloseAll"));

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

  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message
                   barButtonItem:barButtonItem];

  __weak __typeof(self) weakSelf = self;
  NSString* closeAllActionTitle = l10n_util::GetNSString(
      IDS_IOS_INACTIVE_TABS_CLOSE_ALL_CONFIRMATION_OPTION);
  [_actionSheetCoordinator
      addItemWithTitle:closeAllActionTitle
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileInactiveTabsCloseAllConfirm"));
                  [weakSelf closeAllInactiveTabs];
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDestructive];
  [_actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                action:^{
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];
  [_actionSheetCoordinator start];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:^{
                                                [weakSelf onSettingsDismissed];
                                              }];
}

- (void)settingsWasDismissed {
  // This is called when the settings are swiped away by the user.
  // `settingsWasDismissed` is not called after programmatically calling
  // `closeSettings`, so call the completion here.
  [self onSettingsDismissed];
}

- (id<ApplicationCommands, BrowserCommands>)handlerForSettings {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (id<ApplicationCommands>)handlerForApplicationCommands {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (id<SnackbarCommands>)handlerForSnackbarCommands {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

#pragma mark - Actions

- (void)onEdgeSwipe:(UIScreenEdgePanGestureRecognizer*)edgeSwipeRecognizer {
  UIView* baseView = self.baseViewController.view;
  CGFloat horizontalPosition =
      [edgeSwipeRecognizer translationInView:baseView].x;
  CGFloat horizontalVelocity = [edgeSwipeRecognizer velocityInView:baseView].x;
  CGFloat fractionComplete =
      horizontalPosition / CGRectGetWidth(baseView.bounds);

  switch (edgeSwipeRecognizer.state) {
    case UIGestureRecognizerStateBegan:
      // Unhide the snapshot.
      self.baseViewSnapshot.hidden = NO;
      break;
    case UIGestureRecognizerStateChanged:
      self.horizontalPosition.constant = horizontalPosition;
      self.baseViewSnapshotHorizontalPosition.constant =
          -kParallaxDisplacement * (1 - fractionComplete);
      self.baseViewSnapshot.dimming = kDimming * (1 - fractionComplete);
      break;
    case UIGestureRecognizerStateEnded:
      if (horizontalVelocity > kMinForwardVelocityToDismiss) {
        [self animateOut];
      } else {
        if (horizontalVelocity < -kMinBackwardVelocityToCancelDismiss) {
          [self animateIn];
        } else {
          if (horizontalPosition > CGRectGetWidth(baseView.bounds) / 2) {
            [self animateOut];
          } else {
            [self animateIn];
          }
        }
      }
      break;
    case UIGestureRecognizerStateCancelled:
      [self animateIn];
      break;
    default:
      break;
  }
}

#pragma mark - Private

// Called when inactive tabs should be dismissed.
- (void)didFinish {
  [_delegate inactiveTabsCoordinatorDidFinish:self];
}

- (void)dismissActionSheetCoordinator {
  [_actionSheetCoordinator stop];
  _actionSheetCoordinator = nil;
}

// Called to make the Inactive Tabs grid appear in an animation.
- (void)animateIn {
  UIView* baseView = self.baseViewController.view;

  // Trigger a layout, to take into account the changes to the hierarchy prior
  // to animating.
  [baseView layoutIfNeeded];

  // Animate.
  [UIView animateWithDuration:kDuration
      delay:0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:kInitialSpringVelocity
      options:0
      animations:^{
        // Make the Inactive Tabs view controller appear.
        self.horizontalPosition.constant = 0;

        // Make the dimmable snapshot move a little, to give the parallax
        // effect.
        self.baseViewSnapshotHorizontalPosition.constant =
            -kParallaxDisplacement;
        // And dim the snapshot.
        self.baseViewSnapshot.dimming = kDimming;

        // Trigger a layout, to animate constraints changes.
        [baseView layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        // Hide the snapshot. The snapshot is supposed to be overlaid by the
        // Inactive Tabs view controller, but it happened sometimes that the
        // animation of the Inactive Tabs view controller left it just 1 pixel
        // off of the edge, letting the snapshot visible underneath.
        self.baseViewSnapshot.hidden = YES;

        // Once appeared, potentially display the user education screen.
        [self startUserEducationIfNeeded];
      }];
}

// Called to make the Inactive Tabs grid disappear in an animation.
- (void)animateOut {
  UIView* baseView = self.baseViewController.view;

  // Trigger a layout, to take into account the changes to the hierarchy prior
  // to animating.
  [baseView layoutIfNeeded];

  // Animate.
  [UIView animateWithDuration:kDuration
      delay:0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:kInitialSpringVelocity
      options:0
      animations:^{
        // Make the Inactive Tabs view controller disappear.
        self.horizontalPosition.constant = CGRectGetWidth(baseView.bounds);

        // Reset the dimmable snapshot position.
        self.baseViewSnapshotHorizontalPosition.constant = 0;
        // And undim the snapshot.
        self.baseViewSnapshot.dimming = 0;

        // Trigger a layout, to animate constraints changes.
        [baseView layoutIfNeeded];
      }
      completion:^(BOOL success) {
        [self.viewController willMoveToParentViewController:nil];
        [self.viewController.view removeFromSuperview];
        [self.viewController removeFromParentViewController];
        self.horizontalPosition = nil;
        [self.baseViewSnapshot removeFromSuperview];
        self.baseViewSnapshot = nil;
        self.showing = NO;
        self.mediator.consumer = nil;
        self.viewController = nil;
      }];
}

// Called when the Inactive Tabs grid is shown, to start the user education
// coordinator. If the user education screen was ever presented, this is a
// no-op.
- (void)startUserEducationIfNeeded {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if ([defaults boolForKey:kInactiveTabsUserEducationShownOnceKey]) {
    return;
  }

  // Start the user education coordinator.
  self.userEducationCoordinator = [[InactiveTabsUserEducationCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:nullptr];
  self.userEducationCoordinator.delegate = self;
  [self.userEducationCoordinator start];

  // Record the presentation.
  [defaults setBool:YES forKey:kInactiveTabsUserEducationShownOnceKey];
}

// Called when the user confirmed wanting to close all inactive tabs.
- (void)closeAllInactiveTabs {
  [self didFinish];
  // To prevent the Inactive Tabs grid from being immediately emptied, defer the
  // closing to after the view is popped.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf.mediator closeAllItems];
      }),
      kPopUIDelay);
}

// Presents the Inactive Tabs settings modally in their own navigation
// controller.
- (void)presentSettings {
  _settingsController = [SettingsNavigationController
      inactiveTabsControllerForBrowser:self.browser
                              delegate:self];
  [self.viewController presentViewController:_settingsController
                                    animated:YES
                                  completion:nil];
  self.presentingSettings = YES;
}

// Called when Inactive Tabs settings are dismissed.
- (void)onSettingsDismissed {
  self.presentingSettings = NO;
  [_settingsController cleanUpSettings];
  _settingsController = nil;
  [self popIfNeeded];
}

// Tells the delegate this coordinator did finish if it was showing its view
// controller and had no item left.
- (void)popIfNeeded {
  if ([self.mediator numberOfItems] == 0 && self.showing) {
    [self didFinish];
  }
}

@end

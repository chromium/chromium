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
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_user_education_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

// NSUserDefaults key to check whether the user education screen has ever been
// shown. The associated value in user defaults is a BOOL.
NSString* const kInactiveTabsUserEducationShownOnce =
    @"InactiveTabsUserEducationShownOnce";

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

// Optional block called when settings are dismissed. This is because there
// sometimes is work that needs to be delayed between the time the settings are
// changed, and when the UI is updated.
@property(nonatomic, copy) ProceduralBlock onSettingsDismissedBlock;

// The optional user education coordinator shown the first time Inactive Tabs
// are displayed.
@property(nonatomic, strong)
    InactiveTabsUserEducationCoordinator* userEducationCoordinator;

@end

@implementation InactiveTabsCoordinator {
  // Delegate for dismissing the coordinator.
  __weak id<InactiveTabsCoordinatorDelegate> _delegate;

  // Provides the context menu for the tabs on the grid.
  __weak id<TabContextMenuProvider> _menuProvider;
}

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                      delegate:(id<InactiveTabsCoordinatorDelegate>)delegate
                  menuProvider:(id<TabContextMenuProvider>)menuProvider {
  CHECK(IsInactiveTabsAvailable());
  CHECK(menuProvider);
  CHECK(delegate);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _delegate = delegate;
    _menuProvider = menuProvider;
  }
  return self;
}

- (id<GridCommands>)gridCommandsHandler {
  return self.mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  // Create the mediator.
  SessionRestorationBrowserAgent* sessionRestorationBrowserAgent =
      SessionRestorationBrowserAgent::FromBrowser(self.browser);
  SnapshotBrowserAgent* snapshotBrowserAgent =
      SnapshotBrowserAgent::FromBrowser(self.browser);
  sessions::TabRestoreService* tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  self.mediator = [[InactiveTabsMediator alloc]
         initWithWebStateList:self.browser->GetWebStateList()
                  prefService:GetApplicationContext()->GetLocalState()
      sessionRestorationAgent:sessionRestorationBrowserAgent
                snapshotAgent:snapshotBrowserAgent
            tabRestoreService:tabRestoreService];
}

- (void)show {
  if (self.showing) {
    return;
  }
  self.showing = YES;

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

  self.viewController.gridViewController.menuProvider = _menuProvider;

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

  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;
  if (self.presentingSettings) {
    [self closeSettings];
  }
  [self.viewController.gridViewController dismissModals];

  // Unhide the snapshot.
  self.baseViewSnapshot.hidden = NO;

  [self animateOut];
}

- (void)stop {
  [super stop];

  [self.userEducationCoordinator stop];
  self.userEducationCoordinator = nil;

  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - GridViewControllerDelegate

- (void)gridViewController:(GridViewController*)gridViewController
       didSelectItemWithID:(NSString*)itemID {
  base::RecordAction(base::UserMetricsAction("MobileTabGridOpenInactiveTab"));
  [_delegate inactiveTabsCoordinator:self didSelectItemWithID:itemID];
  [_delegate inactiveTabsCoordinatorDidFinish:self];
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
  // Close the Inactive Tabs view when closing the last inactive tab.
  if (count == 0 && self.showing) {
    __weak __typeof(self) weakSelf = self;
    ProceduralBlock didFinish = ^{
      InactiveTabsCoordinator* strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      [strongSelf->_delegate inactiveTabsCoordinatorDidFinish:strongSelf];
    };

    // Delay updating the UI if settings are presented.
    if (self.presentingSettings) {
      self.onSettingsDismissedBlock = didFinish;
    } else {
      didFinish();
    }
  }
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
  [_delegate inactiveTabsCoordinatorDidFinish:self];
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
  [actionSheetCoordinator
      addItemWithTitle:closeAllActionTitle
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileInactiveTabsCloseAllConfirm"));
                  [weakSelf closeAllInactiveTabs];
                }
                 style:UIAlertActionStyleDestructive];

  [actionSheetCoordinator start];
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
  [_delegate inactiveTabsCoordinatorDidFinish:self];
  [self.mediator closeAllItems];
}

// Presents the Inactive Tabs settings modally in their own navigation
// controller.
- (void)presentSettings {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          inactiveTabsControllerForBrowser:self.browser
                                  delegate:self];
  [self.viewController presentViewController:settingsController
                                    animated:YES
                                  completion:nil];
  self.presentingSettings = YES;
}

// Called when Inactive Tabs settings are dismissed.
- (void)onSettingsDismissed {
  self.presentingSettings = NO;
  if (self.onSettingsDismissedBlock) {
    self.onSettingsDismissedBlock();
    self.onSettingsDismissedBlock = nil;
  }
}

@end

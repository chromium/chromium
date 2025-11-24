// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_coordinator.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_drs_view_controller.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/omnibox_focus_orchestrator_parity.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/omnibox_position_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_omnibox_consumer.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/ui_bundled/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_coordinatee.h"
#import "ios/chrome/browser/toolbar/ui_bundled/toolbar_mediator.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/web_state.h"

namespace {

/// The padding necessary for the edit state compact bottom omnibox.
constexpr CGFloat kLocationBarCompactBottomPadding = 10.0;

}  // namespace

@interface ToolbarCoordinator () <GuidedTourCommands,
                                  LocationBarCoordinatorHeightDelegate,
                                  PrimaryToolbarViewControllerDelegate,
                                  ToolbarCommands,
                                  ToolbarMediatorDelegate>

/// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
/// Coordinator for the location bar containing the omnibox.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;
/// Coordinator for the primary toolbar at the top of the screen.
@property(nonatomic, strong)
    PrimaryToolbarCoordinator* primaryToolbarCoordinator;
/// Coordinator for the secondary toolbar at the bottom of the screen.
@property(nonatomic, strong)
    SecondaryToolbarCoordinator* secondaryToolbarCoordinator;

/// Mediator observing WebStateList for toolbars.
@property(nonatomic, strong) ToolbarMediator* toolbarMediator;
/// Orchestrator for the omnibox focus animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;
/// Whether the omnibox is currently focused.
@property(nonatomic, assign) BOOL locationBarFocused;
/// The height of the location bar in edit state.
@property(nonatomic, assign) CGFloat locationBarEditStateHeight;
/// Dynamic response system view controller is an omnibox presenter. Only
/// defined  when kOmniboxDRSPrototype is set.
@property(nonatomic, strong) OmniboxDRSViewController* drsViewController;

@end

@implementation ToolbarCoordinator {
  /// Type of toolbar containing the omnibox. Unlike
  /// `_steadyStateOmniboxPosition`, this tracks the omnibox position at all
  /// time.
  ToolbarType _omniboxPosition;
  /// Type of the toolbar that contains the omnibox when it's not focused. The
  /// animation of focusing/defocusing the omnibox changes depending on this
  /// position.
  ToolbarType _steadyStateOmniboxPosition;
  /// Whether the omnibox focusing should happen with animation.
  BOOL _enableAnimationsForOmniboxFocus;
  //// Indicates whether the focus came from a tap on the NTP's fakebox.
  BOOL _focusedFromFakebox;
  /// Indicates whether the fakebox was pinned on last signal to focus from
  /// the fakebox.
  BOOL _fakeboxPinned;
  /// Command handler for showing the IPH.
  id<HelpCommands> _helpHandler;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    // Initialize both coordinators here as they might be referenced before
    // `start`.
    _primaryToolbarCoordinator =
        [[PrimaryToolbarCoordinator alloc] initWithBrowser:browser];
    _secondaryToolbarCoordinator =
        [[SecondaryToolbarCoordinator alloc] initWithBrowser:browser];

    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ToolbarCommands)];

    _helpHandler =
        HandlerForProtocol(browser->GetCommandDispatcher(), HelpCommands);
  }
  return self;
}

- (void)start {
  if (self.started) {
    return;
  }
  _enableAnimationsForOmniboxFocus = YES;
  // Set a default position, overriden by `setInitialOmniboxPosition` below.
  _omniboxPosition = ToolbarType::kPrimary;

  Browser* browser = self.browser;
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(FakeboxFocuser)];

  if (IsBestOfAppGuidedTourEnabled()) {
    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(GuidedTourCommands)];
  }

  if (base::FeatureList::IsEnabled(kOmniboxDRSPrototype)) {
    self.drsViewController = [[OmniboxDRSViewController alloc] init];
    self.drsViewController.proxiedPresenterDelegate =
        self.popupPresenterDelegate;
    self.popupPresenterDelegate = self.drsViewController;
  }

  segmentation_platform::DeviceSwitcherResultDispatcher* deviceSwitcherResult =
      nullptr;
  if (!browser->GetProfile()->IsOffTheRecord()) {
    deviceSwitcherResult =
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetDispatcherForProfile(browser->GetProfile());
  }
  self.toolbarMediator = [[ToolbarMediator alloc]
      initWithWebStateList:browser->GetWebStateList()
               isIncognito:browser->GetProfile()->IsOffTheRecord()];
  self.toolbarMediator.delegate = self;
  self.toolbarMediator.deviceSwitcherResultDispatcher = deviceSwitcherResult;

  self.locationBarCoordinator =
      [[LocationBarCoordinator alloc] initWithBrowser:browser];
  self.locationBarCoordinator.delegate = self.omniboxFocusDelegate;
  self.locationBarCoordinator.heightDelegate = self;
  self.locationBarCoordinator.popupPresenterDelegate =
      self.popupPresenterDelegate;
  [self.locationBarCoordinator start];
  self.toolbarMediator.omniboxConsumer =
      self.locationBarCoordinator.toolbarOmniboxConsumer;

  self.primaryToolbarCoordinator.viewControllerDelegate = self;
  self.primaryToolbarCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [self.primaryToolbarCoordinator start];
  self.secondaryToolbarCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [self.secondaryToolbarCoordinator start];

  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    self.orchestrator = [[OmniboxFocusOrchestratorParity alloc] init];
  } else {
    self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];
  }

  [self updateOrchestratorAnimatee];

  if (IsBottomOmniboxAvailable()) {
    [self.toolbarMediator setInitialOmniboxPosition];
  } else {
    [self.primaryToolbarCoordinator
        setLocationBarViewController:self.locationBarCoordinator
                                         .locationBarViewController];
  }

  if (IsPageActionMenuEnabled()) {
    [self.locationBarCoordinator setPageActionMenuEntryPointDispatcher];
  }

  [self updateToolbarsLayout];
  [self updateLocationBarHeightWithAnimation:NO focusStateDidChange:NO];

  [super start];
  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }
  [super stop];
  self.orchestrator.editViewAnimatee = nil;
  self.orchestrator.locationBarAnimatee = nil;
  self.orchestrator = nil;

  [self.primaryToolbarCoordinator stop];
  self.primaryToolbarCoordinator.viewControllerDelegate = nil;
  self.primaryToolbarCoordinator = nil;

  [self.secondaryToolbarCoordinator stop];
  self.secondaryToolbarCoordinator = nil;

  [self.locationBarCoordinator stop];
  self.locationBarCoordinator.popupPresenterDelegate = nil;
  self.locationBarCoordinator = nil;

  [self.toolbarMediator disconnect];
  self.toolbarMediator.omniboxConsumer = nil;
  self.toolbarMediator.delegate = nil;
  self.toolbarMediator.deviceSwitcherResultDispatcher = nullptr;
  self.toolbarMediator = nil;

  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  self.started = NO;
}

#pragma mark - Public

- (UIViewController*)baseViewController {
  return self.primaryToolbarCoordinator.baseViewController;
}

- (void)setBaseViewController:(UIViewController*)baseViewController {
  self.primaryToolbarCoordinator.baseViewController = baseViewController;
  self.secondaryToolbarCoordinator.baseViewController = baseViewController;
}

- (UIViewController*)primaryToolbarViewController {
  return self.primaryToolbarCoordinator.viewController;
}

- (UIViewController*)secondaryToolbarViewController {
  return self.secondaryToolbarCoordinator.viewController;
}

- (id<SharingPositioner>)sharingPositioner {
  return self.primaryToolbarCoordinator.SharingPositioner;
}

// Public and in `ToolbarMediatorDelegate`.
- (void)updateToolbar {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return;
  }

  // Please note, this notion of isLoading is slightly different from WebState's
  // IsLoading().
  BOOL isToolbarLoading =
      webState->IsLoading() &&
      !webState->GetLastCommittedURL().SchemeIs(kChromeUIScheme);

  if (self.isLoadingPrerenderer && isToolbarLoading) {
    for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
      [coordinator showPrerenderingAnimation];
    }
  }

  id<FindInPageCommands> findInPageCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), FindInPageCommands);
  [findInPageCommandsHandler showFindUIIfActive];

  id<TextZoomCommands> textZoomCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TextZoomCommands);
  [textZoomCommandsHandler showTextZoomUIIfActive];

  BOOL isNTP = IsVisibleURLNewTabPage(webState);
  BOOL isOffTheRecord = self.isOffTheRecord;
  BOOL canShowTabStrip = CanShowTabStrip(self.traitEnvironment);

  // Hide the toolbar when displaying content suggestions without the tab
  // strip, without the focused omnibox, only when in split toolbar mode.
  BOOL hideToolbar = isNTP && !isOffTheRecord && ![self inEditState] &&
                     !canShowTabStrip &&
                     IsSplitToolbarMode(self.traitEnvironment);

  self.primaryToolbarViewController.view.hidden = hideToolbar;
}

- (BOOL)isLoadingPrerenderer {
  if (!_started) {
    return NO;
  }

  PrerenderBrowserAgent* prerenderBrowserAgent =
      PrerenderBrowserAgent::FromBrowser(self.browser);
  return prerenderBrowserAgent && prerenderBrowserAgent->IsInsertingPrerender();
}

#pragma mark Omnibox and LocationBar

- (void)transitionToLocationBarFocusedState:(BOOL)focused
                                 completion:(ProceduralBlock)completion {
  // Disable infobarBanner overlays when focusing the omnibox as they overlap
  // with primary toolbar.
  OverlayPresentationContext* infobarBannerContext =
      OverlayPresentationContext::FromBrowser(self.browser,
                                              OverlayModality::kInfobarBanner);
  if (infobarBannerContext) {
    infobarBannerContext->SetUIDisabled(focused);
  }

  if (self.traitEnvironment.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassUnspecified) {
    return;
  }
  [self.toolbarMediator locationBarFocusChangedTo:focused];

  // Disable toolbar animations when focusing the omnibox on secondary toolbar.
  ToolbarType editStatePosition;
  if (omnibox::ShouldFocusedOmniboxFollowSteadyStatePosition()) {
    editStatePosition = _steadyStateOmniboxPosition;
  } else if (omnibox::ForceBottomOmniboxInEditState()) {
    if (IsCompactHeight(self.traitEnvironment.traitCollection)) {
      editStatePosition = ToolbarType::kPrimary;
    } else {
      editStatePosition = ToolbarType::kSecondary;
    }
  } else {
    editStatePosition = ToolbarType::kPrimary;
  }

  BOOL animateTransition = _enableAnimationsForOmniboxFocus &&
                           (editStatePosition == _steadyStateOmniboxPosition);

  __weak __typeof(self) weakSelf = self;
  BOOL toolbarExpanded = focused && !CanShowTabStrip(self.traitEnvironment);
  if (base::FeatureList::IsEnabled(kOmniboxDRSPrototype) && focused) {
    [self.baseViewController presentViewController:self.drsViewController
                                          animated:YES
                                        completion:nil];

    return;

  } else {
    [self.orchestrator
        transitionToStateOmniboxFocused:focused
                        toolbarExpanded:toolbarExpanded
                                trigger:[self omniboxFocusTrigger]
                               animated:animateTransition
                             completion:^{
                               [weakSelf focusTransitionDidComplete:focused
                                                         completion:completion];
                             }];
  }

  [self.primaryToolbarCoordinator.viewController setLocationBarFocused:focused];
  [self.secondaryToolbarCoordinator.viewController
      setLocationBarFocused:focused];
  self.locationBarFocused = focused;
  [self updateLocationBarHeightWithAnimation:YES focusStateDidChange:YES];
}

- (BOOL)isOmniboxFirstResponder {
  return [self.locationBarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.locationBarCoordinator showingOmniboxPopup];
}

- (BOOL)inEditState {
  return [self isOmniboxFirstResponder] || [self showingOmniboxPopup];
}

- (void)setBottomOmniboxOffsetForPopup:(CGFloat)bottomOffset {
  [self.toolbarMediator setBottomOmniboxOffsetForPopup:bottomOffset];
}

- (ToolbarType)omniboxPosition {
  return _omniboxPosition;
}

#pragma mark ToolbarHeightProviding

- (CGFloat)collapsedPrimaryToolbarHeight {
  if (IsDiamondPrototypeEnabled() &&
      _omniboxPosition == ToolbarType::kPrimary) {
    return kDiamondCollapsedToolbarHeight;
  }

  if (_omniboxPosition == ToolbarType::kSecondary) {
    // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
    // zero. This is a temporary fix for the pdf bug.
    return 1.0;
  }

  return ToolbarCollapsedHeight(
      self.traitEnvironment.traitCollection.preferredContentSizeCategory);
}

- (CGFloat)expandedPrimaryToolbarHeight {
  if (IsDiamondPrototypeEnabled() &&
      _omniboxPosition == ToolbarType::kPrimary) {
    return kDiamondToolbarHeight;
  }

  CGFloat height =
      self.primaryToolbarViewController.view.intrinsicContentSize.height;
  if (!IsSplitToolbarMode(self.traitEnvironment) ||
      CanShowTabStrip(self.traitEnvironment)) {
    // When the adaptive toolbar is unsplit or the tab strip is visible, add a
    // margin.
    height += kTopToolbarUnsplitMargin;
  }
  return height;
}

- (CGFloat)collapsedSecondaryToolbarHeight {
  if (_omniboxPosition == ToolbarType::kSecondary) {
    return ToolbarCollapsedHeight(
        self.traitEnvironment.traitCollection.preferredContentSizeCategory);
  }
  return 0.0;
}

- (CGFloat)expandedSecondaryToolbarHeight {
  BOOL presentInEditState =
      self.locationBarFocused && omnibox::ForceBottomOmniboxInEditState();
  BOOL showsSecondaryToolbarHeight =
      IsSplitToolbarMode(self.traitEnvironment) || presentInEditState;
  if (!showsSecondaryToolbarHeight) {
    return 0.0;
  }
  CGFloat height =
      self.secondaryToolbarViewController.view.intrinsicContentSize.height;
  if (IsDiamondPrototypeEnabled()) {
    height = 0;
  }
  if (_omniboxPosition == ToolbarType::kSecondary) {
    height += ToolbarExpandedHeight(
        self.traitEnvironment.traitCollection.preferredContentSizeCategory);
  }
  return height;
}

- (CGFloat)locationBarCompactDisplayHeight {
  return self.locationBarCoordinator.locationBarViewController.view.frame.size
             .height +
         kLocationBarCompactBottomPadding;
}

#pragma mark - FakeboxFocuser

- (void)focusOmniboxNoAnimation {
  _enableAnimationsForOmniboxFocus = NO;
  [self.locationBarCoordinator focusOmniboxFromFakebox];
  _enableAnimationsForOmniboxFocus = YES;
  // If the pasteboard is containing a URL, the omnibox popup suggestions are
  // displayed as soon as the omnibox is focused.
  // If the fake omnibox animation is triggered at the same time, it is possible
  // to see the NTP going up where the real omnibox should be displayed.
  if ([self.locationBarCoordinator omniboxPopupHasAutocompleteResults]) {
    [self onFakeboxAnimationComplete];
  }
}

- (void)focusOmniboxFromFakebox:(BOOL)fromFakebox
                            pinned:(BOOL)pinned
    fakeboxButtonsSnapshotProvider:
        (id<FakeboxButtonsSnapshotProvider>)provider {
  _focusedFromFakebox = fromFakebox;
  _fakeboxPinned = pinned;
  [self.locationBarCoordinator setFakeboxButtonsSnapshotProvider:provider];
  [self.locationBarCoordinator focusOmniboxFromFakebox];
}

- (void)onFakeboxBlur {
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (webState && IsVisibleURLNewTabPage(webState)) {
    self.primaryToolbarViewController.view.hidden =
        IsSplitToolbarMode(self.traitEnvironment) &&
        !CanShowTabStrip(self.traitEnvironment);
  }
}

- (void)onFakeboxAnimationComplete {
  self.primaryToolbarViewController.view.hidden = NO;
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setScrollProgressForTabletOmnibox:progress];
  }
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  return self.locationBarCoordinator.omniboxScribbleForwardingTarget;
}

- (void)didNavigateToNTPOnActiveWebState {
  [self.toolbarMediator didNavigateToNTPOnActiveWebState];
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForOverflowMenuIPHDisplayed];
  }
}

- (void)updateUIForIPHDismissed {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForIPHDismissed];
  }
}

- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater setOverflowMenuBlueDot:hasBlueDot];
  }
}

#pragma mark - PrimaryToolbarViewControllerDelegate

- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection {
  if (!_started) {
    return;
  }
  [self updateToolbarsLayout];
}

- (void)close {
  if (self.locationBarFocused) {
    id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationCommandsHandler dismissModalDialogsWithCompletion:nil];
  }
}

- (void)locationBarExpandedInViewController:
    (PrimaryToolbarViewController*)viewController {
  // Do nothing.
}
- (void)locationBarContractedInViewController:
    (PrimaryToolbarViewController*)viewController {
  // Do nothing.
}

- (void)viewController:(PrimaryToolbarViewController*)viewController
    tabGroupIndicatorVisibilityUpdated:(BOOL)visible {
  // Do nothing.
}

- (ToolbarCancelButtonStyle)styleForCancelButtonInToolbar {
  BOOL userPreferenceBottom =
      _toolbarMediator.preferredOmniboxPosition == ToolbarType::kSecondary;
  BOOL followSteadyState =
      omnibox::ShouldFocusedOmniboxFollowSteadyStatePosition();
  BOOL forcedBottomInEditState = omnibox::ForceBottomOmniboxInEditState();
  BOOL inTheBottomInEditState =
      (followSteadyState && userPreferenceBottom) || forcedBottomInEditState;
  if (inTheBottomInEditState) {
    return ToolbarCancelButtonStyle::kXCircle;
  }

  return ToolbarCancelButtonStyle::kCancelLabel;
}

#pragma mark - SideSwipeToolbarInteracting

- (BOOL)isInsideToolbar:(CGPoint)point {
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    // The toolbar frame is inset by -1 because CGRectContainsPoint does
    // include points on the max X and Y edges, which will happen frequently
    // with edge swipes from the right side.
    CGRect toolbarFrame =
        CGRectInset([coordinator viewController].view.bounds, -1, -1);
    CGPoint pointInToolbarCoordinates =
        [[coordinator viewController].view convertPoint:point fromView:nil];
    if (CGRectContainsPoint(toolbarFrame, pointInToolbarCoordinates)) {
      return YES;
    }
  }
  return NO;
}

#pragma mark - SideSwipeToolbarSnapshotProviding

- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState
                                withToolbarType:(ToolbarType)toolbarType {
  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:toolbarType];

  [adaptiveToolbarCoordinator updateToolbarForSideSwipeSnapshot:webState];
  [self updateLocationBarForSideSwipeSnapshot:webState];

  UIView* toolbarView = adaptiveToolbarCoordinator.viewController.view;
  // The toolbar must be in the view hierarchy to be snapshotted.
  if (!toolbarView.window) {
    return nil;
  }
  UIImage* toolbarSnapshot = CaptureViewWithOption(
      toolbarView, [[UIScreen mainScreen] scale], kClientSideRendering);

  [adaptiveToolbarCoordinator resetToolbarAfterSideSwipeSnapshot];
  [self resetLocationBarAfterSideSwipeSnapshot];

  return toolbarSnapshot;
}

#pragma mark SideSwipeToolbarSnapshotProviding Private

/// Returns the coordinator coresponding to `toolbarType`.
- (AdaptiveToolbarCoordinator*)coordinatorWithToolbarType:
    (ToolbarType)toolbarType {
  switch (toolbarType) {
    case ToolbarType::kPrimary:
      return self.primaryToolbarCoordinator;
    case ToolbarType::kSecondary:
      return self.secondaryToolbarCoordinator;
  }
}

/// Prepares location bar for a side swipe snapshot with`webState`.
- (void)updateLocationBarForSideSwipeSnapshot:(web::WebState*)webState {
  // Hide LocationBarView when taking a snapshot on a web state that is not the
  // active one, as the URL is not updated.
  if (webState != self.browser->GetWebStateList()->GetActiveWebState()) {
    [self.locationBarCoordinator.locationBarViewController.view setHidden:YES];
  }
}

/// Resets location bar after a side swipe snapshot.
- (void)resetLocationBarAfterSideSwipeSnapshot {
  [self.locationBarCoordinator.locationBarViewController.view setHidden:NO];
}

#pragma mark - GuidedTourCommands

- (void)highlightViewInStep:(GuidedTourStep)step {
  for (id<GuidedTourCommands> coordinator in self.coordinators) {
    [coordinator highlightViewInStep:step];
  }
}

- (void)stepCompleted:(GuidedTourStep)step {
  for (id<GuidedTourCommands> coordinator in self.coordinators) {
    [coordinator stepCompleted:step];
  }
}

#pragma mark - LocationBarCoordinatorHeightDelegate

- (void)locationBarCoordinator:(LocationBarCoordinator*)coordinator
      didChangeEditStateHeight:(CGFloat)height {
  if (height == self.locationBarEditStateHeight) {
    return;
  }
  self.locationBarEditStateHeight = height;
  [self updateLocationBarHeightWithAnimation:NO focusStateDidChange:NO];
}

- (void)updateLocationBarHeightWithAnimation:(BOOL)animated
                         focusStateDidChange:(BOOL)focusStateDidChange {
  if (!IsMultilineBrowserOmniboxEnabled()) {
    // Location bar height is constant when multiline is not enabled. The height
    // is management in primary and secondary toolbar view controllers.
    return;
  }
  // Steady state height by default.
  CGFloat height =
      LocationBarHeight(self.primaryToolbarViewController.traitCollection
                            .preferredContentSizeCategory);

  // Apply the edit state height only when the location bar is focused and we
  // are not in a transition to focused state.
  if (self.locationBarFocused && !focusStateDidChange) {
    height = self.locationBarEditStateHeight;
  }

  [self.primaryToolbarCoordinator setLocationBarHeight:height];
  [self.secondaryToolbarCoordinator setLocationBarHeight:height];

  BOOL layoutChange = [self inEditState] || focusStateDidChange;
  if (layoutChange) {
    [self.toolbarHeightDelegate toolbarsHeightChanged];
    [self.toolbarHeightDelegate
        layoutToolbarHeightChangeWithAnimation:animated];
  }
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator triggerToolbarSlideInAnimation];
  }
}

- (void)indicateLensOverlayVisible:(BOOL)lensOverlayVisible {
  [self.locationBarCoordinator setLensOverlayVisible:lensOverlayVisible];

  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator indicateLensOverlayVisible:lensOverlayVisible];
  }
}

#pragma mark - ToolbarMediatorDelegate

- (void)transitionOmniboxToToolbarType:(ToolbarType)toolbarType {
  _omniboxPosition = toolbarType;

  [self updateOrchestratorAnimatee];

  OmniboxPositionBrowserAgent* positionBrowserAgent =
      OmniboxPositionBrowserAgent::FromBrowser(self.browser);
  switch (toolbarType) {
    case ToolbarType::kPrimary: {
      if (IsDiamondPrototypeEnabled()) {
        [self.secondaryToolbarCoordinator
            setLocationBarViewController:self.locationBarCoordinator
                                             .locationBarViewController];
        [self.primaryToolbarCoordinator setLocationBarViewController:nil];
      } else {
        [self.primaryToolbarCoordinator
            setLocationBarViewController:self.locationBarCoordinator
                                             .locationBarViewController];
        [self.secondaryToolbarCoordinator setLocationBarViewController:nil];
      }
      positionBrowserAgent->SetIsCurrentLayoutBottomOmnibox(false);
      break;
    }
    case ToolbarType::kSecondary:
      [self.secondaryToolbarCoordinator
          setLocationBarViewController:self.locationBarCoordinator
                                           .locationBarViewController];
      [self.primaryToolbarCoordinator setLocationBarViewController:nil];
      positionBrowserAgent->SetIsCurrentLayoutBottomOmnibox(true);
      break;
  }
  if (IsDiamondPrototypeEnabled()) {
    [self.toolbarHeightDelegate diamondToolbarTypeChanged:toolbarType];
    self.secondaryToolbarCoordinator.usedAsPrimaryToolbar =
        toolbarType == ToolbarType::kPrimary;
  }
  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

- (void)transitionSteadyStateOmniboxToToolbarType:(ToolbarType)toolbarType {
  _steadyStateOmniboxPosition = toolbarType;
}

- (CGFloat)keyboardAttachedBottomOmniboxHeight {
  CGFloat attachedHeight = self.locationBarCoordinator.locationBarViewController
                               .view.frame.size.height +
                           2 * kBottomAdaptiveLocationBarTopMargin;

  if (!self.locationBarFocused) {
    return 0;
  }

  if (IsMultilineBrowserOmniboxEnabled()) {
    return self.locationBarEditStateHeight +
           LocationBarVerticalMargins(
               self.locationBarCoordinator.locationBarViewController
                   .traitCollection.preferredContentSizeCategory);
  }

  BOOL forceEditState = omnibox::ForceBottomOmniboxInEditState();
  if (forceEditState) {
    return attachedHeight;
  }

  BOOL followSteadyState =
      omnibox::ShouldFocusedOmniboxFollowSteadyStatePosition();
  if (_omniboxPosition == ToolbarType::kSecondary && followSteadyState) {
    return attachedHeight;
  }

  return 0;
}

#pragma mark - Private

/// Returns primary and secondary coordinator in a array. Helper to call method
/// on both coordinators.
- (NSArray<id<ToolbarCoordinatee>>*)coordinators {
  return @[ self.primaryToolbarCoordinator, self.secondaryToolbarCoordinator ];
}

/// Returns the trait environment of the toolbars.
- (id<UITraitEnvironment>)traitEnvironment {
  return self.primaryToolbarViewController;
}

/// Updates toolbars layout whith current omnibox focus state and trait
/// collection.
- (void)updateToolbarsLayout {
  [self.toolbarMediator
      toolbarTraitCollectionChangedTo:self.traitEnvironment.traitCollection];
  BOOL omniboxFocused = [self inEditState];
  [self.orchestrator
      transitionToStateOmniboxFocused:omniboxFocused
                      toolbarExpanded:omniboxFocused &&
                                      !CanShowTabStrip(self.traitEnvironment)
                              trigger:[self omniboxFocusTrigger]
                             animated:NO
                           completion:nil];
}

/// Returns the appropriate `OmniboxFocusTrigger` depending on whether this is
/// an incognito browser, the NTP is displayed, and whether the fakebox was
/// pinned if it was selected.
- (OmniboxFocusTrigger)omniboxFocusTrigger {
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    if (!webState) {
      return OmniboxFocusTrigger::kOther;
    }
    if (!IsVisibleURLNewTabPage(webState)) {
      return OmniboxFocusTrigger::kOther;
    }

    // (De)focusing on NTP.

    if (self.isOffTheRecord || !IsSplitToolbarMode(self.traitEnvironment)) {
      return _focusedFromFakebox ? OmniboxFocusTrigger::kUnpinnedFakebox
                                 : OmniboxFocusTrigger::kNTPOmnibox;
    }

    return _fakeboxPinned ? OmniboxFocusTrigger::kPinnedFakebox
                          : OmniboxFocusTrigger::kUnpinnedFakebox;

  } else {
    if (self.isOffTheRecord || !IsSplitToolbarMode(self.traitEnvironment)) {
      return _focusedFromFakebox ? OmniboxFocusTrigger::kUnpinnedFakebox
                                 : OmniboxFocusTrigger::kOther;
    }
    web::WebState* webState =
        self.browser->GetWebStateList()->GetActiveWebState();
    if (!webState) {
      return OmniboxFocusTrigger::kOther;
    }
    if (!IsVisibleURLNewTabPage(webState)) {
      return OmniboxFocusTrigger::kOther;
    }
    return _fakeboxPinned ? OmniboxFocusTrigger::kPinnedFakebox
                          : OmniboxFocusTrigger::kUnpinnedFakebox;
  }
}

- (void)focusTransitionDidComplete:(BOOL)focused
                        completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
    completion = nil;
  }
}

- (void)updateOrchestratorAnimatee {
  id<ToolbarAnimatee> updatedToolbarAnimatee =
      _omniboxPosition == ToolbarType::kPrimary
          ? self.primaryToolbarCoordinator.toolbarAnimatee
          : self.secondaryToolbarCoordinator.toolbarAnimatee;
  BOOL willChangeToolbarAnimatee =
      updatedToolbarAnimatee != self.orchestrator.toolbarAnimatee;

  // If a change occurs, clear any previous animation effects to prevent the
  // toolbar from remaining expanded
  if (willChangeToolbarAnimatee) {
    [self.orchestrator
        transitionToStateOmniboxFocused:NO
                        toolbarExpanded:NO
                                trigger:OmniboxFocusTrigger::kOther
                               animated:NO
                             completion:nil];
  }

  self.orchestrator.toolbarAnimatee = updatedToolbarAnimatee;
  self.orchestrator.locationBarAnimatee =
      [self.locationBarCoordinator locationBarAnimatee];
  self.orchestrator.editViewAnimatee =
      [self.locationBarCoordinator editViewAnimatee];
}

- (void)setEntrypointViewHidden:(BOOL)hidden {
  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:_omniboxPosition];
  adaptiveToolbarCoordinator.viewController.locationBarContainer.hidden =
      hidden;
}

- (UIView*)entrypointViewVisualCopy {
  if (_omniboxPosition == ToolbarType::kSecondary || [self isNTP]) {
    return nil;
  }

  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:_omniboxPosition];
  UIView* locationBarContainer =
      adaptiveToolbarCoordinator.viewController.locationBarContainer;

  UIView* entrypointCopy = [[UIView alloc] init];
  entrypointCopy.frame =
      [locationBarContainer convertRect:locationBarContainer.bounds toView:nil];
  entrypointCopy.layer.cornerRadius = locationBarContainer.layer.cornerRadius;
  entrypointCopy.backgroundColor = locationBarContainer.backgroundColor;
  UIView* locationBarSteadyViewVisualCopy =
      self.locationBarCoordinator.locationBarSteadyViewVisualCopy;
  [entrypointCopy addSubview:locationBarSteadyViewVisualCopy];
  locationBarSteadyViewVisualCopy.translatesAutoresizingMaskIntoConstraints =
      NO;

  [NSLayoutConstraint activateConstraints:@[
    [locationBarSteadyViewVisualCopy.centerXAnchor
        constraintEqualToAnchor:entrypointCopy.centerXAnchor],
    [locationBarSteadyViewVisualCopy.centerYAnchor
        constraintEqualToAnchor:entrypointCopy.centerYAnchor],
    [locationBarSteadyViewVisualCopy.widthAnchor
        constraintEqualToAnchor:entrypointCopy.widthAnchor],
    [locationBarSteadyViewVisualCopy.heightAnchor
        constraintEqualToAnchor:entrypointCopy.heightAnchor],
  ]];

  return entrypointCopy;
}

- (BOOL)isNTP {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return NO;
  }
  return IsVisibleURLNewTabPage(webState);
}

@end

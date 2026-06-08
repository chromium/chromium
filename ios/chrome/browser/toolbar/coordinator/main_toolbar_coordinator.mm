// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"
#import "ios/chrome/browser/bubble/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_coordinator.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_drs_view_controller.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/omnibox_focus_orchestrator.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presentation_context.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_action_menu_entry_point_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_chip_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/toolbar/coordinator/main_toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/coordinator/toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/legacy_toolbar_mediator.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/primary_toolbar_coordinator.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/primary_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_omnibox_consumer.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_type.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/secondary_toolbar_coordinator.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/toolbar_coordinatee.h"
#import "ios/chrome/browser/toolbar/tab_group/coordinator/tab_group_indicator_coordinator.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_view_controller.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/web_state.h"

namespace {
// Extra vertical spacing when the banner promo is active on split mode.
constexpr CGFloat kBannerPromoVerticalSpacing = 8;
}  // namespace

@interface MainToolbarCoordinator () <ContextualPanelEntrypointCommands,
                                      GuidedTourCommands,
                                      LocationBarBadgeCommands,
                                      MainToolbarMediatorDelegate,
                                      PageActionMenuEntryPointCommands,
                                      PrimaryToolbarViewControllerDelegate,
                                      ReaderModeChipCommands,
                                      ToolbarCommands,
                                      ToolbarMediatorDelegate,
                                      FullscreenBrowserAgentObserving>

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
@property(nonatomic, strong) LegacyToolbarMediator* legacyToolbarMediator;
/// Orchestrator for the omnibox focus animation.
@property(nonatomic, strong) OmniboxFocusOrchestrator* orchestrator;
/// Whether the omnibox is currently focused.
@property(nonatomic, assign) BOOL locationBarFocused;
/// Dynamic response system view controller is an omnibox presenter. Only
/// defined  when kOmniboxDRSPrototype is set.
@property(nonatomic, strong) OmniboxDRSViewController* drsViewController;

@end

@implementation MainToolbarCoordinator {
  // The mediator for this coordinator.
  MainToolbarMediator* _mainToolbarMediator;
  /// Type of toolbar containing the omnibox. Unlike
  /// `_steadyStateOmniboxPosition`, this tracks the omnibox position at all
  /// time.
  ToolbarType _omniboxPosition;
  /// Type of the toolbar that contains the omnibox when it's not focused. The
  /// animation of focusing/defocusing the omnibox changes depending on this
  /// position.
  ToolbarType _steadyStateOmniboxPosition;
  //// Indicates whether the focus came from a tap on the NTP's fakebox.
  BOOL _focusedFromFakebox;
  /// Indicates whether the fakebox was pinned on last signal to focus from
  /// the fakebox.
  BOOL _fakeboxPinned;
  /// Command handler for showing the IPH.
  id<HelpCommands> _helpHandler;
  /// Top toolbar mediator.
  ToolbarMediator* _topToolbarMediator;
  /// Top toolbar view controller.
  ToolbarViewController* _topToolbarViewController;
  /// Fullscreen UI Updater for the top toolbar.
  std::unique_ptr<FullscreenUIUpdater> _topToolbarFullscreenUIUpdater;
  /// Top location bar coordinator.
  LocationBarCoordinator* _topLocationBarCoordinator;
  /// Coordinator for the tab group indicator.
  TabGroupIndicatorCoordinator* _tabGroupIndicatorCoordinator;
  /// Bottom toolbar mediator.
  ToolbarMediator* _bottomToolbarMediator;
  /// Bottom toolbar view controller.
  ToolbarViewController* _bottomToolbarViewController;
  /// Observer for fullscreen layout calculations.
  std::unique_ptr<FullscreenBrowserAgentObserverBridge> _fullscreenObserver;
  std::unique_ptr<FullscreenUIUpdater> _bottomToolbarFullscreenUIUpdater;
  /// Bottom location bar coordinator.
  LocationBarCoordinator* _bottomLocationBarCoordinator;
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
  // Set a default position, overridden by `setInitialOmniboxPosition` below.
  _omniboxPosition = ToolbarType::kPrimary;

  Browser* browser = self.browser;
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(FakeboxFocuser)];

  if (IsBestOfAppGuidedTourEnabled() && !IsChromeNextIaEnabled()) {
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

  self.legacyToolbarMediator = [[LegacyToolbarMediator alloc]
      initWithWebStateList:browser->GetWebStateList()
               isIncognito:browser->GetProfile()->IsOffTheRecord()];
  self.legacyToolbarMediator.delegate = self;

  _mainToolbarMediator = [[MainToolbarMediator alloc]
      initWithPrefService:GetApplicationContext()->GetLocalState()];
  _mainToolbarMediator.delegate = self;
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(ReaderModeChipCommands)];
  BOOL isOmniboxInBottomPosition =
      [_mainToolbarMediator isOmniboxInBottomPosition];

  OmniboxPositionBrowserAgent::FromBrowser(self.browser)
      ->SetIsCurrentLayoutBottomOmnibox(isOmniboxInBottomPosition);

  if (IsChromeNextIaEnabled()) {
    _topLocationBarCoordinator =
        [self createLocationBarCoordinatorActive:!isOmniboxInBottomPosition
                                     topPosition:YES];
    _topToolbarMediator = [self createToolbarMediatorTopPosition:YES];
    _topToolbarViewController = [self
        createToolbarViewControllerForMediator:_topToolbarMediator
                                   locationBar:_topLocationBarCoordinator
                                                   .locationBarViewController
                                   topPosition:YES];
    _tabGroupIndicatorCoordinator = [[TabGroupIndicatorCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:browser];
    _tabGroupIndicatorCoordinator.toolbarHeightDelegate =
        self.toolbarHeightDelegate;
    [_tabGroupIndicatorCoordinator start];
    [_topToolbarMediator
        setUICurrentlySupportsPromo:!_tabGroupIndicatorCoordinator.viewVisible];
    [_topToolbarViewController
        setTabGroupIndicatorView:_tabGroupIndicatorCoordinator.view];

    if (!IsFullscreenRefactoringEnabled()) {
      _topToolbarFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
          FullscreenController::FromBrowser(browser),
          _topToolbarViewController);
    } else {
      _fullscreenObserver =
          std::make_unique<FullscreenBrowserAgentObserverBridge>(
              self, FullscreenBrowserAgent::FromBrowser(browser));
    }

    _bottomLocationBarCoordinator =
        [self createLocationBarCoordinatorActive:isOmniboxInBottomPosition
                                     topPosition:NO];
    _bottomToolbarMediator = [self createToolbarMediatorTopPosition:NO];
    _bottomToolbarViewController = [self
        createToolbarViewControllerForMediator:_bottomToolbarMediator
                                   locationBar:_bottomLocationBarCoordinator
                                                   .locationBarViewController
                                   topPosition:NO];
    if (!IsFullscreenRefactoringEnabled()) {
      _bottomToolbarFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
          FullscreenController::FromBrowser(browser),
          _bottomToolbarViewController);
    }

    LayoutGuideCenter* layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
    [layoutGuideCenter referenceView:_topToolbarViewController.view
                           underName:kPrimaryToolbarGuide];
    [layoutGuideCenter
        referenceView:_topLocationBarCoordinator.locationBarViewController.view
            underName:kTopOmniboxGuide];
    [layoutGuideCenter referenceView:_bottomToolbarViewController.view
                           underName:kSecondaryToolbarGuide];

    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ContextualPanelEntrypointCommands)];
    [self.browser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(LocationBarBadgeCommands)];
    if (IsPageActionMenuEnabled()) {
      [browser->GetCommandDispatcher()
          startDispatchingToTarget:self
                       forProtocol:@protocol(PageActionMenuEntryPointCommands)];
    }
    self.started = YES;
    return;
  }

  self.locationBarCoordinator =
      [[LocationBarCoordinator alloc] initWithBrowser:browser];
  self.locationBarCoordinator.delegate = self.omniboxFocusDelegate;
  self.locationBarCoordinator.popupPresenterDelegate =
      self.popupPresenterDelegate;
  [self.locationBarCoordinator start];

  self.primaryToolbarCoordinator.viewControllerDelegate = self;
  self.primaryToolbarCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [self.primaryToolbarCoordinator start];
  self.secondaryToolbarCoordinator.toolbarHeightDelegate =
      self.toolbarHeightDelegate;
  [self.secondaryToolbarCoordinator start];

  if (!IsChromeNextIaEnabled()) {
    self.orchestrator = [[OmniboxFocusOrchestrator alloc] init];
    [self updateOrchestratorAnimatee];
  }

  if (IsBottomOmniboxAvailable()) {
    [self.legacyToolbarMediator setInitialOmniboxPosition];
  } else {
    [self.primaryToolbarCoordinator
        setLocationBarViewController:self.locationBarCoordinator
                                         .locationBarViewController];
  }

  if (IsPageActionMenuEnabled()) {
    [self.locationBarCoordinator setPageActionMenuEntryPointDispatcher];
  }

  [self updateToolbarsLayout];

  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }

  if (IsChromeNextIaEnabled()) {
    [_topToolbarMediator disconnect];
    _topToolbarMediator = nil;
    [_topLocationBarCoordinator stop];
    _topLocationBarCoordinator = nil;
    _topToolbarViewController = nil;
    _fullscreenObserver = nullptr;
    _topToolbarFullscreenUIUpdater = nullptr;

    [_tabGroupIndicatorCoordinator stop];

    [_bottomToolbarMediator disconnect];
    _bottomToolbarMediator = nil;
    [_bottomLocationBarCoordinator stop];
    _bottomLocationBarCoordinator = nil;
    _bottomToolbarViewController = nil;
    _bottomToolbarFullscreenUIUpdater = nullptr;
  }

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

  [self.legacyToolbarMediator disconnect];
  self.legacyToolbarMediator.omniboxConsumer = nil;
  self.legacyToolbarMediator.delegate = nil;
  self.legacyToolbarMediator = nil;

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
  if (IsChromeNextIaEnabled()) {
    return _topToolbarViewController;
  }
  return self.primaryToolbarCoordinator.viewController;
}

- (UIViewController*)secondaryToolbarViewController {
  if (IsChromeNextIaEnabled()) {
    return _bottomToolbarViewController;
  }
  return self.secondaryToolbarCoordinator.viewController;
}

- (UIView*)shareButton {
  return self.primaryToolbarCoordinator.shareButton;
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

  if (!IsChromeNextIaEnabled()) {
    // Hide the toolbar when displaying content suggestions without the tab
    // strip, without the focused omnibox, only when in split toolbar mode.
    BOOL hideToolbar = isNTP && !isOffTheRecord && ![self inEditState] &&
                       !canShowTabStrip &&
                       IsSplitToolbarMode(self.traitEnvironment);

    self.primaryToolbarViewController.view.hidden = hideToolbar;
  }
}

- (BOOL)isLoadingPrerenderer {
  if (!_started) {
    return NO;
  }

  PrerenderBrowserAgent* prerenderBrowserAgent =
      PrerenderBrowserAgent::FromBrowser(self.browser);
  return prerenderBrowserAgent && prerenderBrowserAgent->IsInsertingPrerender();
}

- (void)setToolbarHeightDelegate:(id<ToolbarHeightDelegate>)delegate {
  _toolbarHeightDelegate = delegate;
  _topToolbarViewController.toolbarHeightDelegate = delegate;
  _bottomToolbarViewController.toolbarHeightDelegate = delegate;
}

#pragma mark Omnibox and LocationBar

- (void)transitionToLocationBarFocusedState:(BOOL)focused
                                 completion:(ProceduralBlock)completion {
  CHECK(!IsChromeNextIaEnabled());
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
  [self.legacyToolbarMediator locationBarFocusChangedTo:focused];

  BOOL animateTransition =
      (_steadyStateOmniboxPosition == ToolbarType::kPrimary);

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
                             completion:completion];
  }

  [self.primaryToolbarCoordinator.viewController setLocationBarFocused:focused];
  [self.secondaryToolbarCoordinator.viewController
      setLocationBarFocused:focused];
  self.locationBarFocused = focused;
}

- (BOOL)isOmniboxFirstResponder {
  CHECK(!IsChromeNextIaEnabled());
  return [self.locationBarCoordinator isOmniboxFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  CHECK(!IsChromeNextIaEnabled());
  return [self.locationBarCoordinator showingOmniboxPopup];
}

- (void)setBottomOmniboxOffsetForPopup:(CGFloat)bottomOffset {
  [self.legacyToolbarMediator setBottomOmniboxOffsetForPopup:bottomOffset];
}

#pragma mark ToolbarHeightProviding

- (CGFloat)collapsedPrimaryToolbarHeight {
  if (IsChromeNextIaEnabled()) {
    if (self.primaryToolbarViewController.view.hidden) {
      // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
      // zero. This is a temporary fix for the pdf bug.
      return IsFullscreenRefactoringEnabled() ? 0.0 : 1.0;
    }
    if ([self isOmniboxInBottomPosition]) {
      // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
      // zero. This is a temporary fix for the pdf bug.
      return IsFullscreenRefactoringEnabled() ? 0 : 1;
    }
    if (CanShowTabStrip(self.traitEnvironment)) {
      return kTopToolbarIPadHeightFullscreen;
    }
    if (!IsSplitToolbarMode(self.traitEnvironment)) {
      return kToolbarHeightFullscreen;
    }
    return kTopToolbarIPhonePortraitHeightFullscreen;
  }
  if (_omniboxPosition == ToolbarType::kSecondary) {
    // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
    // zero. This is a temporary fix for the pdf bug.
    return IsFullscreenRefactoringEnabled() ? 0.0 : 1.0;
  }

  return ToolbarCollapsedHeight(
      self.traitEnvironment.traitCollection.preferredContentSizeCategory);
}

- (CGFloat)expandedPrimaryToolbarHeight {
  if (IsChromeNextIaEnabled()) {
    if (self.primaryToolbarViewController.view.hidden) {
      // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
      // zero. This is a temporary fix for the pdf bug.
      return IsFullscreenRefactoringEnabled() ? 0.0 : 1.0;
    }
    BOOL isOmniboxInBottomPosition = [self isOmniboxInBottomPosition];
    CGFloat height = 0;
    if (_tabGroupIndicatorCoordinator.viewVisible) {
      height += kTabGroupIndicatorHeight;
      if (isOmniboxInBottomPosition) {
        height -= kTopToolbarUnsplitMargin;
      }
    }
    if (_topToolbarViewController.bannerPromoVisible) {
      height += kToolbarPromoBannerHeight;
      if (IsSplitToolbarMode(_topToolbarViewController) &&
          !isOmniboxInBottomPosition) {
        height += kBannerPromoVerticalSpacing;
      }
    }
    if (isOmniboxInBottomPosition) {
      // TODO(crbug.com/40279063): Find out why primary toolbar height cannot be
      // zero. This is a temporary fix for the pdf bug.
      if (IsFullscreenRefactoringEnabled()) {
        return height;
      } else {
        return height > 0 ? height : 1;
      }
    }
    if (ShouldHaveFullHeightTopToolbar(self.traitEnvironment)) {
      return height + kToolbarHeight;
    }
    return height + kTopToolbarIPhonePortraitHeight;
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
  if (IsChromeNextIaEnabled()) {
    if (self.secondaryToolbarViewController.view.hidden) {
      return 0.0;
    }
    if ([self isOmniboxInBottomPosition]) {
      return kToolbarHeightFullscreen;
    }
    return 0.0;
  }
  if (_omniboxPosition == ToolbarType::kSecondary) {
    return ToolbarCollapsedHeight(
        self.traitEnvironment.traitCollection.preferredContentSizeCategory);
  }
  return 0.0;
}

- (CGFloat)expandedSecondaryToolbarHeight {
  if (IsChromeNextIaEnabled()) {
    if (self.secondaryToolbarViewController.view.hidden) {
      return 0.0;
    }
    if ([self isOmniboxInBottomPosition]) {
      return kToolbarHeight;
    }
    return 0.0;
  }
  if (!IsSplitToolbarMode(self.traitEnvironment)) {
    return 0.0;
  }
  CGFloat height =
      self.secondaryToolbarViewController.view.intrinsicContentSize.height;
  if (_omniboxPosition == ToolbarType::kSecondary) {
    height += ToolbarExpandedHeight(
        self.traitEnvironment.traitCollection.preferredContentSizeCategory);
  }
  return height;
}

#pragma mark - FakeboxFocuser

- (void)focusOmniboxFromFakebox:(BOOL)fromFakebox
                            pinned:(BOOL)pinned
    fakeboxButtonsSnapshotProvider:
        (id<FakeboxButtonsSnapshotProvider>)provider {
  CHECK(!IsChromeNextIaEnabled());
  _focusedFromFakebox = fromFakebox;
  _fakeboxPinned = pinned;
  [self.locationBarCoordinator setFakeboxButtonsSnapshotProvider:provider];
  [self.locationBarCoordinator focusOmniboxFromFakebox];
}

- (void)onFakeboxBlur {
  CHECK(!IsChromeNextIaEnabled());
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
  CHECK(!IsChromeNextIaEnabled());
  self.primaryToolbarViewController.view.hidden = NO;
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  if (IsChromeNextIaEnabled()) {
    [_topToolbarViewController setNTPScrollProgress:progress];
    [_bottomToolbarViewController setNTPScrollProgress:progress];
    return;
  }

  for (id<NewTabPageControllerDelegate> coordinator in self.coordinators) {
    [coordinator setScrollProgressForTabletOmnibox:progress];
  }
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  return self.locationBarCoordinator.omniboxScribbleForwardingTarget;
}

- (void)didNavigateToNTPOnActiveWebState {
  [self.legacyToolbarMediator didNavigateToNTPOnActiveWebState];
}

#pragma mark - OmniboxStateProvider

- (BOOL)isOmniboxFocused {
  CHECK(!IsChromeNextIaEnabled());
  return [self.locationBarCoordinator isOmniboxFocused];
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  if (IsChromeNextIaEnabled()) {
    [_topToolbarViewController updateUIForOverflowMenuIPHDisplayed];
    [_bottomToolbarViewController updateUIForOverflowMenuIPHDisplayed];
  }
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForOverflowMenuIPHDisplayed];
  }
}

- (void)updateUIForIPHDismissed {
  if (IsChromeNextIaEnabled()) {
    [_topToolbarViewController updateUIForIPHDismissed];
    [_bottomToolbarViewController updateUIForIPHDismissed];
  }
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater updateUIForIPHDismissed];
  }
}

- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot {
  if (IsChromeNextIaEnabled()) {
    [_topToolbarViewController setOverflowMenuBlueDot:hasBlueDot];
    [_bottomToolbarViewController setOverflowMenuBlueDot:hasBlueDot];
  }
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    [coordinator.popupMenuUIUpdater setOverflowMenuBlueDot:hasBlueDot];
  }
}

#pragma mark - PrimaryToolbarViewControllerDelegate

- (void)viewControllerTraitCollectionDidChange:
    (UITraitCollection*)previousTraitCollection {
  CHECK(!IsChromeNextIaEnabled());
  if (!_started) {
    return;
  }
  [self updateToolbarsLayout];
}

- (void)close {
  CHECK(!IsChromeNextIaEnabled());
  if (self.locationBarFocused) {
    id<SceneCommands> sceneHandler =
        HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
    [sceneHandler dismissModalDialogsWithCompletion:nil];
  }
}

- (void)locationBarExpandedInViewController:
    (PrimaryToolbarViewController*)viewController {
  CHECK(!IsChromeNextIaEnabled());
  // Do nothing.
}
- (void)locationBarContractedInViewController:
    (PrimaryToolbarViewController*)viewController {
  CHECK(!IsChromeNextIaEnabled());
  // Do nothing.
}

- (void)viewController:(PrimaryToolbarViewController*)viewController
    tabGroupIndicatorVisibilityUpdated:(BOOL)visible {
  CHECK(!IsChromeNextIaEnabled());
  // Do nothing.
}

#pragma mark - SideSwipeToolbarInteracting

- (BOOL)isInsideToolbar:(CGPoint)point {
  if (IsChromeNextIaEnabled()) {
    return
        [self isPoint:point insideViewController:_topToolbarViewController] ||
        [self isPoint:point insideViewController:_bottomToolbarViewController];
  }
  for (id<ToolbarCoordinatee> coordinator in self.coordinators) {
    if ([self isPoint:point
            insideViewController:[coordinator viewController]]) {
      return YES;
    }
  }
  return NO;
}

#pragma mark - SideSwipeToolbarSnapshotProviding

- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState
                                withToolbarType:(ToolbarType)toolbarType {
  if (IsChromeNextIaEnabled()) {
    ToolbarViewController* toolbar;
    ToolbarMediator* mediator;
    switch (toolbarType) {
      case ToolbarType::kPrimary:
        toolbar = _topToolbarViewController;
        mediator = _topToolbarMediator;
        break;
      case ToolbarType::kSecondary:
        toolbar = _bottomToolbarViewController;
        mediator = _bottomToolbarMediator;
        break;
    }

    [mediator updateConsumerWithWebState:webState animated:NO];

    UIView* toolbarView = toolbar.view;
    // The toolbar must be in the view hierarchy to be snapshotted.
    if (!toolbarView.window) {
      return nil;
    }
    UIImage* toolbarSnapshot = CaptureViewWithOption(
        toolbarView, toolbarView.window.screen.scale, kClientSideRendering);

    [mediator updateConsumerWithWebState:self.browser->GetWebStateList()
                                             ->GetActiveWebState()
                                animated:NO];

    return toolbarSnapshot;
  }

  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:toolbarType];

  [adaptiveToolbarCoordinator updateToolbarForSideSwipeSnapshot:webState];

  UIView* toolbarView = adaptiveToolbarCoordinator.viewController.view;
  // The toolbar must be in the view hierarchy to be snapshotted.
  if (!toolbarView.window) {
    return nil;
  }
  UIImage* toolbarSnapshot = CaptureViewWithOption(
      toolbarView, [[UIScreen mainScreen] scale], kClientSideRendering);

  [adaptiveToolbarCoordinator resetToolbarAfterSideSwipeSnapshot];

  return toolbarSnapshot;
}

#pragma mark SideSwipeToolbarSnapshotProviding Private

/// Returns the coordinator coresponding to `toolbarType`.
- (AdaptiveToolbarCoordinator*)coordinatorWithToolbarType:
    (ToolbarType)toolbarType {
  CHECK(!IsChromeNextIaEnabled());
  switch (toolbarType) {
    case ToolbarType::kPrimary:
      return self.primaryToolbarCoordinator;
    case ToolbarType::kSecondary:
      return self.secondaryToolbarCoordinator;
  }
}

/// Prepares location bar for a side swipe snapshot with`webState`.
- (void)updateLocationBarForSideSwipeSnapshot:(web::WebState*)webState {
  CHECK(!IsChromeNextIaEnabled());
  // Hide LocationBarView when taking a snapshot on a web state that is not the
  // active one, as the URL is not updated.
  if (webState != self.browser->GetWebStateList()->GetActiveWebState()) {
    [self.locationBarCoordinator.locationBarViewController.view setHidden:YES];
  }
}

/// Resets location bar after a side swipe snapshot.
- (void)resetLocationBarAfterSideSwipeSnapshot {
  CHECK(!IsChromeNextIaEnabled());
  [self.locationBarCoordinator.locationBarViewController.view setHidden:NO];
}

#pragma mark - GuidedTourCommands

- (void)highlightViewInStep:(GuidedTourStep)step {
  CHECK(!IsChromeNextIaEnabled());
  for (id<GuidedTourCommands> coordinator in self.coordinators) {
    [coordinator highlightViewInStep:step];
  }
}

- (void)stepCompleted:(GuidedTourStep)step {
  CHECK(!IsChromeNextIaEnabled());
  for (id<GuidedTourCommands> coordinator in self.coordinators) {
    [coordinator stepCompleted:step];
  }
}

#pragma mark - ComposeboxAnimationBase

- (void)setEntrypointViewHidden:(BOOL)hidden {
  if (IsChromeNextIaEnabled()) {
    [_topToolbarViewController setLocationBarHidden:hidden];
    [_bottomToolbarViewController setLocationBarHidden:hidden];
    return;
  }
  AdaptiveToolbarCoordinator* adaptiveToolbarCoordinator =
      [self coordinatorWithToolbarType:_omniboxPosition];
  adaptiveToolbarCoordinator.viewController.locationBarContainer.hidden =
      hidden;
}

- (UIView*)entrypointViewVisualCopy {
  if (IsChromeNextIaEnabled()) {
    if ([self isOmniboxInBottomPosition] || [self isNTP]) {
      return nil;
    }

    UIView* entrypointCopy =
        [_topToolbarViewController locationBarContainerCopy];
    UIView* locationBarSteadyViewVisualCopy =
        _topLocationBarCoordinator.locationBarSteadyViewVisualCopy;
    [entrypointCopy addSubview:locationBarSteadyViewVisualCopy];
    locationBarSteadyViewVisualCopy.translatesAutoresizingMaskIntoConstraints =
        NO;

    AddSameConstraints(entrypointCopy, locationBarSteadyViewVisualCopy);
    return entrypointCopy;
  }

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

#pragma mark - LocationBarBadgeCommands

- (void)updateBadgeConfig:(LocationBarBadgeConfiguration*)config {
  CHECK(IsChromeNextIaEnabled());
  [_topLocationBarCoordinator updateBadgeConfig:config];
  [_bottomLocationBarCoordinator updateBadgeConfig:config];
}

- (void)updateColorForIPH {
  CHECK(IsChromeNextIaEnabled());
  [_topLocationBarCoordinator updateColorForIPH];
  [_bottomLocationBarCoordinator updateColorForIPH];
}

- (void)markDisplayedBadgeAsUnread:(BOOL)read {
  CHECK(IsChromeNextIaEnabled());
  [_topLocationBarCoordinator markDisplayedBadgeAsUnread:read];
  [_bottomLocationBarCoordinator markDisplayedBadgeAsUnread:read];
}

#pragma mark - ReaderModeChipCommands

- (void)showReaderModeChip {
  if (IsChromeNextIaEnabled()) {
    [_topLocationBarCoordinator.readerModeChipHandler showReaderModeChip];
    [_bottomLocationBarCoordinator.readerModeChipHandler showReaderModeChip];
  } else {
    [self.locationBarCoordinator.readerModeChipHandler showReaderModeChip];
  }
}

- (void)hideReaderModeChip {
  if (IsChromeNextIaEnabled()) {
    [_topLocationBarCoordinator.readerModeChipHandler hideReaderModeChip];
    [_bottomLocationBarCoordinator.readerModeChipHandler hideReaderModeChip];
  } else {
    [self.locationBarCoordinator.readerModeChipHandler hideReaderModeChip];
  }
}

#pragma mark - ContextualPanelEntrypointCommands

- (void)notifyContextualPanelEntrypointIPHDismissed {
  CHECK(IsChromeNextIaEnabled());
  [_topLocationBarCoordinator notifyContextualPanelEntrypointIPHDismissed];
  [_bottomLocationBarCoordinator notifyContextualPanelEntrypointIPHDismissed];
}

- (void)cancelContextualPanelEntrypointLoudMoment {
  CHECK(IsChromeNextIaEnabled());
  [_topLocationBarCoordinator cancelContextualPanelEntrypointLoudMoment];
  [_bottomLocationBarCoordinator cancelContextualPanelEntrypointLoudMoment];
}

#pragma mark - PageActionMenuEntryPointCommands

- (void)toggleEntryPointHighlight:(BOOL)highlight {
  CHECK(IsChromeNextIaEnabled());
  CHECK(IsPageActionMenuEnabled());
  [_topLocationBarCoordinator
      togglePageActionMenuEntryPointHighlight:highlight];
  [_bottomLocationBarCoordinator
      togglePageActionMenuEntryPointHighlight:highlight];
}

#pragma mark - ToolbarCommands

- (void)indicateLensOverlayVisible:(BOOL)lensOverlayVisible {
  if (IsChromeNextIaEnabled()) {
    [_topLocationBarCoordinator setLensOverlayVisible:lensOverlayVisible];
    [_bottomLocationBarCoordinator setLensOverlayVisible:lensOverlayVisible];
    return;
  } else {
    [self.locationBarCoordinator setLensOverlayVisible:lensOverlayVisible];
  }

  for (id<ToolbarCommands> coordinator in self.coordinators) {
    [coordinator indicateLensOverlayVisible:lensOverlayVisible];
  }
}

- (void)focusLocationBarForVoiceOver {
  if (IsChromeNextIaEnabled()) {
    if (_topToolbarViewController.visible) {
      [_topLocationBarCoordinator focusOmniboxForVoiceOver];
    } else {
      [_bottomLocationBarCoordinator focusOmniboxForVoiceOver];
    }
  } else {
    id<OmniboxCommands> omniboxHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), OmniboxCommands);
    [omniboxHandler focusOmniboxForVoiceOver];
  }
}

#pragma mark - MainToolbarMediatorDelegate

- (void)mainToolbarMediatorDidChangeOmniboxPosition:
    (MainToolbarMediator*)mediator {
  if (!IsChromeNextIaEnabled()) {
    return;
  }

  if (mediator.isOmniboxInBottomPosition) {
    [_topLocationBarCoordinator setLocationBarActive:NO];
    [_bottomLocationBarCoordinator setLocationBarActive:YES];
    OmniboxPositionBrowserAgent::FromBrowser(self.browser)
        ->SetIsCurrentLayoutBottomOmnibox(YES);
  } else {
    [_topLocationBarCoordinator setLocationBarActive:YES];
    [_bottomLocationBarCoordinator setLocationBarActive:NO];
    OmniboxPositionBrowserAgent::FromBrowser(self.browser)
        ->SetIsCurrentLayoutBottomOmnibox(NO);
  }
}

#pragma mark - ToolbarMediatorDelegate

- (void)transitionOmniboxToToolbarType:(ToolbarType)toolbarType {
  _omniboxPosition = toolbarType;

  if (!IsChromeNextIaEnabled()) {
    [self updateOrchestratorAnimatee];
  }

  OmniboxPositionBrowserAgent* positionBrowserAgent =
      OmniboxPositionBrowserAgent::FromBrowser(self.browser);
  switch (toolbarType) {
    case ToolbarType::kPrimary: {
      [self.primaryToolbarCoordinator
          setLocationBarViewController:self.locationBarCoordinator
                                           .locationBarViewController];
      [self.secondaryToolbarCoordinator setLocationBarViewController:nil];
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
  [self.toolbarHeightDelegate toolbarsHeightChanged];
}

- (void)transitionSteadyStateOmniboxToToolbarType:(ToolbarType)toolbarType {
  _steadyStateOmniboxPosition = toolbarType;
}

- (CGFloat)keyboardAttachedBottomOmniboxHeight {
  if (IsChromeNextIaEnabled()) {
    if (self.browser->GetSceneState().layoutState.appBarPosition ==
        AppBarPosition::kBottom) {
      return kKeyboardAttachedOmniboxBottomPadding;
    } else {
      return kKeyboardAttachedOmniboxBottomPaddingLandscape;
    }
  }
  return 0;
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent {
  agent->AddObscuredInsetRange(UIRectEdgeTop,
                               [self collapsedPrimaryToolbarHeight],
                               [self expandedPrimaryToolbarHeight]);
  agent->AddObscuredInsetRange(UIRectEdgeBottom,
                               [self collapsedSecondaryToolbarHeight],
                               [self expandedSecondaryToolbarHeight]);
}

- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent {
  CGFloat topMin = [self collapsedPrimaryToolbarHeight];
  CGFloat topMax = [self expandedPrimaryToolbarHeight];
  CGFloat topInset = topMin + (topMax - topMin) * agent->top_progress();
  agent->AddObscuredInset(UIRectEdgeTop, topInset);
  [_topToolbarViewController updateForFullscreenProgress:agent->top_progress()];

  CGFloat bottomMin = [self collapsedSecondaryToolbarHeight];
  CGFloat bottomMax = [self expandedSecondaryToolbarHeight];
  CGFloat bottomInset =
      bottomMin + (bottomMax - bottomMin) * agent->bottom_progress();
  agent->AddObscuredInset(UIRectEdgeBottom, bottomInset);
  [_bottomToolbarViewController
      updateForFullscreenProgress:agent->bottom_progress()];
}

#pragma mark - Private

/// Whether the omnibox is currently in edit state.
- (BOOL)inEditState {
  CHECK(!IsChromeNextIaEnabled());
  return [self isOmniboxFirstResponder] || [self showingOmniboxPopup];
}

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
  CHECK(!IsChromeNextIaEnabled());
  [self.legacyToolbarMediator
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
}

- (void)updateOrchestratorAnimatee {
  CHECK(!IsChromeNextIaEnabled());
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

- (BOOL)isNTP {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    return NO;
  }
  return IsVisibleURLNewTabPage(webState);
}

// Creates a new toolbar view controller, for the associated `mediator`.
- (ToolbarViewController*)
    createToolbarViewControllerForMediator:(ToolbarMediator*)mediator
                               locationBar:(UIViewController*)locationBar
                               topPosition:(BOOL)topPosition {
  CHECK(IsChromeNextIaEnabled());

  BOOL incognito = self.profile->IsOffTheRecord();

  Browser* browser = self.browser;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();

  ToolbarViewController* toolbarViewController =
      [[ToolbarViewController alloc] initInIncognito:incognito
                                         topPosition:topPosition];
  toolbarViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(browser);
  toolbarViewController.layoutState = browser->GetSceneState().layoutState;
  toolbarViewController.buttonFactory =
      [[ToolbarButtonFactory alloc] initWithIncognito:incognito];
  toolbarViewController.mutator = mediator;
  toolbarViewController.browserCoordinatorHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  toolbarViewController.popupMenuHandler =
      HandlerForProtocol(dispatcher, PopupMenuCommands);
  toolbarViewController.activityServiceHandler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);
  toolbarViewController.sceneHandler =
      HandlerForProtocol(dispatcher, SceneCommands);
  toolbarViewController.toolbarHeightDelegate = self.toolbarHeightDelegate;
  toolbarViewController.locationBarViewController = locationBar;
  toolbarViewController.bannerPromoDelegate = mediator;

  if (incognito) {
    toolbarViewController.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  }

  mediator.consumer = toolbarViewController;

  return toolbarViewController;
}

// Creates a new location bar coordinator.
- (LocationBarCoordinator*)createLocationBarCoordinatorActive:(BOOL)active
                                                  topPosition:
                                                      (BOOL)topPosition {
  LocationBarCoordinator* coordinator =
      [[LocationBarCoordinator alloc] initWithBrowser:self.browser];
  [coordinator start];
  [coordinator setTopPosition:topPosition];
  [coordinator setLocationBarActive:active];

  return coordinator;
}

// Creates a new toolbar mediator.
- (ToolbarMediator*)createToolbarMediatorTopPosition:(BOOL)topPosition {
  CHECK(IsChromeNextIaEnabled());

  Browser* browser = self.browser;
  BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:browser
             scenario:kMenuScenarioHistogramToolbarMenu];

  BOOL isIncognito = self.profile->IsOffTheRecord();
  DefaultBrowserBannerPromoAppAgent* agent = nil;
  if (topPosition && !isIncognito) {
    agent = [DefaultBrowserBannerPromoAppAgent
        agentFromApp:browser->GetSceneState().profileState.appState];
  }

  ProfileIOS* profile = self.profile;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  GeminiService* geminiService = GeminiServiceFactory::GetForProfile(profile);
  GeminiBrowserAgent* geminiBrowserAgent =
      GeminiBrowserAgent::FromBrowser(browser);

  ToolbarMediator* toolbarMediator = [[ToolbarMediator alloc]
                 initWithIncognito:isIncognito
                      webStateList:browser->GetWebStateList()
                     actionFactory:actionFactory
                       prefService:profile->GetPrefs()
              fullscreenController:FullscreenController::FromBrowser(browser)
                       topPosition:topPosition
      defaultBrowserBannerAppAgent:agent
             authenticationService:authService
                     geminiService:geminiService
                geminiBrowserAgent:geminiBrowserAgent];
  toolbarMediator.navigationBrowserAgent =
      WebNavigationBrowserAgent::FromBrowser(browser);
  toolbarMediator.tabBasedIPHAgent =
      TabBasedIPHBrowserAgent::FromBrowser(browser);
  if (IsFullscreenRefactoringEnabled()) {
    toolbarMediator.fullscreenCommands =
        HandlerForProtocol(browser->GetCommandDispatcher(), FullscreenCommands);
  }
  toolbarMediator.settingsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SettingsCommands);
  toolbarMediator.geminiHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), BWGCommands);
  toolbarMediator.baseViewController = self.baseViewController;
  toolbarMediator.sceneHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SceneCommands);

  return toolbarMediator;
}

// Returns true if the omnibox is in the bottom position.
- (BOOL)isOmniboxInBottomPosition {
  CHECK(IsChromeNextIaEnabled());
  return IsBottomOmniboxAvailable() &&
         [_mainToolbarMediator isOmniboxInBottomPosition];
}

// Returns whether `point` in window coordinates is inside the frame of
// `viewController`'s view.
- (BOOL)isPoint:(CGPoint)point
    insideViewController:(UIViewController*)viewController {
  // The toolbar bounds are inset by 1 because CGRectContainsPoint does
  // include points on the max X and Y edges, which will happen frequently
  // with edge swipes from the right side.
  CGRect toolbarBounds = CGRectInset(viewController.view.bounds, -1, -1);
  CGPoint pointInToolbarCoordinates = [viewController.view convertPoint:point
                                                               fromView:nil];
  return CGRectContainsPoint(toolbarBounds, pointInToolbarCoordinates);
}

@end

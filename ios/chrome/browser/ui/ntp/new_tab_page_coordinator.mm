// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/search_engines/default_search_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageCoordinator () <BooleanObserver,
                                     NewTabPageCommands,
                                     NewTabPageContentDelegate,
                                     OverscrollActionsControllerDelegate,
                                     PrefObserverDelegate,
                                     SceneStateObserver> {
  // Helper object managing the availability of the voice search feature.
  VoiceSearchAvailability _voiceSearchAvailability;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* ntpViewController;

// Mediator owned by this Coordinator.
@property(nonatomic, strong) NTPHomeMediator* ntpMediator;

// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;

// View controller wrapping the Discover feed.
@property(nonatomic, strong)
    DiscoverFeedWrapperViewController* discoverFeedWrapperViewController;

// View controller for the incognito NTP.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

// The timetick of the last time the NTP was displayed.
@property(nonatomic, assign) base::TimeTicks didAppearTime;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
@property(nonatomic, assign) BOOL visible;

// Whether the view is new tab view is currently presented (possibly in
// background). Used to report NTP usage metrics.
@property(nonatomic, assign) BOOL viewPresented;

// Wheter the scene is currently in foreground.
@property(nonatomic, assign) BOOL sceneInForeground;

// Handles interactions with the content suggestions header and the fake
// omnibox.
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerSynchronizer;

// The ViewController displayed by this Coordinator. This is the returned
// ViewController and will contain the |containedViewController| (Which can
// change depending on Feed visibility).
@property(nonatomic, strong) UIViewController* containerViewController;

// The coordinator contained ViewController. It can be either a
// NewTabPageViewController (When the Discover Feed is being shown) or a
// ContentSuggestionsViewController (When the Discover Feed is hidden or when
// the non refactored NTP is being used.)
// TODO(crbug.com/1114792): Update this comment when the NTP refactors launches.
@property(nonatomic, strong) UIViewController* containedViewController;

// Whether the feed should be expanded or collapsed. Collapsed
// means to show the feed header, but not any of the feed content.
@property(nonatomic, strong) PrefBackedBoolean* discoverFeedExpanded;

@end

@implementation NewTabPageCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    self.containerViewController = [[UIViewController alloc] init];

    PrefService* prefService =
        ChromeBrowserState::FromBrowserState(browser->GetBrowserState())
            ->GetPrefs();
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(prefService);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kArticlesForYouEnabled, _prefChangeRegistrar.get());
    _prefObserverBridge->ObserveChangesForPreference(
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        _prefChangeRegistrar.get());
    if (IsRefactoredNTP()) {
      _discoverFeedExpanded = [[PrefBackedBoolean alloc]
          initWithPrefService:prefService
                     prefName:feed::prefs::kArticlesListVisible];
      [_discoverFeedExpanded setObserver:self];
    }
  }
  return self;
}

- (void)start {
  if (self.started)
    return;

  DCHECK(self.browser);
  DCHECK(self.webState);
  DCHECK(self.toolbarDelegate);

  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    self.started = YES;
    return;
  }

  DCHECK(!self.contentSuggestionsCoordinator);

  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  self.ntpMediator = [[NTPHomeMediator alloc]
             initWithWebState:self.webState
           templateURLService:templateURLService
                    URLLoader:UrlLoadingBrowserAgent::FromBrowser(self.browser)
                  authService:self.authService
              identityManager:IdentityManagerFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
                   logoVendor:ios::GetChromeBrowserProvider()->CreateLogoVendor(
                                  self.browser, self.webState)
      voiceSearchAvailability:&_voiceSearchAvailability];
  self.ntpMediator.browser = self.browser;

  self.contentSuggestionsCoordinator = [[ContentSuggestionsCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
  self.contentSuggestionsCoordinator.webState = self.webState;
  self.contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
  self.contentSuggestionsCoordinator.panGestureHandler = self.panGestureHandler;
  self.contentSuggestionsCoordinator.ntpMediator = self.ntpMediator;
  self.contentSuggestionsCoordinator.ntpCommandHandler = self;
  self.contentSuggestionsCoordinator.bubblePresenter = self.bubblePresenter;

  [self.contentSuggestionsCoordinator start];

  self.ntpMediator.refactoredFeedVisible = [self isNTPRefactoredAndFeedVisible];
  if ([self isNTPRefactoredAndFeedVisible]) {
    self.ntpViewController = [[NewTabPageViewController alloc]
        initWithContentSuggestionsViewController:
            self.contentSuggestionsCoordinator.viewController];
    self.ntpViewController.panGestureHandler = self.panGestureHandler;
    self.ntpMediator.ntpViewController = self.ntpViewController;

    UIViewController* discoverFeedViewController =
        ios::GetChromeBrowserProvider()
            ->GetDiscoverFeedProvider()
            ->NewFeedViewControllerWithScrollDelegate(self.browser,
                                                      self.ntpViewController);

    self.discoverFeedWrapperViewController =
        [[DiscoverFeedWrapperViewController alloc]
            initWithDiscoverFeedViewController:discoverFeedViewController];

    self.headerSynchronizer = [[ContentSuggestionsHeaderSynchronizer alloc]
        initWithCollectionController:self.ntpViewController
                    headerController:self.contentSuggestionsCoordinator
                                         .headerController];

    self.ntpViewController.discoverFeedWrapperViewController =
        self.discoverFeedWrapperViewController;
    self.ntpViewController.overscrollDelegate = self;
    self.ntpViewController.ntpContentDelegate = self;

    self.ntpViewController.headerController =
        self.contentSuggestionsCoordinator.headerController;
    self.ntpMediator.primaryViewController = self.ntpViewController;
  }

  base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState addObserver:self];
  self.sceneInForeground =
      sceneState.activationLevel >= SceneActivationLevelForegroundInactive;

  UIViewController* containedViewController =
      [self isNTPRefactoredAndFeedVisible]
          ? self.ntpViewController
          : self.contentSuggestionsCoordinator.viewController;

  [containedViewController
      willMoveToParentViewController:self.containerViewController];
  [self.containerViewController addChildViewController:containedViewController];
  [self.containerViewController.view addSubview:containedViewController.view];
  [containedViewController
      didMoveToParentViewController:self.containerViewController];

  containedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(containedViewController.view,
                     self.containerViewController.view);

  self.containedViewController = containedViewController;

  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  self.viewPresented = NO;
  [self updateVisible];
  [self.contentSuggestionsCoordinator stop];
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState removeObserver:self];
  self.contentSuggestionsCoordinator = nil;
  self.incognitoViewController = nil;
  self.ntpViewController = nil;
  self.discoverFeedWrapperViewController = nil;

  [self.ntpMediator shutdown];
  self.ntpMediator = nil;

  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];

  self.started = NO;
}

// Updates the visible property based on viewPresented and sceneInForeground
// properties.
// Sends metrics when NTP becomes invisible.
- (void)updateVisible {
  BOOL visible = self.viewPresented && self.sceneInForeground;
  if (visible == self.visible) {
    return;
  }
  self.visible = visible;
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    // Do not report metrics on incognito NTP.
    return;
  }
  if (visible) {
    self.didAppearTime = base::TimeTicks::Now();
  } else {
    if (!self.didAppearTime.is_null()) {
      UmaHistogramMediumTimes("NewTabPage.TimeSpent",
                              base::TimeTicks::Now() - self.didAppearTime);
      self.didAppearTime = base::TimeTicks();
    }
  }
}

#pragma mark - Properties

- (UIViewController*)viewController {
  [self start];
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.containerViewController;
  }
}

#pragma mark - Public Methods

- (void)dismissModals {
  [self.contentSuggestionsCoordinator dismissModals];
}

- (void)stopScrolling {
  if (!self.contentSuggestionsCoordinator) {
    return;
  }
  if ([self isNTPRefactoredAndFeedVisible]) {
    [self.ntpViewController stopScrolling];
  } else {
    [self.contentSuggestionsCoordinator stopScrolling];
  }
}

- (UIEdgeInsets)contentInset {
  return [self.contentSuggestionsCoordinator contentInset];
}

- (CGPoint)contentOffset {
  return [self.contentSuggestionsCoordinator contentOffset];
}

- (void)willUpdateSnapshot {
  if (self.contentSuggestionsCoordinator.started &&
      [self isNTPRefactoredAndFeedVisible]) {
    [self.ntpViewController willUpdateSnapshot];
  } else {
    [self.contentSuggestionsCoordinator willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  [self.contentSuggestionsCoordinator.headerController focusFakebox];
}

- (void)reload {
  if ([self isNTPRefactoredAndFeedVisible]) {
    ios::GetChromeBrowserProvider()->GetDiscoverFeedProvider()->RefreshFeed();
  }
  [self reloadContentSuggestions];
}

- (void)locationBarDidBecomeFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidBecomeFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidResignFirstResponder];
}

- (void)constrainDiscoverHeaderMenuButtonNamedGuide {
  [self.contentSuggestionsCoordinator
          constrainDiscoverHeaderMenuButtonNamedGuide];
}

- (void)ntpDidChangeVisibility:(BOOL)visible {
  self.viewPresented = visible;
  [self updateVisible];
}

- (void)handleDeviceRotation {
  [self.ntpViewController handleDeviceRotation];
}

#pragma mark - NewTabPageCommands

- (void)updateDiscoverFeedVisibility {
  [self stop];
  [self start];
  [self updateDiscoverFeedLayout];

  [self.containerViewController.view setNeedsLayout];
  [self.containerViewController.view layoutIfNeeded];
}

- (void)updateDiscoverFeedLayout {
  if ([self isNTPRefactoredAndFeedVisible]) {
    [self.containedViewController.view setNeedsLayout];
    [self.containedViewController.view layoutIfNeeded];
    [self.ntpViewController updateLayoutForContentSuggestions];
  }
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.contentSuggestionsCoordinator
              .headerController logoAnimationControllerOwner];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  self.sceneInForeground = level >= SceneActivationLevelForegroundInactive;
  [self updateVisible];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK(IsRefactoredNTP());
  [self updateDiscoverFeedVisibility];
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  id<ApplicationCommands, BrowserCommands, OmniboxCommands, SnackbarCommands>
      handler = static_cast<id<ApplicationCommands, BrowserCommands,
                               OmniboxCommands, SnackbarCommands>>(
          self.browser->GetCommandDispatcher());
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      [handler openURLInNewTab:[OpenNewTabCommand command]];
    } break;
    case OverscrollAction::CLOSE_TAB: {
      [handler closeCurrentTab];
      base::RecordAction(base::UserMetricsAction("OverscrollActionCloseTab"));
    } break;
    case OverscrollAction::REFRESH:
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return YES;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [[self.contentSuggestionsCoordinator.headerController toolBarView]
      snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.discoverFeedWrapperViewController.view;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat topInset =
      self.discoverFeedWrapperViewController.view.safeAreaInsets.top;
  return self.contentSuggestionsCoordinator.viewController.collectionView
             .contentSize.height +
         topInset;
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat height =
      [self.contentSuggestionsCoordinator.headerController toolBarView]
          .bounds.size.height;
  CGFloat topInset =
      self.discoverFeedWrapperViewController.view.safeAreaInsets.top;
  return height + topInset;
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // Fullscreen isn't supported here.
  return nullptr;
}

#pragma mark - NewTabPageContentDelegate

- (void)reloadContentSuggestions {
  [self.contentSuggestionsCoordinator reload];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kArticlesForYouEnabled && IsRefactoredNTP()) {
    [self updateDiscoverFeedVisibility];
  }
  if ([self isNTPRefactoredAndFeedVisible] &&
      preferenceName ==
          DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
    [self updateDiscoverFeedLayout];
  }
}

#pragma mark - Private

// YES if we're using the refactored NTP and the Discover Feed is visible.
- (BOOL)isNTPRefactoredAndFeedVisible {
  // Make sure we call this only if self.contentSuggestionsCoordinator has been
  // started.
  DCHECK(self.contentSuggestionsCoordinator.started);
  return IsRefactoredNTP() &&
         [self.contentSuggestionsCoordinator isDiscoverFeedVisible];
}

@end

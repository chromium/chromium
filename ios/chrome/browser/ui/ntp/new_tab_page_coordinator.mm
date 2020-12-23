// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageCoordinator () <OverscrollActionsControllerDelegate,
                                     SceneStateObserver>

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* ntpViewController;

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

@end

@implementation NewTabPageCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
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
  } else {
    DCHECK(!self.contentSuggestionsCoordinator);
    self.contentSuggestionsCoordinator = [[ContentSuggestionsCoordinator alloc]
        initWithBaseViewController:nil
                           browser:self.browser];
    self.contentSuggestionsCoordinator.webState = self.webState;
    self.contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
    self.contentSuggestionsCoordinator.panGestureHandler =
        self.panGestureHandler;

    [self.contentSuggestionsCoordinator start];

    if (IsRefactoredNTP()) {
      self.ntpViewController = [[NewTabPageViewController alloc]
          initWithContentSuggestionsViewController:
              self.contentSuggestionsCoordinator.viewController];

      UIViewController* discoverFeedViewController =
          ios::GetChromeBrowserProvider()
              ->GetDiscoverFeedProvider()
              ->NewFeedViewControllerWithScrollDelegate(self.browser,
                                                        self.ntpViewController);

      self.discoverFeedWrapperViewController =
          [[DiscoverFeedWrapperViewController alloc]
              initWithDiscoverFeedViewController:discoverFeedViewController];

      self.ntpViewController.discoverFeedWrapperViewController =
          self.discoverFeedWrapperViewController;
      self.ntpViewController.overscrollDelegate = self;
    }

    base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
    SceneState* sceneState =
        SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
    [sceneState addObserver:self];
    self.sceneInForeground =
        sceneState.activationLevel >= SceneActivationLevelForegroundInactive;
  }

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
    return IsRefactoredNTP()
               ? self.ntpViewController
               : self.contentSuggestionsCoordinator.viewController;
  }
}

#pragma mark - Public Methods

- (void)dismissModals {
  [self.contentSuggestionsCoordinator dismissModals];
}

- (UIEdgeInsets)contentInset {
  return [self.contentSuggestionsCoordinator contentInset];
}

- (CGPoint)contentOffset {
  return [self.contentSuggestionsCoordinator contentOffset];
}

- (void)willUpdateSnapshot {
  if (IsRefactoredNTP()) {
    [self.ntpViewController willUpdateSnapshot];
  } else {
    [self.contentSuggestionsCoordinator willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  [self.contentSuggestionsCoordinator.headerController focusFakebox];
}

- (void)reload {
  if (IsRefactoredNTP()) {
    ios::GetChromeBrowserProvider()->GetDiscoverFeedProvider()->RefreshFeed();
  }
  [self.contentSuggestionsCoordinator reload];
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
  return self.contentSuggestionsCoordinator.viewController.collectionView
      .contentSize.height;
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

@end

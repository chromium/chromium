// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presentation_context.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/thumb_strip_commands.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Enum actions for the IOS.Thumbstrip.OpenBy UMA metrics. Entries should not be
// renumbered and numeric values should never be reused.
enum class ThumbstripOpenByAction {
  TAB_STRIP_DRAG_DOWN = 0,
  PRIMARY_TOOLBAR_DRAG_DOWN = 1,
  WEB_PAGE_SCROLL_DOWN = 2,
  kMaxValue = WEB_PAGE_SCROLL_DOWN,
};

// Enum actions for the IOS.Thumbstrip.CloseBy UMA metrics. Entries should not
// be renumbered and numeric values should never be reused.
enum class ThumbstripCloseByAction {
  PRIMARY_TOOLBAR_DRAG_UP = 0,
  FAKE_TAB_TAP = 1,
  USER_NAVIGATION = 2,
  WEB_PAGE_SCROLL_UP = 3,
  NEW_TAB_BUTTON = 4,
  OMNIBOX_FOCUS = 5,
  BACKGROUND_TAP = 6,
  BACKGROUND_SWIPE = 7,
  kMaxValue = BACKGROUND_SWIPE,
};

}  // namespace

@interface ThumbStripCoordinator () <ThumbStripCommands,
                                     ThumbStripNavigationConsumer>

@property(nonatomic, strong) ThumbStripMediator* mediator;

// The initial state for the pan handler.
@property(nonatomic, assign) ViewRevealState initialState;

@property(nonatomic, assign) BOOL started;

@end

@implementation ThumbStripCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              initialState:(ViewRevealState)initialState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _initialState = initialState;
  }
  return self;
}

- (void)start {
  self.started = YES;
  CGFloat baseViewHeight = self.baseViewController.view.frame.size.height;
  self.panHandler = [[ViewRevealingVerticalPanHandler alloc]
      initWithPeekedHeight:kThumbStripHeight
            baseViewHeight:baseViewHeight
              initialState:self.initialState];

  self.mediator = [[ThumbStripMediator alloc] init];
  self.mediator.consumer = self;
  if (self.regularBrowser) {
    [self.regularBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ThumbStripCommands)];
    self.mediator.regularWebStateList = self.regularBrowser->GetWebStateList();
    self.mediator.regularOverlayPresentationContext =
        OverlayPresentationContext::FromBrowser(
            self.regularBrowser, OverlayModality::kInfobarBanner);
  }
  if (self.incognitoBrowser) {
    [self.incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ThumbStripCommands)];
    self.mediator.incognitoWebStateList =
        self.incognitoBrowser->GetWebStateList();
    self.mediator.incognitoOverlayPresentationContext =
        OverlayPresentationContext::FromBrowser(
            self.incognitoBrowser, OverlayModality::kInfobarBanner);
  }
  self.mediator.webViewScrollViewObserver = self.panHandler;
  [self.panHandler addAnimatee:self.mediator];

  // For metrics only:
  [self.panHandler addAnimatee:self];
}

- (void)stop {
  self.started = NO;
  self.mediator.regularWebStateList = nil;
  self.mediator.incognitoWebStateList = nil;
  self.mediator.webViewScrollViewObserver = nil;
  self.panHandler = nil;
  self.mediator = nil;

  if (self.regularBrowser) {
    [self.regularBrowser->GetCommandDispatcher()
        stopDispatchingForProtocol:@protocol(ThumbStripCommands)];
  }
  self.regularBrowser = nullptr;
  // Dispatching is stopped in -setIncognitoBrowser.
  self.incognitoBrowser = nullptr;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  if (_incognitoBrowser) {
    [_incognitoBrowser->GetCommandDispatcher()
        stopDispatchingForProtocol:@protocol(ThumbStripCommands)];
  }
  _incognitoBrowser = incognitoBrowser;
  if (!self.started) {
    return;
  }
  if (_incognitoBrowser) {
    [_incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(ThumbStripCommands)];
  }
  self.mediator.incognitoWebStateList =
      _incognitoBrowser ? _incognitoBrowser->GetWebStateList() : nullptr;

  self.mediator.incognitoOverlayPresentationContext =
      _incognitoBrowser
          ? OverlayPresentationContext::FromBrowser(
                _incognitoBrowser, OverlayModality::kInfobarBanner)
          : nullptr;
}

#pragma mark - ThumbStripNavigationConsumer

- (void)navigationDidStart {
  // Close the thumb strip if navigation occurred in peeked state. This
  // indicates the user wants to keep using the current tab.
  if (self.panHandler.currentState == ViewRevealState::Peeked) {
    [self closeThumbStripWithTrigger:ViewRevealTrigger::UserNavigation];
  }
}

#pragma mark - ThumbStripCommands

- (void)closeThumbStripWithTrigger:(ViewRevealTrigger)trigger {
  [self.panHandler setNextState:ViewRevealState::Hidden
                       animated:YES
                        trigger:trigger];
}

#pragma mark - ViewRevealingAnimatee

- (void)didAnimateViewRevealFromState:(ViewRevealState)startViewRevealState
                              toState:(ViewRevealState)currentViewRevealState
                              trigger:(ViewRevealTrigger)trigger {
  // Cancelled.
  if (startViewRevealState == ViewRevealState::Hidden &&
      currentViewRevealState == ViewRevealState::Hidden) {
    switch (trigger) {
      // User was dragging from the tab strip, then cancelled the action.
      case ViewRevealTrigger::TabStrip:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.CancelBy",
            ThumbstripOpenByAction::TAB_STRIP_DRAG_DOWN);
        break;
      // User was dragging from the primary toolbar, then cancelled the action.
      case ViewRevealTrigger::PrimaryToolbar:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.CancelBy",
            ThumbstripOpenByAction::PRIMARY_TOOLBAR_DRAG_DOWN);
        break;
      // User scrolling the web view / ntp, but reversed course cancelling the
      // action.
      case ViewRevealTrigger::WebScroll:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.CancelBy",
            ThumbstripOpenByAction::WEB_PAGE_SCROLL_DOWN);
        break;
      default:
        // This is reached at startup, but doesn't require logging. It can also
        // happens when app is backgrounded.
        break;
    }
    // Opening.
  } else if (startViewRevealState == ViewRevealState::Hidden &&
             currentViewRevealState == ViewRevealState::Peeked) {
    switch (trigger) {
      // User is dragging from the tab strip.
      case ViewRevealTrigger::TabStrip:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.OpenBy",
            ThumbstripOpenByAction::TAB_STRIP_DRAG_DOWN);
        break;
      // User is dragging from the primary toolbar.
      case ViewRevealTrigger::PrimaryToolbar:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.OpenBy",
            ThumbstripOpenByAction::PRIMARY_TOOLBAR_DRAG_DOWN);
        break;
      // Triggered by user scrolling the web view.
      case ViewRevealTrigger::WebScroll:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.OpenBy",
            ThumbstripOpenByAction::WEB_PAGE_SCROLL_DOWN);
        break;
      default:
        // Ignore this, it can happen when app is backgrounded.
        break;
    }
    // Closing.
  } else if (startViewRevealState == ViewRevealState::Peeked &&
             currentViewRevealState == ViewRevealState::Hidden) {
    switch (trigger) {
      // User is dragging from the primary toolbar.
      case ViewRevealTrigger::PrimaryToolbar:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.CloseBy",
            ThumbstripCloseByAction::PRIMARY_TOOLBAR_DRAG_UP);
        break;
      // User tapped the fake tab at bottom.
      case ViewRevealTrigger::FakeTab:
        base::UmaHistogramEnumeration("IOS.Thumbstrip.CloseBy",
                                      ThumbstripCloseByAction::FAKE_TAB_TAP);
        break;
      // Triggered by user web page navigation.
      case ViewRevealTrigger::UserNavigation:
        base::UmaHistogramEnumeration("IOS.Thumbstrip.CloseBy",
                                      ThumbstripCloseByAction::USER_NAVIGATION);
        break;
      // Triggered by user scrolling the web view.
      case ViewRevealTrigger::WebScroll:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.CloseBy",
            ThumbstripCloseByAction::WEB_PAGE_SCROLL_UP);
        break;
      // User requested tab grid opening or closing or new tab.
      case ViewRevealTrigger::TabGrid:
        base::UmaHistogramEnumeration("IOS.Thumbstrip.CloseBy",
                                      ThumbstripCloseByAction::NEW_TAB_BUTTON);
        break;
      // Triggered by user focus on omnibox.
      case ViewRevealTrigger::OmniboxFocus:
        base::UmaHistogramEnumeration("IOS.Thumbstrip.CloseBy",
                                      ThumbstripCloseByAction::OMNIBOX_FOCUS);
        break;
      // Triggered by user tap on background.
      case ViewRevealTrigger::BackgroundTap:
        base::UmaHistogramEnumeration("IOS.Thumbstrip.CloseBy",
                                      ThumbstripCloseByAction::BACKGROUND_TAP);
        break;
      // Triggered by user swipping up background.
      case ViewRevealTrigger::BackgroundSwipe:
        base::UmaHistogramEnumeration(
            "IOS.Thumbstrip.CloseBy",
            ThumbstripCloseByAction::BACKGROUND_SWIPE);
        break;
      default:
        // Ignore this, it can happen when app is backgrounded.
        break;
    }
  }
}

@end

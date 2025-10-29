// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace {

// Time to transition in seconds.
const int kTransitionTimeInSeconds = 2;

}  // anonymous namespace

@interface LocationBarBadgeMediator () <CRWWebStateObserver,
                                        WebStateListObserving>
@end

@implementation LocationBarBadgeMediator {
  // Timer keeping track of when badge transitions to a promo.
  std::unique_ptr<base::OneShotTimer> _promoStartTimer;
  // Timer keeping track of when to return to the default badge state after
  // a promo was shown.
  std::unique_ptr<base::OneShotTimer> _promoEndTimer;
  // The WebStateList for the Browser this mediator is in.
  raw_ptr<WebStateList> _webStateList;
  // The observer for the WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // The active WebState.
  raw_ptr<web::WebState> _activeWebState;
  // The observer for the WebState.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _activeWebState = _webStateList->GetActiveWebState();
    if (_activeWebState) {
      _activeWebState->AddObserver(_webStateObserver.get());
    }
  }
  return self;
}

- (void)disconnect {
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_webStateObserver.get());
    _activeWebState = nullptr;
  }
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateList = nullptr;
  }
}

#pragma mark - BadgeViewVisibilityDelegate

- (void)setBadgeViewHidden:(BOOL)hidden {
  [self.consumer setBadge:LocationBarBadgeType::kBadgeView hidden:hidden];
}

#pragma mark - IncognitoBadgeViewVisibilityDelegate

- (void)setIncognitoBadgeViewHidden:(BOOL)hidden {
  [self.consumer setBadge:LocationBarBadgeType::kIncognito hidden:hidden];
}

#pragma mark - ReaderModeChipVisibilityDelegate

- (void)readerModeChipCoordinator:(ReaderModeChipCoordinator*)coordinator
       didSetReaderModeChipHidden:(BOOL)hidden {
  [self.consumer setBadge:LocationBarBadgeType::kReaderMode hidden:hidden];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (status.active_web_state_change()) {
    [self updateActiveWebState:status.new_active_web_state];
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  [self.consumer hideBadge];
}

- (void)webStateWasDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_activeWebState, webState);
  _activeWebState->RemoveObserver(_webStateObserver.get());
  _activeWebState = nullptr;
}

#pragma mark - LocationBarBadgeCommands

- (void)updateBadgeConfig:(LocationBarBadgeConfiguration*)config {
  [self resetTimersAndUIStateAnimated:NO];
  [self.consumer setBadgeConfig:config];
  [self.consumer collapseBadgeContainer];
  [self.consumer showBadge];

  if (config.badgeText) {
    [self startPromoTimer];
  }
}

- (void)updateColorForIPH {
  [self.consumer highlightBadge:YES];
}

#pragma mark - LocationBarBadgeMutator

- (void)dismissIPHAnimated:(BOOL)animated {
  [self.consumer highlightBadge:NO];
}

- (void)badgeTapped:(LocationBarBadgeType)badgeType {
  // Cancel any pending transition timers since user interacted with the badge.
  [self resetTimersAndUIStateAnimated:YES];

  if (badgeType == LocationBarBadgeType::kAskGeminiChip) {
    [self.BWGCommandHandler
        startBWGFlowWithEntryPoint:bwg::EntryPoint::OmniboxChip];
  }
}

- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  [self.delegate setLocationBarLabelCenteredBetweenContent:self
                                                  centered:centered];
}

#pragma mark - Private

// Starts the promo timer.
- (void)startPromoTimer {
  __weak LocationBarBadgeMediator* weakSelf = self;
  _promoStartTimer = std::make_unique<base::OneShotTimer>();
  _promoStartTimer->Start(FROM_HERE, base::Seconds(kTransitionTimeInSeconds),
                          base::BindOnce(^{
                            [weakSelf setupAndExpandChip];
                          }));
}

// Transforms the badge into a chip and starts the timers to transition back to
// the default badge state.
- (void)setupAndExpandChip {
  if (![self.delegate canShowLargeContextualPanelEntrypoint:self]) {
    // Enable fullscreen in case it was disabled when trying to show the IPH.
    [self.delegate enableFullscreen];
    return;
  }

  [self.delegate disableFullscreen];
  [self.consumer expandBadgeContainer];

  // TODO(crbug.com/454072799): Add metric log for chip showing.

  __weak LocationBarBadgeMediator* weakSelf = self;

  _promoEndTimer = std::make_unique<base::OneShotTimer>();
  _promoEndTimer->Start(FROM_HERE, base::Seconds(kTransitionTimeInSeconds),
                        base::BindOnce(^{
                          [weakSelf cleanupAndTransitionToDefaultBadgeState];
                        }));
}

// Changes the UI to the default badge state.
- (void)cleanupAndTransitionToDefaultBadgeState {
  [self.consumer collapseBadgeContainer];
  [self.delegate enableFullscreen];
}

// Cancels pending timers, dismisses any showing IPH and removes any active
// fullscreen disabler.
- (void)resetTimersAndUIStateAnimated:(BOOL)animated {
  _promoStartTimer = nullptr;
  _promoEndTimer = nullptr;
  [self dismissIPHAnimated:animated];
  [self cleanupAndTransitionToDefaultBadgeState];
}

// Update `_activeWebState` to ensure the active WebState is observed.
- (void)updateActiveWebState:(web::WebState*)webState {
  if (_activeWebState == webState) {
    return;
  }
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_webStateObserver.get());
  }
  _activeWebState = webState;
  if (_activeWebState) {
    _activeWebState->AddObserver(_webStateObserver.get());
  }
}

@end

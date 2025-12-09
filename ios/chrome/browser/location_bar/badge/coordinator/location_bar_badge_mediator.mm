// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/timer/timer.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Time to start transition in seconds.
const int kStartExpandTransitionTimeInSeconds = 2;
// Time to start the collapse transition in seconds.
const int kStartCollapseTransitionTimeInSeconds = 5;
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
  // Tracker for feature events.
  raw_ptr<feature_engagement::Tracker> _tracker;
  // Pref service.
  raw_ptr<PrefService> _prefService;
  // Gemini service
  raw_ptr<BwgService> _geminiService;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                             tracker:(feature_engagement::Tracker*)tracker
                         prefService:(PrefService*)prefService
                       geminiService:(BwgService*)geminiService {
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
    _tracker = tracker;
    _prefService = prefService;
    _geminiService = geminiService;
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

  _promoStartTimer = nullptr;
  _promoEndTimer = nullptr;
  _tracker = nil;
  _prefService = nil;
  _geminiService = nil;
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
  RecordLocationBarBadgeUpdate(config.badgeType);
  // If another badge update is sent while an initial badge is still displayed,
  // ignore the update.
  if (![self shouldShowBadge:config.badgeType] ||
      [self.consumer isBadgeVisible]) {
    // TODO (crbug.com/445719031): Store badge updates in consumer instead of
    // ignoring the update.
    return;
  }

  [self resetTimersAndUIStateAnimated:NO];
  [self.consumer setBadgeConfig:config];
  [self.consumer collapseBadgeContainer];
  [self.consumer showBadge];
  [self logBadgeShown:config.badgeType];

  if (config.badgeText) {
    [self startPromoTimer];
  }
}

- (void)updateColorForIPH {
  [self.consumer highlightBadge:YES];
}

- (void)markDisplayedBadgeAsUnread:(BOOL)unread {
  [self.consumer showUnreadBadge:unread];
}

#pragma mark - LocationBarBadgeMutator

- (void)dismissIPHAnimated:(BOOL)animated {
  [self.consumer highlightBadge:NO];
}

- (void)badgeTapped:(LocationBarBadgeConfiguration*)badgeConfig {
  // Cancel any pending transition timers since user interacted with the badge.
  [self resetTimersAndUIStateAnimated:YES];

  switch (badgeConfig.badgeType) {
    case LocationBarBadgeType::kGeminiContextualCueChip:
      if (IsAskGeminiChipPrepopulateFloatyEnabled()) {
        BwgTabHelper* BWGTabHelper =
            BwgTabHelper::FromWebState(_activeWebState);
        if (BWGTabHelper) {
          BWGTabHelper->SetContextualCueLabel(
              l10n_util::GetNSString(IDS_IOS_ASK_GEMINI_CHIP_PREFILL_PROMPT));
        }
      }
      [self.BWGCommandHandler
          startBWGFlowWithEntryPoint:bwg::EntryPoint::OmniboxChip];
      _tracker->NotifyEvent(
          feature_engagement::events::kIOSGeminiContextualCueChipUsed);
      break;
    default:
      break;
  }
}

- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  [self.delegate setLocationBarLabelCenteredBetweenContent:self
                                                  centered:centered];
}

- (void)handleBadgeContainerCollapse:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kGeminiContextualCueChip:
      _tracker->Dismissed(feature_engagement::kIPHiOSGeminiContextualCueChip);
      break;
    default:
      break;
  }
}

#pragma mark - Private

// Starts the promo timer.
- (void)startPromoTimer {
  __weak LocationBarBadgeMediator* weakSelf = self;
  _promoStartTimer = std::make_unique<base::OneShotTimer>();
  _promoStartTimer->Start(FROM_HERE,
                          base::Seconds(kStartExpandTransitionTimeInSeconds),
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
  _promoEndTimer->Start(FROM_HERE,
                        base::Seconds(kStartCollapseTransitionTimeInSeconds),
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

// Checks FET (Feature Engagement Tracker) criteria for a given `badgeType`. By
// default, returns YES as badges should show given no other criteria.
- (BOOL)shouldShowBadge:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kGeminiContextualCueChip:
      if ([self shouldShowGeminiContextualChip]) {
        return YES;
      }
      return NO;
    default:
      return YES;
  }
}

// Logs metrics or preferences for a specific `badgeType` being shown.
- (void)logBadgeShown:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kGeminiContextualCueChip:
      _tracker->NotifyEvent(
          feature_engagement::events::kIOSGeminiContextualCueChipTriggered);
      _prefService->SetTime(prefs::kLastGeminiContextualChipDisplayedTimestamp,
                            base::Time::Now());
      break;
    default:
      break;
  }
  RecordLocationBarBadgeShown(badgeType);
}

// Whether to show Gemini contextual chip. Checks if the page is eligible for
// Gemini, a user has consented, and checks if two hours has passed since the
// last chip display.
- (BOOL)shouldShowGeminiContextualChip {
  BOOL isPageEligible =
      _geminiService->IsBwgAvailableForWebState(_activeWebState);
  BOOL isUserConsented = _prefService->GetBoolean(prefs::kIOSBwgConsent);

  // Checks if an eligible amount of time has passed since the last chip
  // display.
  base::Time lastDisplayedChipTime =
      _prefService->GetTime(prefs::kLastGeminiContextualChipDisplayedTimestamp);
  base::TimeDelta timeSinceLastShown =
      base::Time::Now() - lastDisplayedChipTime;
  BOOL eligibleTimeWindow =
      timeSinceLastShown >= base::Hours(kGeminiContextualCueChipSlidingWindow);

  if (IsAskGeminiChipIgnoreCriteria()) {
    return YES;
  }

  return isPageEligible && isUserConsented && eligibleTimeWindow &&
         _tracker->ShouldTriggerHelpUI(
             feature_engagement::kIPHiOSGeminiContextualCueChip);
}

@end

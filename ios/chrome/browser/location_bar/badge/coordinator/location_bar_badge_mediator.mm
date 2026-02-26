// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator_delegate.h"
#import "ios/chrome/browser/location_bar/badge/metrics/location_bar_badge_metrics.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_configuration.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_constants.h"
#import "ios/chrome/browser/location_bar/badge/ui/location_bar_badge_consumer.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Time to start transition in seconds.
const int kStartExpandTransitionTimeInSeconds = 2;
// Time to start the collapse transition in seconds.
const int kStartCollapseTransitionTimeInSeconds = 5;
}  // anonymous namespace

@interface LocationBarBadgeMediator () <ContextualPanelTabHelperObserving,
                                        CRWWebStateObserver,
                                        InfobarBadgeTabHelperObserving,
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
  // Bridge for the InfobarBadgeTabHelper observation.
  std::unique_ptr<InfobarBadgeTabHelperObserverBridge>
      _infobarBadgeObserverBridge;
  std::unique_ptr<base::ScopedObservation<InfobarBadgeTabHelper,
                                          InfobarBadgeTabHelperObserverBridge>>
      _infobarBadgeObservation;
  // Whether there currently are any Infobar badges being shown.
  BOOL _infobarBadgesCurrentlyShown;
  // Bridge for the ContextualPanelTabHelper observation.
  std::unique_ptr<ContextualPanelTabHelperObserverBridge>
      _contextualPanelObserverBridge;
  // Forwarder to always be observing the active ContextualPanelTabHelper.
  std::unique_ptr<ActiveContextualPanelTabHelperObservationForwarder>
      _activeContextualPanelObservationForwarder;
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
    if (IsLocationBarBadgeMigrationEnabled()) {
      // Setup InfobarBadgeTabHelper observation.
      _infobarBadgeObserverBridge =
          std::make_unique<InfobarBadgeTabHelperObserverBridge>(self);
      _infobarBadgeObservation = std::make_unique<base::ScopedObservation<
          InfobarBadgeTabHelper, InfobarBadgeTabHelperObserverBridge>>(
          _infobarBadgeObserverBridge.get());

      if (_activeWebState) {
        _infobarBadgeObservation->Observe(
            InfobarBadgeTabHelper::FromWebState(_activeWebState));
      }

      // Set up active ContextualPanelTabHelper observation.
      _contextualPanelObserverBridge =
          std::make_unique<ContextualPanelTabHelperObserverBridge>(self);
      _activeContextualPanelObservationForwarder =
          std::make_unique<ActiveContextualPanelTabHelperObservationForwarder>(
              webStateList, _contextualPanelObserverBridge.get());
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

  if (IsLocationBarBadgeMigrationEnabled()) {
    _infobarBadgeObservation->Reset();
    _infobarBadgeObservation.reset();
    _infobarBadgeObserverBridge.reset();
    _activeContextualPanelObservationForwarder.reset();
    _contextualPanelObserverBridge.reset();
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
  // Return early if the active web state is the same as before the change.
  if (!status.active_web_state_change()) {
    return;
  }

  if (IsLocationBarBadgeMigrationEnabled()) {
    // De-register observer bridge for the old WebState's InfobarBadgeTabHelper.
    _infobarBadgeObservation->Reset();
  }

  if (status.old_active_web_state) {
    [self updateOldActiveWebstate:status.old_active_web_state];
  }

  [self resetTimersAndUIStateAnimated:NO];

  // Return early if no new webstates are active.
  if (!status.new_active_web_state) {
    if (_activeWebState) {
      _activeWebState->RemoveObserver(_webStateObserver.get());
      _activeWebState = nil;
    }
    return;
  }

  [self updateActiveWebState:status.new_active_web_state];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  // Do not modify badge state if the navigation is on the same document.
  if (!navigationContext->IsSameDocument()) {
    [self.consumer hideBadge];
  }
}

- (void)webStateWasDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_activeWebState, webState);
  _activeWebState->RemoveObserver(_webStateObserver.get());
  _activeWebState = nullptr;
}

#pragma mark - InfobarBadgeTabHelperObserving

- (void)infobarBadgesUpdated:(InfobarBadgeTabHelper*)tabHelper {
  // Return early if the notification doesn't come from the currently active
  // webstate's tab helper.
  raw_ptr<web::WebState> active_web_state = _webStateList->GetActiveWebState();
  if (!active_web_state || active_web_state->IsBeingDestroyed()) {
    return;
  }
  if (tabHelper != InfobarBadgeTabHelper::FromWebState(active_web_state)) {
    return;
  }

  size_t badgesCount = tabHelper->GetInfobarBadgesCount();

  BOOL infobarBadgesCurrentlyShown = badgesCount > 0;

  // Disable contextual panel separator when Proactive Suggestions Framework is
  // enabled to prevent conflicts.
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    infobarBadgesCurrentlyShown = NO;
  }

  if (_infobarBadgesCurrentlyShown == infobarBadgesCurrentlyShown) {
    return;
  }
  _infobarBadgesCurrentlyShown = infobarBadgesCurrentlyShown;

  if (_infobarBadgesCurrentlyShown) {
    [self dismissIPHAnimated:YES];
  }

  [self.consumer setInfobarBadgesCurrentlyShown:_infobarBadgesCurrentlyShown];
}

#pragma mark - ContextualPanelTabHelperObserving

- (void)contextualPanel:(ContextualPanelTabHelper*)tabHelper
             hasNewData:
                 (std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>)
                     item_configurations {
  [self activeTabHasNewData:item_configurations.empty()
                                ? nullptr
                                : item_configurations[0].get()];
}

- (void)contextualPanelTabHelperDestroyed:(ContextualPanelTabHelper*)tabHelper {
  [self activeTabHasNewData:nullptr];
}

- (void)contextualPanelOpened:(ContextualPanelTabHelper*)tabHelper {
  [self.consumer transitionToContextualPanelOpenedState:YES];
}

- (void)contextualPanelClosed:(ContextualPanelTabHelper*)tabHelper {
  [self.consumer transitionToContextualPanelOpenedState:NO];
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
  [self badgeShown:config.badgeType];

  if ([self shouldShowIPH:config.badgeType]) {
    [self startIPHTimer:config];
    return;
  }

  // If there's badge text, attempt to show a chip.
  if (config.badgeText) {
    [self startPromoTimer:config];
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
  [_entrypointHelpHandler dismissContextualPanelEntrypointIPH:animated];
  [self.consumer highlightBadge:NO];
}

- (void)badgeTapped:(LocationBarBadgeConfiguration*)badgeConfig {
  // Cancel any pending transition timers since user interacted with the badge.
  [self resetTimersAndUIStateAnimated:YES];

  switch (badgeConfig.badgeType) {
    case LocationBarBadgeType::kGeminiContextualCueChip: {
      NSString* prompt = nil;
      if (IsAskGeminiChipPrepopulateFloatyEnabled()) {
        prompt = l10n_util::GetNSString(IDS_IOS_ASK_GEMINI_CHIP_PREFILL_PROMPT);
      }

      GeminiStartupState* state = [[GeminiStartupState alloc]
          initWithEntryPoint:gemini::EntryPoint::OmniboxChip];
      state.prepopulatedPrompt = prompt;
      [self.BWGCommandHandler startGeminiFlowWithStartupState:state];
      _tracker->NotifyEvent(
          feature_engagement::events::kIOSGeminiContextualCueChipUsed);

      // Ensure badge is hidden after the user interacts with it.
      if ([self.consumer isBadgeVisible]) {
        [self.consumer hideBadge];
      }
      break;
    }
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kPriceInsights:
    case LocationBarBadgeType::kReaderMode:
      [self contextualPanelEntrypointBadgeTapped];
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
      if (!IsAskGeminiChipIgnoreCriteria()) {
        _tracker->Dismissed(feature_engagement::kIPHiOSGeminiContextualCueChip);
      }
      break;
    default:
      break;
  }
}

#pragma mark - Private

// Starts the promo timer.
- (void)startPromoTimer:(LocationBarBadgeConfiguration*)badgeConfig {
  __weak LocationBarBadgeMediator* weakSelf = self;
  _promoStartTimer = std::make_unique<base::OneShotTimer>();
  _promoStartTimer->Start(FROM_HERE,
                          base::Seconds(kStartExpandTransitionTimeInSeconds),
                          base::BindOnce(^{
                            [weakSelf setupAndExpandChip:badgeConfig];
                          }));
}

// Timer to end the promo.
- (void)startEndPromoTimer {
  __weak LocationBarBadgeMediator* weakSelf = self;
  _promoEndTimer = std::make_unique<base::OneShotTimer>();
  _promoEndTimer->Start(FROM_HERE,
                        base::Seconds(kStartCollapseTransitionTimeInSeconds),
                        base::BindOnce(^{
                          [weakSelf cleanupAndTransitionToDefaultBadgeState];
                        }));
}

// Starts the promo timer for an IPH.
- (void)startIPHTimer:(LocationBarBadgeConfiguration*)badgeConfig {
  __weak LocationBarBadgeMediator* weakSelf = self;
  _promoStartTimer = std::make_unique<base::OneShotTimer>();
  _promoStartTimer->Start(FROM_HERE,
                          base::Seconds(kStartExpandTransitionTimeInSeconds),
                          base::BindOnce(^{
                            [weakSelf setupAndShowIPH:badgeConfig];
                          }));
}

// Transforms the badge into a chip and starts the timers to transition back to
// the default badge state.
- (void)setupAndExpandChip:(LocationBarBadgeConfiguration*)badgeConfig {
  if (![self shouldShowChip:badgeConfig] || ![self.delegate canShowChip:self]) {
    // Enable fullscreen in case it was disabled when trying to show the IPH.
    [self.delegate enableFullscreen];
    return;
  }

  [self.delegate disableFullscreen];
  [self.consumer expandBadgeContainer];
  [self chipShown:badgeConfig.badgeType];
  [self startEndPromoTimer];
  _promoStartTimer = nullptr;
}

// Shows the IPH related to the `badgeConfig`.
- (void)setupAndShowIPH:(LocationBarBadgeConfiguration*)badgeConfig {
  [self.delegate disableFullscreen];
  [self showIPH:badgeConfig];
}

// Shows a IPH with `badgeType`.
- (void)showIPH:(LocationBarBadgeConfiguration*)badgeConfig {
  LocationBarBadgeType badgeType = badgeConfig.badgeType;
  switch (badgeType) {
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kPriceInsights:
    case LocationBarBadgeType::kReaderMode: {
      // Special case for first entrypoint appearances where an IPH is shown
      // instead of the large entrypoint. If showing the IPH fails, will
      // fallback to showing the large entrypoint.
      if (![self shouldShowIPH:badgeConfig.badgeType]) {
        [self setupAndExpandChip:badgeConfig];
        return;
      }

      ContextualPanelTabHelper* contextualPanelTabHelper =
          ContextualPanelTabHelper::FromWebState(
              _webStateList->GetActiveWebState());
      ContextualPanelItemConfiguration* config =
          contextualPanelTabHelper->GetFirstCachedConfig().get();
      NSString* text = base::SysUTF8ToNSString(config->entrypoint_message);

      // Try to show the entrypoint's IPH and capture the result.
      BOOL success = [self attemptShowingEntrypointIPHWithText:text
                                                        config:config];

      // Show the large entrypoint if showing the IPH was not successful.
      if (!success) {
        [self setupAndExpandChip:badgeConfig];
        return;
      }

      [self.consumer highlightBadge:YES];

      std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
          metricsData = contextualPanelTabHelper->GetMetricsData();
      if (metricsData) {
        metricsData->iphWasShown = true;
      }

      contextualPanelTabHelper->SetLoudMomentEntrypointShown(true);
      // IPH was shown, so fire loud display metrics.
      [LocationBarBadgeMetrics
          logLoudDisplayContextualPanelEntrypointMetrics:metricsData];
      break;
    }
    case LocationBarBadgeType::kNone:
      break;
    default:
      break;
  }
  [self startEndPromoTimer];
}

// Changes the UI to the default badge state.
- (void)cleanupAndTransitionToDefaultBadgeState {
  [self dismissIPHAnimated:YES];
  [self.consumer collapseBadgeContainer];
  [self.delegate enableFullscreen];
  _promoEndTimer = nullptr;
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
  if (_activeWebState) {
    _activeWebState->RemoveObserver(_webStateObserver.get());
  }

  _activeWebState = webState;
  _activeWebState->AddObserver(_webStateObserver.get());

  if (!IsLocationBarBadgeMigrationEnabled()) {
    return;
  }

  // Register observer bridge for the new WebState's InfobarBadgeTabHelper.
  _infobarBadgeObservation->Observe(
      InfobarBadgeTabHelper::FromWebState(_activeWebState));

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(_activeWebState);
  // In some cases (e.g. tests), an OTR web state (without a
  // ContextualPanelTabHelper) can be added to the web state list being
  // observed, even though this mediator is only created for non-OTR browsers.
  // Just in case, make sure there actually is a ContextualPanelTabHelper.
  if (!contextualPanelTabHelper) {
    return;
  }
  [self activeTabHasNewData:contextualPanelTabHelper->GetFirstCachedConfig()
                                .get()];
}

// Update old active WebState.
- (void)updateOldActiveWebstate:(web::WebState*)webState {
  if (!IsLocationBarBadgeMigrationEnabled()) {
    return;
  }

  // Update old active web state's visible time for ContextualPanelEntrypoint.
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(webState);
  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>& metricsData =
      contextualPanelTabHelper->GetMetricsData();
  if (metricsData && metricsData->appearance_time) {
    metricsData->time_visible +=
        base::Time::Now() - metricsData->appearance_time.value();
    metricsData->appearance_time = std::nullopt;
  }
}

// Checks FET (Feature Engagement Tracker) criteria for a given `badgeType`. By
// default, returns YES as badges should show given no other criteria.
- (BOOL)shouldShowBadge:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kGeminiContextualCueChip:
      if ([self shouldShowGeminiContextualBadge]) {
        return YES;
      }
      return NO;
    case LocationBarBadgeType::kNone:
      return NO;
    default:
      return YES;
  }
}

// Whether a chip with `badgeType` should show. Only use before the chip
// shows as this function can lead to `ShouldTriggerHelpUI` calls which are
// properly handled. FET dismiss calls are handled as long as the chip is shown.
- (BOOL)shouldShowChip:(LocationBarBadgeConfiguration*)badgeConfig {
  if (!badgeConfig.badgeText) {
    return NO;
  }

  LocationBarBadgeType badgeType = badgeConfig.badgeType;
  switch (badgeType) {
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kPriceInsights:
    case LocationBarBadgeType::kReaderMode: {
      ContextualPanelTabHelper* contextualPanelTabHelper =
          ContextualPanelTabHelper::FromWebState(
              _webStateList->GetActiveWebState());
      ContextualPanelItemConfiguration* config =
          contextualPanelTabHelper->GetFirstCachedConfig().get();
      return [self canShowLargeEntrypointWithConfig:config];
    }
    case LocationBarBadgeType::kGeminiContextualCueChip:
      return [self shouldShowGeminiContextualChip];
    case LocationBarBadgeType::kNone:
      return NO;
    default:
      return YES;
  }
}

// Whether an IPH with `badgeType` should show.
- (BOOL)shouldShowIPH:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kPriceInsights:
    case LocationBarBadgeType::kReaderMode: {
      ContextualPanelTabHelper* contextualPanelTabHelper =
          ContextualPanelTabHelper::FromWebState(
              _webStateList->GetActiveWebState());
      ContextualPanelItemConfiguration* config =
          contextualPanelTabHelper->GetFirstCachedConfig().get();
      return [self canShowEntrypointIPHWithConfig:config];
    }
    default:
      return NO;
      ;
  }
}

// Handles additional logic for `badgeType` when badge is shown.
- (void)badgeShown:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kPriceInsights:
    case LocationBarBadgeType::kReaderMode: {
      ContextualPanelTabHelper* contextualPanelTabHelper =
          ContextualPanelTabHelper::FromWebState(
              _webStateList->GetActiveWebState());
      std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
          metricsData = contextualPanelTabHelper->GetMetricsData();
      [LocationBarBadgeMetrics
          logFirstDisplayContextualPanelEntrypointMetrics:metricsData];
      break;
    }
    default:
      break;
  }
  RecordLocationBarBadgeShown(badgeType);
}

// Handles additional logic for `badgeType` when the chip is shown.
- (void)chipShown:(LocationBarBadgeType)badgeType {
  switch (badgeType) {
    case LocationBarBadgeType::kContextualPanelEntryPointSample:
    case LocationBarBadgeType::kPriceInsights:
    case LocationBarBadgeType::kReaderMode: {
      ContextualPanelTabHelper* contextualPanelTabHelper =
          ContextualPanelTabHelper::FromWebState(
              _webStateList->GetActiveWebState());
      std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
          metricsData = contextualPanelTabHelper->GetMetricsData();
      if (metricsData) {
        metricsData->largeEntrypointWasShown = true;
      }
      contextualPanelTabHelper->SetLoudMomentEntrypointShown(true);
      [LocationBarBadgeMetrics
          logLoudDisplayContextualPanelEntrypointMetrics:metricsData];
      break;
    }
    case LocationBarBadgeType::kGeminiContextualCueChip:
      _prefService->SetTime(prefs::kLastGeminiContextualChipDisplayedTimestamp,
                            base::Time::Now());
      break;
    default:
      break;
  }
}

// Whether to show Gemini contextual badge before it transforms into a chip.
// Checks if the page is eligible for Gemini, a user has consented, and checks
// if two hours has passed since the last chip display.
- (BOOL)shouldShowGeminiContextualBadge {
  BOOL isPageEligible =
      _geminiService->IsBwgAvailableForWebState(_activeWebState);
  // TODO(crbug.com/465766925): Remove when feature is enabled by default.
  BOOL isConsentEligible = IsAskGeminiChipAllowNonconsentedUsersEnabled() ||
                           _prefService->GetBoolean(prefs::kIOSBwgConsent);

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

  // If the promo timers have already started, do not allow the chip to show to
  // avoid calling `ShouldTriggerHelpUI()` when the chip is in the process of
  // being displayed.
  if ([self arePromoTimersRunning]) {
    return NO;
  }

  return isPageEligible && isConsentEligible && eligibleTimeWindow &&
         _tracker->WouldTriggerHelpUI(
             feature_engagement::kIPHiOSGeminiContextualCueChip);
}

// Whether to show Gemini contextual chip.
- (BOOL)shouldShowGeminiContextualChip {
  if (IsAskGeminiChipIgnoreCriteria()) {
    return YES;
  }

  return _tracker->ShouldTriggerHelpUI(
      feature_engagement::kIPHiOSGeminiContextualCueChip);
}

// Returns whether the promo timers exist which implies a promo is in the
// process of being displayed.
- (BOOL)arePromoTimersRunning {
  return _promoStartTimer != nullptr || _promoEndTimer != nullptr;
}

#pragma mark - Private ContextualPanelEntrypoint

// Updates the entrypoint state whenever the active tab changes or new data is
// provided.
- (void)activeTabHasNewData:(ContextualPanelItemConfiguration*)config {
  if ([self arePromoTimersRunning]) {
    return;
  }

  [self resetTimersAndUIStateAnimated:NO];

  if (!config) {
    [self.consumer hideBadge];
    return;
  }

  // Prevents entrypoint from showing while the Gemini promo is showing.
  if (IsPageActionMenuEnabled()) {
    BwgTabHelper* BWGTabHelper =
        BwgTabHelper::FromWebState(_webStateList->GetActiveWebState());
    if (BWGTabHelper) {
      if (BWGTabHelper->ShouldPreventContextualPanelEntryPoint()) {
        [self.consumer hideBadge];
        return;
      }
    }
  }

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>& metricsData =
      contextualPanelTabHelper->GetMetricsData();

  if (!metricsData) {
    ContextualPanelTabHelper::EntrypointMetricsData metricData;
    metricData.entrypoint_item_type = config->item_type;
    contextualPanelTabHelper->SetMetricsData(metricData);
  } else if (!metricsData->appearance_time) {
    metricsData->appearance_time = base::Time::Now();
  }

  LocationBarBadgeConfiguration* badgeConfig =
      [self createConfigFromContextualPanelEntrypointConfig:config];
  [self updateBadgeConfig:badgeConfig];
  [self.consumer setContextualPanelItemType:config->item_type];
  [self.consumer
      transitionToContextualPanelOpenedState:
          contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()];
}

// Whether to show the Contextual Panel Entrypoint IPH given a `config`.
- (BOOL)canShowEntrypointIPHWithConfig:
    (ContextualPanelItemConfiguration*)config {
  return [self canShowLoudEntrypointMoment] && config &&
         config->CanShowEntrypointIPH() &&
         _tracker->WouldTriggerHelpUI(*config->iph_feature);
}

// Whether to show a loud contextual panel entrypoint moment.
- (BOOL)canShowLoudEntrypointMoment {
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  return !_infobarBadgesCurrentlyShown &&
         !contextualPanelTabHelper->IsContextualPanelCurrentlyOpened() &&
         !contextualPanelTabHelper->WasLoudMomentEntrypointShown() &&
         !contextualPanelTabHelper->WasLoudMomentEntrypointCanceled() &&
         [self.delegate canShowChip:self];
}

// Creates a `LocationBarBadgeConfiguration` from
// a `ContextualPanelItemConfiguration`.
- (LocationBarBadgeConfiguration*)
    createConfigFromContextualPanelEntrypointConfig:
        (ContextualPanelItemConfiguration*)config {
  if (!config) {
    return nil;
  }

  LocationBarBadgeType badgeType;
  switch (config->item_type) {
    case ContextualPanelItemType::SamplePanelItem:
      badgeType = LocationBarBadgeType::kContextualPanelEntryPointSample;
      break;
    case ContextualPanelItemType::PriceInsightsItem:
      badgeType = LocationBarBadgeType::kPriceInsights;
      break;
    case ContextualPanelItemType::ReaderModeItem:
      badgeType = LocationBarBadgeType::kReaderMode;
      break;
  }

  RecordLocationBarBadgeUpdate(badgeType);
  // TODO(crbug.com/448422022): Store Contextual Panel Entrypoint badges
  // instead of preventing them.
  if ([self.consumer isBadgeVisible]) {
    return nil;
  }

  NSString* accessibilityLabel =
      base::SysUTF8ToNSString(config->accessibility_label);

  UIImage* image;
  CGFloat symbolPointSize = kBadgeSymbolPointSize;
  switch (config->image_type) {
    case ContextualPanelItemConfiguration::EntrypointImageType::SFSymbol:
      image = DefaultSymbolWithPointSize(
          base::SysUTF8ToNSString(config->entrypoint_image_name),
          symbolPointSize);
      break;
    case ContextualPanelItemConfiguration::EntrypointImageType::Image:
      image = CustomSymbolWithPointSize(
          base::SysUTF8ToNSString(config->entrypoint_image_name),
          symbolPointSize);
      break;
  }

  LocationBarBadgeConfiguration* badgeConfig =
      [[LocationBarBadgeConfiguration alloc]
           initWithBadgeType:badgeType
          accessibilityLabel:accessibilityLabel
                  badgeImage:image];
  badgeConfig.badgeText = base::SysUTF8ToNSString(config->entrypoint_message);

  if (config->accessibility_hint.size() > 0) {
    badgeConfig.accessibilityHint =
        base::SysUTF8ToNSString(config->accessibility_hint);
  }
  return badgeConfig;
}

// Tries to show the entrypoint's IPH with the config text, and returns whether
// it was shown successfully. Also passes the current config's entrypoint FET
// feature, which controls whether the IPH can be shown.
- (BOOL)attemptShowingEntrypointIPHWithText:(NSString*)text
                                     config:(ContextualPanelItemConfiguration*)
                                                config {
  BOOL isBottomOmnibox = [self.delegate isBottomOmniboxActive];

  CGPoint anchorPoint =
      [self.delegate helpAnchorUsingBottomOmnibox:isBottomOmnibox];

  BOOL shown = [_entrypointHelpHandler
      showContextualPanelEntrypointIPHWithConfig:config
                                     anchorPoint:anchorPoint
                                 isBottomOmnibox:isBottomOmnibox];

  return shown;
}

// Whether a large contextual panel entrypoint moment can show.
- (BOOL)canShowLargeEntrypointWithConfig:
    (ContextualPanelItemConfiguration*)config {
  return [self canShowLoudEntrypointMoment] && config &&
         config->CanShowLargeEntrypoint();
}

- (void)contextualPanelEntrypointBadgeTapped {
  // Cancel any pending transition timers since user interacted with entrypoint.
  [self resetTimersAndUIStateAnimated:YES];

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  ContextualPanelItemConfiguration* config =
      contextualPanelTabHelper->GetFirstCachedConfig().get();
  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>& metricsData =
      contextualPanelTabHelper->GetMetricsData();

  if (contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()) {
    [LocationBarBadgeMetrics
        logContextualPanelEntrypointDismissMetrics:metricsData];
    [_contextualSheetHandler closeContextualSheet];
  } else {
    [LocationBarBadgeMetrics
        logFirstTapMetricsContextualPanelEntrypointMetrics:metricsData];
    if (!config || !config->entrypoint_custom_action) {
      // The contextual panel should not be opened if there is a primary item
      // with a custom action.
      [_contextualSheetHandler openContextualSheet];
    }
  }

  if (config && config->entrypoint_custom_action) {
    // Regardless of whether the contextual panel is opened or closed, if the
    // primary item has a custom action, then it should be triggered when upon
    // being tapped.
    config->entrypoint_custom_action.Run();
  }

  if (!config || config->iph_entrypoint_used_event_name.empty()) {
    return;
  }
  _tracker->NotifyEvent(config->iph_entrypoint_used_event_name);
}

- (void)cancelContextualPanelEntrypointLoudMoment {
  [self resetTimersAndUIStateAnimated:YES];
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  contextualPanelTabHelper->SetLoudMomentEntrypointCanceled(true);
}

@end

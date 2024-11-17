// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_type.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_entrypoint_iph_commands.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@interface ContextualPanelEntrypointMediator () <
    ContextualPanelTabHelperObserving,
    InfobarBadgeTabHelperObserving,
    WebStateListObserving>
@end

@implementation ContextualPanelEntrypointMediator {
  // Whether there currently are any Infobar badges being shown.
  BOOL _infobarBadgesCurrentlyShown;

  // The command handler for contextual sheet commands.
  __weak id<ContextualSheetCommands> _contextualSheetHandler;

  // The command handler for entrypoint in-product help commands.
  __weak id<ContextualPanelEntrypointIPHCommands> _entrypointHelpHandler;

  // The engagement tracker for the current browser.
  raw_ptr<feature_engagement::Tracker> _engagementTracker;

  // WebStateList to use for observing ContextualPanelTabHelper events.
  raw_ptr<WebStateList> _webStateList;

  // Timer keeping track of when to transition to a loud moment for the
  // entrypoint (large entrypoint or IPH shown).
  std::unique_ptr<base::OneShotTimer> _transitionToEntrypointLoudMomentTimer;

  // Timer to keep track of when to return to the normal small entrypoint after
  // having transitioned to a loud moment.
  std::unique_ptr<base::OneShotTimer> _transitionToDefaultEntrypointTimer;

  // Observer machinery for the web state list.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Bridge for the ContextualPanelTabHelper observation.
  std::unique_ptr<ContextualPanelTabHelperObserverBridge>
      _contextualPanelObserverBridge;

  // Bridge for the InfobarBadgeTabHelper observation.
  std::unique_ptr<InfobarBadgeTabHelperObserverBridge>
      _infobarBadgeObserverBridge;
  std::unique_ptr<base::ScopedObservation<InfobarBadgeTabHelper,
                                          InfobarBadgeTabHelperObserverBridge>>
      _infobarBadgeObservation;

  // Forwarder to always be observing the active ContextualPanelTabHelper.
  std::unique_ptr<ActiveContextualPanelTabHelperObservationForwarder>
      _activeContextualPanelObservationForwarder;
}

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
         engagementTracker:(feature_engagement::Tracker*)engagementTracker
    contextualSheetHandler:(id<ContextualSheetCommands>)contextualSheetHandler
     entrypointHelpHandler:
         (id<ContextualPanelEntrypointIPHCommands>)entrypointHelpHandler {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _contextualSheetHandler = contextualSheetHandler;
    _entrypointHelpHandler = entrypointHelpHandler;
    _engagementTracker = engagementTracker;

    // Set up web state list observation.
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateListObservation = std::make_unique<
        base::ScopedObservation<WebStateList, WebStateListObserverBridge>>(
        _webStateListObserver.get());
    _webStateListObservation->Observe(_webStateList);

    // Set up active ContextualPanelTabHelper observation.
    _contextualPanelObserverBridge =
        std::make_unique<ContextualPanelTabHelperObserverBridge>(self);
    _activeContextualPanelObservationForwarder =
        std::make_unique<ActiveContextualPanelTabHelperObservationForwarder>(
            webStateList, _contextualPanelObserverBridge.get());

    // Setup InfobarBadgeTabHelper observation.
    _infobarBadgeObserverBridge =
        std::make_unique<InfobarBadgeTabHelperObserverBridge>(self);
    _infobarBadgeObservation = std::make_unique<base::ScopedObservation<
        InfobarBadgeTabHelper, InfobarBadgeTabHelperObserverBridge>>(
        _infobarBadgeObserverBridge.get());

    if (_webStateList->GetActiveWebState()) {
      _infobarBadgeObservation->Observe(
          InfobarBadgeTabHelper::GetOrCreateForWebState(
              _webStateList->GetActiveWebState()));
    }
  }
  return self;
}

- (void)disconnect {
  _infobarBadgeObservation->Reset();
  _infobarBadgeObservation.reset();
  _infobarBadgeObserverBridge.reset();
  _activeContextualPanelObservationForwarder.reset();
  _contextualPanelObserverBridge.reset();
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _webStateList = nullptr;
}

#pragma mark - ContextualPanelEntrypointMutator

- (void)dismissIPHAnimated:(BOOL)animated {
  [self dismissEntrypointIPHAnimated:animated];
}

- (void)entrypointTapped {
  // Cancel any pending transition timers since user interacted with entrypoint.
  [self resetTimersAndUIStateAnimated:YES];

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  if (contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()) {
    base::UmaHistogramEnumeration(
        "IOS.ContextualPanel.DismissedReason",
        ContextualPanelDismissedReason::UserDismissed);
    [_contextualSheetHandler closeContextualSheet];
  } else {
    [self logEntrypointFirstTapMetrics];
    [_contextualSheetHandler openContextualSheet];
  }

  base::WeakPtr<ContextualPanelItemConfiguration> config =
      contextualPanelTabHelper->GetFirstCachedConfig();
  if (!config || config->iph_entrypoint_used_event_name.empty()) {
    return;
  }
  _engagementTracker->NotifyEvent(config->iph_entrypoint_used_event_name);
}

- (void)setLocationBarLabelCenteredBetweenContent:(BOOL)centered {
  [self.delegate setLocationBarLabelCenteredBetweenContent:self
                                                  centered:centered];
}

#pragma mark - ContextualPanelTabHelperObserving

- (void)contextualPanel:(ContextualPanelTabHelper*)tabHelper
             hasNewData:
                 (std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>)
                     item_configurations {
  [self activeTabHasNewData:item_configurations.empty()
                                ? nullptr
                                : item_configurations[0]];
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

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // Return early if the active web state is the same as before the change.
  if (!status.active_web_state_change()) {
    return;
  }

  // De-register observer bridge for the old WebState's InfobarBadgeTabHelper.
  _infobarBadgeObservation->Reset();

  if (status.old_active_web_state) {
    // Update old active web state's visible time.
    ContextualPanelTabHelper* contextualPanelTabHelper =
        ContextualPanelTabHelper::FromWebState(status.old_active_web_state);
    std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
        metricsData = contextualPanelTabHelper->GetMetricsData();
    if (metricsData && metricsData->appearance_time) {
      metricsData->time_visible +=
          base::Time::Now() - metricsData->appearance_time.value();
      metricsData->appearance_time = std::nullopt;
    }
  }

  [self resetTimersAndUIStateAnimated:NO];

  // Return early if no new webstates are active.
  if (!status.new_active_web_state) {
    return;
  }

  // Register observer bridge for the new WebState's InfobarBadgeTabHelper.
  _infobarBadgeObservation->Observe(
      InfobarBadgeTabHelper::GetOrCreateForWebState(
          status.new_active_web_state));

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(status.new_active_web_state);
  [self activeTabHasNewData:contextualPanelTabHelper->GetFirstCachedConfig()];
}

#pragma mark - InfobarBadgeTabHelperObserving

- (void)infobarBadgesUpdated:(InfobarBadgeTabHelper*)tabHelper {
  // Return early if the notification doesn't come from the currently active
  // webstate's tab helper.
  raw_ptr<web::WebState> active_web_state = _webStateList->GetActiveWebState();
  if (!active_web_state || active_web_state->IsBeingDestroyed()) {
    return;
  }
  if (tabHelper !=
      InfobarBadgeTabHelper::GetOrCreateForWebState(active_web_state)) {
    return;
  }

  size_t badgesCount = tabHelper->GetInfobarBadgesCount();

  BOOL infobarBadgesCurrentlyShown = badgesCount > 0;
  if (_infobarBadgesCurrentlyShown == infobarBadgesCurrentlyShown) {
    return;
  }
  _infobarBadgesCurrentlyShown = infobarBadgesCurrentlyShown;

  if (_infobarBadgesCurrentlyShown) {
    [self dismissEntrypointIPHAnimated:YES];
  }

  [self.consumer setInfobarBadgesCurrentlyShown:_infobarBadgesCurrentlyShown];
}

#pragma mark - private

// Cancels pending timers, dismisses any showing IPH and removes any active
// fullscreen disabler.
- (void)resetTimersAndUIStateAnimated:(BOOL)animated {
  _transitionToEntrypointLoudMomentTimer = nullptr;
  _transitionToDefaultEntrypointTimer = nullptr;
  [self dismissEntrypointIPHAnimated:animated];
  [self.delegate enableFullscreen];
}

// Updates the entrypoint state whenever the active tab changes or new data is
// provided.
- (void)activeTabHasNewData:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  [self resetTimersAndUIStateAnimated:NO];

  if (!config) {
    [self.consumer hideEntrypoint];
    return;
  }

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  if (![self metricsData]) {
    ContextualPanelTabHelper::EntrypointMetricsData metricsData;
    metricsData.entrypoint_item_type = config->item_type;
    contextualPanelTabHelper->SetMetricsData(metricsData);
  } else if (![self metricsData]->appearance_time) {
    [self metricsData]->appearance_time = base::Time::Now();
  }

  [self.consumer setEntrypointConfig:config];
  [self.consumer transitionToSmallEntrypoint];
  [self.consumer showEntrypoint];

  [self logEntrypointFirstDisplayMetrics];

  [self.consumer
      transitionToContextualPanelOpenedState:
          contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()];

  // Special case for first entrypoint appearances where an IPH is shown
  // instead of the large entrypoint. If showing the IPH fails, will fallback to
  // showing the large entrypoint.
  if ([self canShowEntrypointIPHWithConfig:config]) {
    [self startEntrypointIPHTimers];
    return;
  }

  if ([self canShowLargeEntrypointWithConfig:config]) {
    [self startLargeEntrypointTimers];
    return;
  }
}

// Changes the UI to the large entrypoint variation and starts the timers to
// transition back to the small variation.
- (void)setupAndTransitionToLargeEntrypoint {
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  base::WeakPtr<ContextualPanelItemConfiguration> config =
      contextualPanelTabHelper->GetFirstCachedConfig();

  if (![self canShowLargeEntrypointWithConfig:config] ||
      ![self.delegate canShowLargeContextualPanelEntrypoint:self]) {
    // Enable fullscreen in case it was disabled when trying to show the IPH.
    [self.delegate enableFullscreen];
    return;
  }

  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>& metricsData =
      [self metricsData];
  if (metricsData) {
    metricsData->largeEntrypointWasShown = true;
  }
  contextualPanelTabHelper->SetLoudMomentEntrypointShown(true);
  [self.delegate disableFullscreen];
  [self.consumer transitionToLargeEntrypoint];

  // Large entrypoint has been displayed so fire loud display metrics here.
  [self logEntrypointLoudDisplayMetrics];

  __weak ContextualPanelEntrypointMediator* weakSelf = self;

  _transitionToDefaultEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToDefaultEntrypointTimer->Start(
      FROM_HERE,
      base::Seconds(LargeContextualPanelEntrypointDisplayedInSeconds()),
      base::BindOnce(^{
        [weakSelf cleanupAndTransitionToSmallEntrypoint];
      }));
}

// Changes the UI to the small entrypoint variation.
- (void)cleanupAndTransitionToSmallEntrypoint {
  [self.consumer transitionToSmallEntrypoint];
  [self.delegate enableFullscreen];
}

- (void)startLargeEntrypointTimers {
  __weak ContextualPanelEntrypointMediator* weakSelf = self;
  _transitionToEntrypointLoudMomentTimer =
      std::make_unique<base::OneShotTimer>();
  _transitionToEntrypointLoudMomentTimer->Start(
      FROM_HERE, base::Seconds(LargeContextualPanelEntrypointDelayInSeconds()),
      base::BindOnce(^{
        [weakSelf setupAndTransitionToLargeEntrypoint];
      }));
}

- (void)setupAndShowEntrypointIPH {
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  base::WeakPtr<ContextualPanelItemConfiguration> config =
      contextualPanelTabHelper->GetFirstCachedConfig();

  // Show the large entrypoint instead if the IPH can't be shown.
  if (!config || ![self canShowEntrypointIPHWithConfig:config]) {
    [self setupAndTransitionToLargeEntrypoint];
    return;
  }

  [self.delegate disableFullscreen];
  NSString* text = base::SysUTF8ToNSString(config->entrypoint_message);

  // Try to show the entrypoint's IPH and capture the result.
  BOOL success = [self attemptShowingEntrypointIPHWithText:text config:config];

  // Show the large entrypoint if showing the IPH was not successful.
  if (!success) {
    [self setupAndTransitionToLargeEntrypoint];
    return;
  }

  [self.consumer setEntrypointColored:YES];

  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>& metricsData =
      [self metricsData];
  if (metricsData) {
    metricsData->iphWasShown = true;
  }

  contextualPanelTabHelper->SetLoudMomentEntrypointShown(true);

  // IPH was shown, so fire loud display metrics here.
  [self logEntrypointLoudDisplayMetrics];

  __weak ContextualPanelEntrypointMediator* weakSelf = self;
  _transitionToDefaultEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToDefaultEntrypointTimer->Start(
      FROM_HERE,
      base::Seconds(LargeContextualPanelEntrypointDisplayedInSeconds()),
      base::BindOnce(^{
        [weakSelf dismissEntrypointIPHAnimated:YES];
        [weakSelf.delegate enableFullscreen];
      }));
}

- (void)startEntrypointIPHTimers {
  __weak ContextualPanelEntrypointMediator* weakSelf = self;
  _transitionToEntrypointLoudMomentTimer =
      std::make_unique<base::OneShotTimer>();
  _transitionToEntrypointLoudMomentTimer->Start(
      FROM_HERE, base::Seconds(LargeContextualPanelEntrypointDelayInSeconds()),
      base::BindOnce(^{
        [weakSelf setupAndShowEntrypointIPH];
      }));
}

// Tries to show the entrypoint's IPH with the config text, and returns whether
// it was shown successfully. Also passes the current config's entrypoint FET
// feature, which controls whether the IPH can be shown.
- (BOOL)attemptShowingEntrypointIPHWithText:(NSString*)text
                                     config:
                                         (base::WeakPtr<
                                             ContextualPanelItemConfiguration>)
                                             config {
  BOOL isBottomOmnibox = [self.delegate isBottomOmniboxActive];

  CGPoint anchorPoint =
      [self.delegate helpAnchorUsingBottomOmnibox:isBottomOmnibox];

  BOOL shown = [_entrypointHelpHandler
      maybeShowContextualPanelEntrypointIPHWithConfig:config
                                          anchorPoint:anchorPoint
                                      isBottomOmnibox:isBottomOmnibox];

  return shown;
}

- (void)dismissEntrypointIPHAnimated:(BOOL)animated {
  [_entrypointHelpHandler dismissContextualPanelEntrypointIPHAnimated:animated];
  [self.consumer setEntrypointColored:NO];
}

- (BOOL)canShowLargeEntrypointWithConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  return [self canShowLoudEntrypointMoment] && config &&
         config->CanShowLargeEntrypoint();
}

- (BOOL)canShowEntrypointIPHWithConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  return [self canShowLoudEntrypointMoment] && config &&
         config->CanShowEntrypointIPH() &&
         _engagementTracker->WouldTriggerHelpUI(*config->iph_feature);
}

- (BOOL)canShowLoudEntrypointMoment {
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  return !_infobarBadgesCurrentlyShown &&
         !contextualPanelTabHelper->IsContextualPanelCurrentlyOpened() &&
         !contextualPanelTabHelper->WasLoudMomentEntrypointShown() &&
         [self.delegate canShowLargeContextualPanelEntrypoint:self];
}

- (std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&)metricsData {
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  return contextualPanelTabHelper->GetMetricsData();
}

#pragma mark - Metrics helpers

// Logs metrics that should be fired when the entrypoint is displayed for the
// first time.
- (void)logEntrypointFirstDisplayMetrics {
  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
      optionalMetricsData = [self metricsData];
  if (!optionalMetricsData ||
      optionalMetricsData->entrypoint_regular_display_metrics_fired) {
    return;
  }

  ContextualPanelTabHelper::EntrypointMetricsData& metricsData =
      optionalMetricsData.value();

  metricsData.entrypoint_regular_display_metrics_fired = true;

  base::UmaHistogramEnumeration("IOS.ContextualPanel.EntrypointDisplayed",
                                metricsData.entrypoint_item_type);

  std::string entrypointTypeHistogramName =
      "IOS.ContextualPanel.Entrypoint.Regular";
  base::UmaHistogramEnumeration(entrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);

  std::string blockTypeEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.Regular.%s",
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeEntrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);
}

// Log any metrics that should be logged when a loud entrypoint is displayed.
- (void)logEntrypointLoudDisplayMetrics {
  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
      optionalMetricsData = [self metricsData];
  if (!optionalMetricsData ||
      optionalMetricsData->entrypoint_loud_display_metrics_fired) {
    return;
  }

  ContextualPanelTabHelper::EntrypointMetricsData& metricsData =
      optionalMetricsData.value();

  std::string entrypointTypeString =
      [self loudEntrypointTypeStringForMetrics:metricsData];

  // Either the IPH or Large entrypoint should have been shown by now.
  if (entrypointTypeString == "") {
    return;
  }

  metricsData.entrypoint_loud_display_metrics_fired = true;

  std::string entrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s", entrypointTypeString.c_str());
  base::UmaHistogramEnumeration(entrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);

  std::string blockTypeEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s.%s", entrypointTypeString.c_str(),
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeEntrypointTypeHistogramName,
                                EntrypointInteractionType::Displayed);
}

// Logs any metrics fired the first time a given entrypoint is opened via
// tapping.
- (void)logEntrypointFirstTapMetrics {
  std::optional<ContextualPanelTabHelper::EntrypointMetricsData>&
      optionalMetricsData = [self metricsData];
  if (!optionalMetricsData ||
      optionalMetricsData->entrypoint_tap_metrics_fired) {
    return;
  }

  ContextualPanelTabHelper::EntrypointMetricsData& metricsData =
      optionalMetricsData.value();

  base::TimeDelta visibleTimeThisIteration =
      (metricsData.appearance_time)
          ? (base::Time::Now() - metricsData.appearance_time.value())
          : base::Seconds(0);
  base::TimeDelta visibleTime =
      metricsData.time_visible + visibleTimeThisIteration;

  metricsData.entrypoint_tap_metrics_fired = true;

  // Fire metrics saying the entrypoint was tapped.
  base::UmaHistogramEnumeration("IOS.ContextualPanel.EntrypointTapped",
                                metricsData.entrypoint_item_type);

  // Always fire the regular tap events because the regular display events are
  // also always fired.
  base::UmaHistogramEnumeration("IOS.ContextualPanel.Entrypoint.Regular",
                                EntrypointInteractionType::Tapped);

  std::string blockTypeEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.Regular.%s",
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeEntrypointTypeHistogramName,
                                EntrypointInteractionType::Tapped);

  // Fire metrics for the time to tap.
  base::UmaHistogramTimes(
      "IOS.ContextualPanel.Entrypoint.Regular.UptimeBeforeTap", visibleTime);

  std::string blockTypeEntrypointTypeUptimeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.Regular.%s.UptimeBeforeTap",
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramTimes(blockTypeEntrypointTypeUptimeHistogramName,
                          visibleTime);

  // Additionally fire metrics for the loud entrypoint variant, if one was
  // shown.
  std::string entrypointTypeString =
      [self loudEntrypointTypeStringForMetrics:metricsData];
  if (entrypointTypeString == "") {
    return;
  }
  std::string loudEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s", entrypointTypeString.c_str());
  base::UmaHistogramEnumeration(loudEntrypointTypeHistogramName,
                                EntrypointInteractionType::Tapped);

  std::string blockTypeLoudEntrypointTypeHistogramName = base::StringPrintf(
      "IOS.ContextualPanel.Entrypoint.%s.%s", entrypointTypeString.c_str(),
      StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramEnumeration(blockTypeLoudEntrypointTypeHistogramName,
                                EntrypointInteractionType::Tapped);

  // Time to tap metrics:
  std::string loudEntrypointTypeUptimeHistogramName =
      base::StringPrintf("IOS.ContextualPanel.Entrypoint.%s.UptimeBeforeTap",
                         entrypointTypeString.c_str());
  base::UmaHistogramTimes(loudEntrypointTypeUptimeHistogramName, visibleTime);

  std::string blockTypeLoudEntrypointTypeUptimeHistogramName =
      base::StringPrintf(
          "IOS.ContextualPanel.Entrypoint.%s.%s.UptimeBeforeTap",
          entrypointTypeString.c_str(),
          StringForItemType(metricsData.entrypoint_item_type).c_str());
  base::UmaHistogramTimes(blockTypeLoudEntrypointTypeUptimeHistogramName,
                          visibleTime);
}

// Which type of loud entrypoint was displayed to be used in metric names.
- (std::string)loudEntrypointTypeStringForMetrics:
    (ContextualPanelTabHelper::EntrypointMetricsData&)metricsData {
  if (metricsData.iphWasShown) {
    return "IPH";
  } else if (metricsData.largeEntrypointWasShown) {
    return "Large";
  } else {
    return "";
  }
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"

#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_consumer.h"
#import "ios/chrome/browser/contextual_panel/model/active_contextual_panel_tab_helper_observation_forwarder.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_item_configuration.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@interface ContextualPanelEntrypointMediator () <
    ContextualPanelTabHelperObserving,
    WebStateListObserving>
@end

@implementation ContextualPanelEntrypointMediator {
  // WebStateList to use for observing ContextualPanelTabHelper events.
  raw_ptr<WebStateList> _webStateList;

  // Timer keeping track of when to transition to a large entrypoint.
  std::unique_ptr<base::OneShotTimer> _transitionToLargeEntrypointTimer;

  // Timer to keep track of when to return to a small entrypoint after having
  // transitioned to a large entrypoint.
  std::unique_ptr<base::OneShotTimer> _transitionToSmallEntrypointTimer;

  // Observer machinery for the web state list.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  std::unique_ptr<
      base::ScopedObservation<WebStateList, WebStateListObserverBridge>>
      _webStateListObservation;

  // Bridge for the ContextualPanelTabHelper observation.
  std::unique_ptr<ContextualPanelTabHelperObserverBridge>
      _contextualPanelObserverBridge;

  // Forwarder to always be observing the active ContextualPanelTabHelper.
  std::unique_ptr<ActiveContextualPanelTabHelperObservationForwarder>
      _activeContextualPanelObservationForwarder;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _webStateList = webStateList;

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
  }
  return self;
}

- (void)disconnect {
  _activeContextualPanelObservationForwarder.reset();
  _contextualPanelObserverBridge.reset();
  _webStateListObservation.reset();
  _webStateListObserver.reset();
  _webStateList = nullptr;
}

#pragma mark - ContextualPanelEntrypointMutator

- (void)entrypointTapped {
  // Cancel any pending transition timers since user interacted with entrypoint.
  _transitionToLargeEntrypointTimer = nullptr;
  _transitionToSmallEntrypointTimer = nullptr;
  [self.delegate enableFullscreen];

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  if (contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()) {
    contextualPanelTabHelper->CloseContextualPanel();
  } else {
    contextualPanelTabHelper->OpenContextualPanel();
  }
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
  [self activeTabHasNewData:item_configurations];
}

- (void)contextualPanelTabHelperDestroyed:(ContextualPanelTabHelper*)tabHelper {
  [self activeTabHasNewData:{}];
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

  // Return early if no new webstates are active.
  if (!status.new_active_web_state) {
    return;
  }
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(status.new_active_web_state);
  [self activeTabHasNewData:contextualPanelTabHelper
                                ->GetCurrentCachedConfigurations()];
}

#pragma mark - private

// Updates the entrypoint state whenever the active tab changes or new data is
// provided.
- (void)activeTabHasNewData:
    (std::vector<base::WeakPtr<ContextualPanelItemConfiguration>>)
        item_configurations {
  _transitionToLargeEntrypointTimer = nullptr;
  _transitionToSmallEntrypointTimer = nullptr;

  [self.delegate enableFullscreen];

  if (item_configurations.empty()) {
    [self.consumer hideEntrypoint];
    return;
  }

  base::WeakPtr<ContextualPanelItemConfiguration> config =
      item_configurations[0];

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());

  [self.consumer setEntrypointConfig:config];
  [self.consumer transitionToSmallEntrypoint];
  [self.consumer showEntrypoint];

  [self.consumer
      transitionToContextualPanelOpenedState:
          contextualPanelTabHelper->IsContextualPanelCurrentlyOpened()];

  if (![self canShowLargeEntrypointWithConfig:config]) {
    return;
  }

  // Start timers since we can show the large entrypoint.
  __weak ContextualPanelEntrypointMediator* weakSelf = self;

  _transitionToLargeEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToLargeEntrypointTimer->Start(
      FROM_HERE, base::Seconds(LargeContextualPanelEntrypointDelayInSeconds()),
      base::BindOnce(^{
        [weakSelf setupAndTransitionToLargeEntrypoint];
      }));
}

// Changes the UI to the large entrypoint variation and starts the timers to
// transition back to the small variation.
- (void)setupAndTransitionToLargeEntrypoint {
  if (![self.delegate canShowLargeContextualPanelEntrypoint:self]) {
    return;
  }

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  contextualPanelTabHelper->SetLargeEntrypointShown(true);
  [self.delegate disableFullscreen];
  [self.consumer transitionToLargeEntrypoint];

  __weak ContextualPanelEntrypointMediator* weakSelf = self;

  _transitionToSmallEntrypointTimer = std::make_unique<base::OneShotTimer>();
  _transitionToSmallEntrypointTimer->Start(
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

- (BOOL)canShowLargeEntrypointWithConfig:
    (base::WeakPtr<ContextualPanelItemConfiguration>)config {
  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(
          _webStateList->GetActiveWebState());
  return !contextualPanelTabHelper->IsContextualPanelCurrentlyOpened() &&
         !contextualPanelTabHelper->WasLargeEntrypointShown() &&
         !config->entrypoint_message.empty() &&
         config->relevance >= config->high_relevance &&
         [self.delegate canShowLargeContextualPanelEntrypoint:self];
}

@end

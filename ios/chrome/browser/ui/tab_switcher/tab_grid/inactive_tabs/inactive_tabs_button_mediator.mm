// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/scoped_observation.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"

using ScopedWebStateListObservation =
    base::ScopedObservation<WebStateList, WebStateListObserver>;

@interface InactiveTabsButtonMediator () <PrefObserverDelegate,
                                          WebStateListObserving>
@end

@implementation InactiveTabsButtonMediator {
  // The UI consumer to which updates are made.
  __weak id<InactiveTabsInfoConsumer> _consumer;
  // The list of inactive tabs.
  raw_ptr<WebStateList> _webStateList;
  // Observers of _webStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedWebStateListObservation> _scopedWebStateListObservation;
  // Preference service from the application context.
  raw_ptr<PrefService> _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithConsumer:(id<InactiveTabsInfoConsumer>)consumer
                    webStateList:(WebStateList*)webStateList
                     prefService:(PrefService*)prefService {
  CHECK(IsInactiveTabsAvailable());
  // TODO(crbug.com/40923937): Reinstate this CHECK once
  // InactiveTabsButtonMediator is not created when not needed (for example when
  // a policy disables the regular tab grid).
  // CHECK(consumer);
  CHECK(webStateList);
  CHECK(prefService);
  self = [super init];
  if (self) {
    _consumer = consumer;
    _webStateList = webStateList;

    // Observe the web state list.
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation =
        std::make_unique<ScopedWebStateListObservation>(
            _webStateListObserverBridge.get());
    _scopedWebStateListObservation->Observe(_webStateList);

    // Observe the preferences for changes to Inactive Tabs settings.
    _prefService = prefService;
    _prefChangeRegistrar.Init(_prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    // Register to observe any changes on pref backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);

    // Push the info to the consumer.
    [_consumer updateInactiveTabsCount:_webStateList->count()];
    NSInteger daysThreshold = InactiveTabsTimeThreshold().InDays();
    [_consumer updateInactiveTabsDaysThreshold:daysThreshold];
  }
  return self;
}

- (void)disconnect {
  _consumer = nil;
  _scopedWebStateListObservation.reset();
  _webStateListObserverBridge.reset();
  _webStateList = nullptr;
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nullptr;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    NSInteger daysThreshold =
        _prefService->GetInteger(prefs::kInactiveTabsTimeThreshold);
    [_consumer updateInactiveTabsDaysThreshold:daysThreshold];
  }
}

#pragma mark - WebStateListObserving

- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  // Do nothing. Updating the consumer with the new count will be handled in
  // didChangeWebStateList:change:status: with kDetach.
}

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Consumer will be updated at the end of the batch.
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when the status in WebStateList is updated.
      break;
    case WebStateListChange::Type::kDetach:
      [_consumer updateInactiveTabsCount:_webStateList->count()];
      break;
    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kReplace:
    case WebStateListChange::Type::kInsert:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      NOTREACHED();
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  // No-op. This is called when all inactive tabs are closed at once.
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  [_consumer updateInactiveTabsCount:_webStateList->count()];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  _scopedWebStateListObservation.reset();
  _webStateList = nullptr;
}

@end

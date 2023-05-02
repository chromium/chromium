// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_mediator.h"

#import "base/notreached.h"
#import "base/scoped_observation.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ScopedWebStateListObservation =
    base::ScopedObservation<WebStateList, WebStateListObserver>;

@interface InactiveTabsButtonMediator () <PrefObserverDelegate,
                                          WebStateListObserving>
@end

@implementation InactiveTabsButtonMediator {
  // The UI consumer to which updates are made.
  __weak id<InactiveTabsInfoConsumer> _consumer;
  // The list of inactive tabs.
  WebStateList* _webStateList;
  // Observers of _webStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedWebStateListObservation> _scopedWebStateListObservation;
  // Preference service from the application context.
  PrefService* _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

- (instancetype)initWithConsumer:(id<InactiveTabsInfoConsumer>)consumer
                    webStateList:(WebStateList*)webStateList
                     prefService:(PrefService*)prefService {
  CHECK(IsInactiveTabsAvailable());
  CHECK(consumer);
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

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  if (_webStateList->IsBatchInProgress()) {
    // Consumer will be updated at the end of the batch.
    return;
  }
  NOTREACHED();
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  NOTREACHED();
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  NOTREACHED();
}

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)index {
  // No-op. `-webStateList:didDetachWebState:atIndex` will soon be called and
  // will update the consumer with the new count.
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Consumer will be updated at the end of the batch.
    return;
  }
  [_consumer updateInactiveTabsCount:_webStateList->count()];
}

- (void)webStateList:(WebStateList*)webStateList
    willCloseWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           userAction:(BOOL)userAction {
  // No-op. Closed tabs have previously been detached, which means the count has
  // already been updated.
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  // No-op. This is called when the selected web state is moved (closed and
  // opened elsewhere) from inactive to active.
}

- (void)webStateList:(WebStateList*)webStateList
    didChangePinnedStateForWebState:(web::WebState*)webState
                            atIndex:(int)index {
  NOTREACHED();
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

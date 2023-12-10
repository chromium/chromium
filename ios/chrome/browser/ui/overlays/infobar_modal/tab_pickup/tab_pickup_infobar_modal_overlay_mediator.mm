// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_modal/tab_pickup/tab_pickup_infobar_modal_overlay_mediator.h"

#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/tabs/model/tab_pickup/features.h"
#import "ios/chrome/browser/ui/infobars/modals/tab_pickup/infobar_tab_pickup_consumer.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

@interface TabPickupInfobarModalOverlayMediator () <BooleanObserver,
                                                    SyncObserverModelBridge>

// The permissions modal config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation TabPickupInfobarModalOverlayMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // Sync service.
  syncer::SyncService* _syncService;
  // Observer for changes to the sync state.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;
  // Preference value for the tab pickup feature.
  PrefBackedBoolean* _tabPickupEnabledPref;
}

#pragma mark - Public

- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                 syncService:(syncer::SyncService*)syncService
                                    consumer:
                                        (id<InfobarTabPickupConsumer>)consumer
                                     request:(OverlayRequest*)request {
  self = [super initWithRequest:request];
  if (self) {
    CHECK(localPrefService);
    CHECK(syncService);
    CHECK(consumer);
    CHECK(IsTabPickupEnabled());
    _prefs = localPrefService;
    _syncService = syncService;
    _consumer = consumer;
    _syncObserverBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);

    _tabPickupEnabledPref = [[PrefBackedBoolean alloc]
        initWithPrefService:localPrefService
                   prefName:prefs::kTabPickupEnabled];
    _tabPickupEnabledPref.observer = self;

    [_consumer setTabPickupEnabled:_tabPickupEnabledPref.value];
    const bool tabSyncEnabled =
        _syncService->GetUserSettings()->GetSelectedTypes().Has(
            syncer::UserSelectableType::kTabs);
    [_consumer setTabSyncEnabled:tabSyncEnabled];
  }
  return self;
}

- (void)disconnect {
}

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarTabPickupTableViewControllerDelegate

// Sends the `enabled` state of the tab pickup feature to the model.
- (void)infobarTabPickupTableViewController:
            (InfobarTabPickupTableViewController*)
                infobarTabPickupTableViewController
                         didEnableTabPickup:(BOOL)enabled {
  _prefs->SetBoolean(prefs::kTabPickupEnabled, enabled);
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  const bool tabSyncEnabled =
      _syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs);
  [_consumer setTabSyncEnabled:tabSyncEnabled];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _tabPickupEnabledPref) {
    [_consumer setTabPickupEnabled:_tabPickupEnabledPref.value];
  }
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/set_up_list.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ntp/model/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_metrics.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"

using set_up_list_prefs::SetUpListItemState;

namespace {

bool GetIsItemComplete(SetUpListItemType type,
                       PrefService* prefs,
                       PrefService* local_state,
                       AuthenticationService* auth_service) {
  switch (type) {
    case SetUpListItemType::kSignInSync:
      return auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
    case SetUpListItemType::kDefaultBrowser:
      return IsChromeLikelyDefaultBrowser();
    case SetUpListItemType::kAutofill:
      return password_manager_util::IsCredentialProviderEnabledOnStartup(
          local_state);
    case SetUpListItemType::kNotifications: {
      id<SystemIdentity> identity =
          auth_service->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
      if (IsIOSTipsNotificationsEnabled()) {
        return push_notification_settings::
            IsMobileNotificationsEnabledForAnyClient(
                base::SysNSStringToUTF8(identity.gaiaID), prefs);
      } else {
        return push_notification_settings::
                   GetMobileNotificationPermissionStatusForClient(
                       PushNotificationClientId::kContent,
                       base::SysNSStringToUTF8(identity.gaiaID)) ||
               push_notification_settings::
                   GetMobileNotificationPermissionStatusForClient(
                       PushNotificationClientId::kSports,
                       base::SysNSStringToUTF8(identity.gaiaID));
      }
    }
    case SetUpListItemType::kFollow:
    case SetUpListItemType::kAllSet:
      NOTREACHED();
  }
}

SetUpListItem* BuildItem(SetUpListItemType type,
                         PrefService* prefs,
                         PrefService* local_state,
                         AuthenticationService* auth_service) {
  SetUpListItemState state = set_up_list_prefs::GetItemState(local_state, type);
  SetUpListItemState new_state = state;
  bool complete = false;
  switch (state) {
    case SetUpListItemState::kUnknown:
    case SetUpListItemState::kNotComplete:
      complete = GetIsItemComplete(type, prefs, local_state, auth_service);
      // If complete, mark it as "not in list" for next time, but add to list
      // this time.
      new_state = complete ? SetUpListItemState::kCompleteInList
                           : SetUpListItemState::kNotComplete;
      set_up_list_prefs::SetItemState(local_state, type, new_state);
      if (complete) {
        set_up_list_metrics::RecordItemCompleted(type);
      }
      return [[SetUpListItem alloc] initWithType:type complete:complete];
    case SetUpListItemState::kCompleteInList:
      return [[SetUpListItem alloc] initWithType:type complete:YES];
    case SetUpListItemState::kCompleteNotInList:
      return nil;
  }
}

// Adds the item to the array if it is not `nil`.
void AddItemIfNotNil(NSMutableArray* array, id item) {
  if (item) {
    [array addObject:item];
  }
}

// Returns true if signin is allowed / enabled.
bool IsSigninEnabled(AuthenticationService* auth_service) {
  switch (auth_service->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      return true;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      return false;
  }
}

// Returns `YES` if all items are complete.
BOOL AllItemsComplete(NSArray<SetUpListItem*>* items) {
  for (SetUpListItem* item in items) {
    if (!item.complete) {
      return NO;
    }
  }
  return YES;
}

}  // namespace

@interface SetUpList () <PrefObserverDelegate>
@end

@implementation SetUpList {
  // Local state prefs that store item state.
  raw_ptr<PrefService> _localState;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // YES if the Notification item should be included in `allItems`.
  BOOL _shouldIncludeNotificationItem;
}

+ (instancetype)buildFromPrefs:(PrefService*)prefs
                    localState:(PrefService*)localState
                   syncService:(syncer::SyncService*)syncService
         authenticationService:(AuthenticationService*)authService
    contentNotificationEnabled:(BOOL)isContentNotificationEnabled {
  if (IsHomeCustomizationEnabled() &&
      !prefs->GetBoolean(prefs::kHomeCustomizationMagicStackSetUpListEnabled)) {
    return nil;
  }

  if (!IsHomeCustomizationEnabled() &&
      set_up_list_prefs::IsSetUpListDisabled(localState)) {
    return nil;
  }

  NSMutableArray<SetUpListItem*>* items =
      [[NSMutableArray<SetUpListItem*> alloc] init];

  auto AddSignInItem = ^{
    if (IsSigninEnabled(authService) &&
        !syncService->HasDisableReason(
            syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) &&
        !HasManagedSyncDataType(syncService)) {
      AddItemIfNotNil(items, BuildItem(SetUpListItemType::kSignInSync, prefs,
                                       localState, authService));
    }
  };

  AddItemIfNotNil(items, BuildItem(SetUpListItemType::kDefaultBrowser, prefs,
                                   localState, authService));
  AddItemIfNotNil(items, BuildItem(SetUpListItemType::kAutofill, prefs,
                                   localState, authService));

  // Add notification item if any of the feature is enabled.
  if (IsIOSTipsNotificationsEnabled() || isContentNotificationEnabled) {
    AddItemIfNotNil(items, BuildItem(SetUpListItemType::kNotifications, prefs,
                                     localState, authService));
  }
  AddSignInItem();

  // Once all items are complete, set them to disappear from the list the next
  // time so that the list will be empty and the "All Set" item will not show.
  if (AllItemsComplete(items)) {
    for (SetUpListItem* item in items) {
      set_up_list_prefs::SetItemState(localState, item.type,
                                      SetUpListItemState::kCompleteNotInList);
    }
    set_up_list_prefs::MarkAllItemsComplete(localState);
  }

  // TODO(crbug.com/40262090): Add a Follow item to the Set Up List.
  return [[self alloc] initWithItems:items
                          localState:localState
               authenticationService:authService
          contentNotificationEnabled:isContentNotificationEnabled];
}

- (instancetype)initWithItems:(NSArray<SetUpListItem*>*)items
                    localState:(PrefService*)localState
         authenticationService:(AuthenticationService*)authService
    contentNotificationEnabled:(BOOL)isContentNotificationEnabled {
  self = [super init];
  if (self) {
    _items = items;
    _localState = localState;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(localState);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kSigninSyncItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kDefaultBrowserItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kAutofillItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kFollowItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kNotificationsItemState, &_prefChangeRegistrar);
    _shouldIncludeNotificationItem =
        IsIOSTipsNotificationsEnabled() || isContentNotificationEnabled;
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _localState = nullptr;
}

- (BOOL)allItemsComplete {
  return AllItemsComplete(self.items);
}

- (NSArray<SetUpListItem*>*)allItems {
  NSMutableArray* itemTypes = [[NSMutableArray alloc]
      initWithObjects:@(int(SetUpListItemType::kSignInSync)),
                      @(int(SetUpListItemType::kDefaultBrowser)),
                      @(int(SetUpListItemType::kAutofill)), nil];
  if (_shouldIncludeNotificationItem) {
    [itemTypes addObject:@(int(SetUpListItemType::kNotifications))];
  }
  for (SetUpListItem* item in _items) {
    [itemTypes removeObject:@(int(item.type))];
  }

  NSMutableArray* completeItems = [_items mutableCopy];
  for (NSNumber* typeNum in itemTypes) {
    [completeItems
        addObject:[[SetUpListItem alloc]
                      initWithType:(SetUpListItemType)[typeNum intValue]
                          complete:YES]];
  }
  return completeItems;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  SetUpListItem* item = [self itemForPrefName:preferenceName];
  if (!item) {
    return;
  }

  SetUpListItemState state =
      set_up_list_prefs::GetItemState(_localState, item.type);
  if (state == SetUpListItemState::kCompleteInList) {
    [item markComplete];
    BOOL allItemsComplete = [self allItemsComplete];
    [self.delegate setUpListItemDidComplete:item
                          allItemsCompleted:allItemsComplete];
    if (allItemsComplete) {
      set_up_list_prefs::MarkAllItemsComplete(_localState);
    }
  }
}

#pragma mark - Private

// Returns the item with a type that matches the given `prefName`.
- (SetUpListItem*)itemForPrefName:(const std::string&)preferenceName {
  for (SetUpListItem* item in self.items) {
    if (preferenceName == set_up_list_prefs::PrefNameForItem(item.type)) {
      return item;
    }
  }
  return nil;
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/set_up_list.h"

#import <vector>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/ntp/model/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_metrics.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/constants.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"

using set_up_list_prefs::SetUpListItemState;

namespace {

bool GetIsItemComplete(SetUpListItemType type,
                       PrefService* prefs,
                       PrefService* local_state,
                       signin::IdentityManager* identity_manager) {
  switch (type) {
    case SetUpListItemType::kDefaultBrowser:
      return IsChromeLikelyDefaultBrowser();
    case SetUpListItemType::kAutofill:
      return password_manager_util::IsCredentialProviderEnabledOnStartup(
          local_state);
    case SetUpListItemType::kNotifications: {
      UNAuthorizationStatus auth_status =
          [PushNotificationUtil getSavedPermissionSettings];
      if (auth_status == UNAuthorizationStatusNotDetermined ||
          auth_status == UNAuthorizationStatusProvisional) {
        return false;
      }
      CoreAccountInfo account = identity_manager->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
      return push_notification_settings::
          IsMobileNotificationsEnabledForAnyClient(account.gaia, prefs);
    }
    case SetUpListItemType::kAllSet:
      NOTREACHED();
  }
}

SetUpListItem* BuildItem(SetUpListItemType type,
                         PrefService* prefs,
                         PrefService* local_state,
                         signin::IdentityManager* identity_manager) {
  SetUpListItemState state = set_up_list_prefs::GetItemState(local_state, type);
  SetUpListItemState new_state = state;
  bool complete = false;
  switch (state) {
    case SetUpListItemState::kUnknown:
    case SetUpListItemState::kNotComplete:
      complete = GetIsItemComplete(type, prefs, local_state, identity_manager);
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

// Returns `YES` if all items are complete.
BOOL AllItemsComplete(NSArray<SetUpListItem*>* items) {
  for (SetUpListItem* item in items) {
    if (!item.complete) {
      return NO;
    }
  }
  return YES;
}

// Returns an ordered list of SetUpListItemType to show.
std::vector<SetUpListItemType> GetSetUpListItemTypeOrder() {
  std::vector<SetUpListItemType> items;
  items.push_back(SetUpListItemType::kDefaultBrowser);
  items.push_back(SetUpListItemType::kAutofill);
  items.push_back(SetUpListItemType::kNotifications);

  return items;
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
}

+ (instancetype)buildFromPrefs:(PrefService*)prefs
               identityManager:(signin::IdentityManager*)identityManager
                    localState:(PrefService*)localState {
  if (!prefs->GetBoolean(ntp_tiles::prefs::kTipsHomeModuleEnabled)) {
    return nil;
  }

  NSMutableArray<SetUpListItem*>* items = [NSMutableArray array];
  std::vector<SetUpListItemType> itemTypeOrder = GetSetUpListItemTypeOrder();
  for (SetUpListItemType itemType : itemTypeOrder) {
    AddItemIfNotNil(items,
                    BuildItem(itemType, prefs, localState, identityManager));
  }

  // Once all items are complete, set them to disappear from the list the next
  // time so that the list will be empty and the "All Set" item will not show.
  if (AllItemsComplete(items)) {
    for (SetUpListItem* item in items) {
      set_up_list_prefs::SetItemState(localState, item.type,
                                      SetUpListItemState::kCompleteNotInList);
    }
    set_up_list_prefs::MarkAllItemsComplete(localState);
  }

  return [[self alloc] initWithItems:items localState:localState];
}

- (instancetype)initWithItems:(NSArray<SetUpListItem*>*)items
                   localState:(PrefService*)localState {
  self = [super init];
  if (self) {
    _items = items;
    _localState = localState;
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefChangeRegistrar.Init(localState);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kDefaultBrowserItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kAutofillItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kNotificationsItemState, &_prefChangeRegistrar);
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
      initWithObjects:@(int(SetUpListItemType::kDefaultBrowser)),
                      @(int(SetUpListItemType::kAutofill)), nil];
  [itemTypes addObject:@(int(SetUpListItemType::kNotifications))];

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

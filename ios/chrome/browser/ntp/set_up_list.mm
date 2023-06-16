// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/set_up_list.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/ntp/set_up_list_delegate.h"
#import "ios/chrome/browser/ntp/set_up_list_item.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/set_up_list_metrics.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/signin/authentication_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using set_up_list_prefs::SetUpListItemState;

namespace {

bool GetIsItemComplete(SetUpListItemType type,
                       PrefService* prefs,
                       AuthenticationService* auth_service) {
  switch (type) {
    case SetUpListItemType::kSignInSync:
      return auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
    case SetUpListItemType::kDefaultBrowser:
      return IsChromeLikelyDefaultBrowser();
    case SetUpListItemType::kAutofill:
      return password_manager_util::IsCredentialProviderEnabledOnStartup(prefs);
    case SetUpListItemType::kFollow:
    case SetUpListItemType::kAllSet:
      NOTREACHED_NORETURN();
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
      complete = GetIsItemComplete(type, prefs, auth_service);
      // If complete, mark it as "not in list" for next time, but add to list
      // this time.
      new_state = complete ? SetUpListItemState::kCompleteNotInList
                           : SetUpListItemState::kNotComplete;
      set_up_list_prefs::SetItemState(local_state, type, new_state);
      if (complete) {
        set_up_list_metrics::RecordItemCompleted(type);
      }
      return [[SetUpListItem alloc] initWithType:type complete:complete];
    case SetUpListItemState::kCompleteInList:
      // Display in list this time, but remove from list next time.
      new_state = SetUpListItemState::kCompleteNotInList;
      set_up_list_prefs::SetItemState(local_state, type, new_state);
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

}  // namespace

@interface SetUpList () <PrefObserverDelegate>
@end

@implementation SetUpList {
  // Local state prefs that store item state.
  PrefService* _localState;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

+ (instancetype)buildFromPrefs:(PrefService*)prefs
                    localState:(PrefService*)localState
         authenticationService:(AuthenticationService*)authService {
  if (set_up_list_prefs::IsSetUpListDisabled(localState)) {
    return nil;
  }
  NSMutableArray<SetUpListItem*>* items =
      [[NSMutableArray<SetUpListItem*> alloc] init];
  if (IsSigninEnabled(authService)) {
    AddItemIfNotNil(items, BuildItem(SetUpListItemType::kSignInSync, prefs,
                                     localState, authService));
  }
  AddItemIfNotNil(items, BuildItem(SetUpListItemType::kDefaultBrowser, prefs,
                                   localState, authService));
  AddItemIfNotNil(items, BuildItem(SetUpListItemType::kAutofill, prefs,
                                   localState, authService));
  // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
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
        set_up_list_prefs::kSigninSyncItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kDefaultBrowserItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kAutofillItemState, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        set_up_list_prefs::kFollowItemState, &_prefChangeRegistrar);
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _localState = nullptr;
}

- (BOOL)allItemsComplete {
  for (SetUpListItem* item in _items) {
    if (!item.complete) {
      return NO;
    }
  }
  return YES;
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
    [self.delegate setUpListItemDidComplete:item];
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

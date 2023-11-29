// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_metrics.h"

namespace set_up_list_prefs {

const char kSigninSyncItemState[] = "set_up_list.signin_sync_item.state";
const char kDefaultBrowserItemState[] =
    "set_up_list.default_browser_item.state";
const char kAutofillItemState[] = "set_up_list.autofill_item.state";
const char kFollowItemState[] = "set_up_list.follow_item.state";
const char kContentNotificationItemState[] =
    "set_up_list.content_notification_item.state";
const char kDisabled[] = "set_up_list.disabled";
const char kLastInteraction[] = "set_up_list.last_interaction";

void RegisterPrefs(PrefRegistrySimple* registry) {
  int unknown = static_cast<int>(SetUpListItemState::kUnknown);
  registry->RegisterIntegerPref(kSigninSyncItemState, unknown);
  registry->RegisterIntegerPref(kDefaultBrowserItemState, unknown);
  registry->RegisterIntegerPref(kAutofillItemState, unknown);
  registry->RegisterIntegerPref(kFollowItemState, unknown);
  registry->RegisterIntegerPref(kContentNotificationItemState, unknown);
  registry->RegisterBooleanPref(kDisabled, false);
  registry->RegisterTimePref(kLastInteraction, base::Time());
}

const char* PrefNameForItem(SetUpListItemType type) {
  switch (type) {
    case SetUpListItemType::kSignInSync:
      return kSigninSyncItemState;
    case SetUpListItemType::kDefaultBrowser:
      return kDefaultBrowserItemState;
    case SetUpListItemType::kAutofill:
      return kAutofillItemState;
    case SetUpListItemType::kFollow:
      return kFollowItemState;
    case SetUpListItemType::kContentNotification:
      return kContentNotificationItemState;
    case SetUpListItemType::kAllSet:
      NOTREACHED_NORETURN();
  }
}

SetUpListItemState GetItemState(PrefService* prefs, SetUpListItemType type) {
  int value = prefs->GetInteger(PrefNameForItem(type));
  return static_cast<SetUpListItemState>(value);
}

void SetItemState(PrefService* prefs,
                  SetUpListItemType type,
                  SetUpListItemState state) {
  int value = static_cast<int>(state);
  prefs->SetInteger(PrefNameForItem(type), value);
}

void MarkItemComplete(PrefService* prefs, SetUpListItemType type) {
  // If it is already completed and already removed from list, skip setting.
  switch (GetItemState(prefs, type)) {
    case SetUpListItemState::kUnknown:
    case SetUpListItemState::kNotComplete:
      set_up_list_metrics::RecordItemCompleted(type);
      SetItemState(prefs, type, SetUpListItemState::kCompleteInList);
      break;
    case SetUpListItemState::kCompleteInList:
    case SetUpListItemState::kCompleteNotInList:
      // Already complete, so there is nothing to do.
      break;
  }
}

bool IsSetUpListDisabled(PrefService* prefs) {
  return prefs->GetBoolean(kDisabled);
}

void DisableSetUpList(PrefService* prefs) {
  prefs->SetBoolean(kDisabled, true);
}

void RecordInteraction(PrefService* prefs) {
  prefs->SetTime(kLastInteraction, base::Time::Now());
}

base::Time GetLastInteraction(PrefService* prefs) {
  return prefs->GetTime(kLastInteraction);
}

}  // namespace set_up_list_prefs

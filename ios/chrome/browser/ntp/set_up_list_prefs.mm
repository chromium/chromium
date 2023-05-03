// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/set_up_list_prefs.h"

#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace set_up_list_prefs {

const char kSigninSyncItemState[] = "set_up_list.signin_sync_item.state";
const char kDefaultBrowserItemState[] =
    "set_up_list.default_browser_item.state";
const char kAutofillItemState[] = "set_up_list.autofill_item.state";
const char kFollowItemState[] = "set_up_list.follow_item.state";

void RegisterPrefs(PrefRegistrySimple* registry) {
  int unknown = static_cast<int>(SetUpListItemState::kUnknown);
  registry->RegisterIntegerPref(kSigninSyncItemState, unknown);
  registry->RegisterIntegerPref(kDefaultBrowserItemState, unknown);
  registry->RegisterIntegerPref(kAutofillItemState, unknown);
  registry->RegisterIntegerPref(kFollowItemState, unknown);
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
  if (GetItemState(prefs, type) == SetUpListItemState::kCompleteNotInList) {
    return;
  }

  SetItemState(prefs, type, SetUpListItemState::kCompleteInList);
}

}  // namespace set_up_list_prefs

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_PREFS_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_PREFS_H_

namespace base {
class Time;
}
class PrefRegistrySimple;
class PrefService;
enum class SetUpListItemType;

namespace set_up_list_prefs {

// Prefs to store the state of each item in the list.
extern const char kSigninSyncItemState[];
extern const char kDefaultBrowserItemState[];
extern const char kAutofillItemState[];
extern const char kFollowItemState[];
extern const char kNotificationsItemState[];
extern const char kAllItemsComplete[];
extern const char kDisabled[];

// Possible values stored in prefs for each Set Up List item state.
enum class SetUpListItemState {
  // Default state.
  kUnknown,
  // The item has not been completed.
  kNotComplete,
  // The item is complete, but is still displayed in the list as completed.
  kCompleteInList,
  // The item is complete and no longer displayed in the list.
  kCompleteNotInList,
  // The max value.
  kMaxValue = kCompleteNotInList,
};

// Returns the pref name for the given Set Up List Item Type.
const char* PrefNameForItem(SetUpListItemType type);

// Registers the prefs associated with the Set Up List.
void RegisterPrefs(PrefRegistrySimple* registry);

// Get the item state from `prefs`.
SetUpListItemState GetItemState(PrefService* prefs, SetUpListItemType type);

// Set the item state to `prefs`.
void SetItemState(PrefService* prefs,
                  SetUpListItemType type,
                  SetUpListItemState state);

// Marks the item as completed.
void MarkItemComplete(PrefService* prefs, SetUpListItemType type);

// Records that all items are complete.
void MarkAllItemsComplete(PrefService* prefs);

// Returns true if all items are complete.
bool AllItemsComplete(PrefService* prefs);

// Returns `true` if the Set Up List has been disabled.
bool IsSetUpListDisabled(PrefService* prefs);

// Disables the SetUpList.
void DisableSetUpList(PrefService* prefs);

// Stores the current time as the "last interaction" time for SetUpList.
void RecordInteraction(PrefService* prefs);

// Returns the "last interaction" time for Set Up List.
base::Time GetLastInteraction(PrefService* prefs);

}  // namespace set_up_list_prefs

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_SET_UP_LIST_PREFS_H_

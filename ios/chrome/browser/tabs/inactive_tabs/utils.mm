// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/inactive_tabs/utils.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void MoveTab(WebStateList* source,
             int source_index,
             WebStateList* destination,
             int destination_index) {
  std::unique_ptr<web::WebState> removed_web_state =
      source->DetachWebStateAt(source_index);
  destination->InsertWebState(destination_index, std::move(removed_web_state),
                              WebStateList::InsertionFlags::INSERT_FORCE_INDEX,
                              WebStateOpener());
}

void MoveTabsFromActiveToInactive(Browser* active_browser,
                                  Browser* inactive_browser) {
  DCHECK(IsInactiveTabsEnabled());
  WebStateList* active_web_state_list = active_browser->GetWebStateList();
  WebStateList* inactive_web_state_list = inactive_browser->GetWebStateList();
  const base::TimeDelta inactivity_threshold = InactiveTabsTimeThreshold();

  for (int index = active_web_state_list->GetIndexOfFirstNonPinnedWebState();
       index < active_web_state_list->count();) {
    web::WebState* current_web_state =
        active_web_state_list->GetWebStateAt(index);
    const base::TimeDelta time_since_last_activation =
        base::Time::Now() - current_web_state->GetLastActiveTime();

    if (time_since_last_activation > inactivity_threshold) {
      MoveTab(active_web_state_list, index, inactive_web_state_list,
              inactive_web_state_list->count());
    } else {
      ++index;
    }
  }

  // Ensure to have an active web state so the save can be performed.
  // TODO(crbug.com/1264451): Remove this workaround when it will not be longer
  // required to have an active WebState in the WebStateList.
  if (inactive_web_state_list->count() > 0) {
    inactive_web_state_list->ActivateWebStateAt(
        inactive_web_state_list->count() - 1);
  }
}

void MoveTabsFromInactiveToActive(Browser* inactive_browser,
                                  Browser* active_browser) {
  DCHECK(IsInactiveTabsEnabled());
  WebStateList* active_web_state_list = active_browser->GetWebStateList();
  WebStateList* inactive_web_state_list = inactive_browser->GetWebStateList();
  const base::TimeDelta inactivity_threshold = InactiveTabsTimeThreshold();
  int removed_web_state_number = 0;

  for (int index = 0; index < inactive_web_state_list->count();) {
    web::WebState* current_web_state =
        inactive_web_state_list->GetWebStateAt(index);
    const base::TimeDelta time_since_last_activation =
        base::Time::Now() - current_web_state->GetLastActiveTime();

    if (time_since_last_activation < inactivity_threshold) {
      int insertion_index =
          active_web_state_list->GetIndexOfFirstNonPinnedWebState() +
          removed_web_state_number++;
      MoveTab(inactive_web_state_list, index, active_web_state_list,
              insertion_index);
    } else {
      ++index;
    }
  }
}

void RestoreAllInactiveTabs(Browser* inactive_browser,
                            Browser* active_browser) {
  DCHECK(!IsInactiveTabsEnabled());
  WebStateList* active_web_state_list = active_browser->GetWebStateList();
  WebStateList* inactive_web_state_list = inactive_browser->GetWebStateList();
  // Record the number of tabs restored from the inactive browser after Inactive
  // Tabs has been disabled.
  base::UmaHistogramCounts100("Tabs.RestoredFromInactiveCount",
                              inactive_web_state_list->count());
  for (int index = inactive_web_state_list->count() - 1; index >= 0; index--) {
    MoveTab(inactive_web_state_list, index, active_web_state_list,
            active_web_state_list->GetIndexOfFirstNonPinnedWebState());
  }
}

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/inactive_tabs/utils.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/main/browser_util.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns true if the given web state last is inactive determined by the given
// threshold.
bool IsInactive(const base::TimeDelta& threshold, web::WebState* web_state) {
  const base::TimeDelta time_since_last_activation =
      base::Time::Now() - web_state->GetLastActiveTime();
  if (threshold > base::Days(1)) {
    // Note: Even though a week is 7 days, the threshold value is returned with
    // one extra day in all cases (> instead of >= operator) as it matches the
    // user expectations in the following case:
    //
    //     The user opens a tab every Monday. Last Monday it was opened at
    //     10:05am. The tab should not immediately be considered inactive at
    //     10:06am today.
    //
    // The padding is here to encompass a flexibility of a day.
    return time_since_last_activation.InDays() > threshold.InDays();
  } else {
    // This is the demo mode. Compare the times with no one-day padding.
    // TODO(crbug.com/1412108): Remove this once the experimental flag is
    // removed.
    return time_since_last_activation > threshold;
  }
}

// Returns the set of stable identifiers from the web state list.
NSSet<NSString*>* StableIdentifiers(WebStateList* web_state_list) {
  NSMutableSet<NSString*>* stable_identifiers = [[NSMutableSet alloc] init];
  for (int index = 0; index < web_state_list->count(); index++) {
    NSString* stable_identifier =
        web_state_list->GetWebStateAt(index)->GetStableIdentifier();
    [stable_identifiers addObject:stable_identifier];
  }
  return stable_identifiers;
}

// Closes any tab from `losers_web_state_list` that is the duplicate of a tab
// in `keepers_web_state_list`.
// Returns the number of dropped duplicates.
int FilterCrossDuplicates(WebStateList* keepers_web_state_list,
                          WebStateList* losers_web_state_list) {
  NSSet<NSString*>* keepers_identifiers =
      StableIdentifiers(keepers_web_state_list);
  // Count the number of dropped tabs because they are duplicates, for
  // reporting.
  int duplicate_count = 0;
  for (int index = losers_web_state_list->count() - 1; index >= 0; --index) {
    web::WebState* web_state = losers_web_state_list->GetWebStateAt(index);
    if ([keepers_identifiers containsObject:web_state->GetStableIdentifier()]) {
      losers_web_state_list->CloseWebStateAt(index,
                                             WebStateList::CLOSE_NO_FLAGS);
      duplicate_count++;
    }
  }
  return duplicate_count;
}

// Manages batch migration for the active and inactive browser.
void PerformBatchMigration(
    Browser* active_browser,
    Browser* inactive_browser,
    base::OnceCallback<void(WebStateList*, WebStateList*)> migration) {
  __block base::OnceCallback<void(WebStateList*, WebStateList*)>
      web_state_lists_migrations = std::move(migration);
  active_browser->GetWebStateList()->PerformBatchOperation(
      base::BindOnce(^(WebStateList* active_web_state_list) {
        inactive_browser->GetWebStateList()->PerformBatchOperation(
            base::BindOnce(^(WebStateList* inactive_web_state_list) {
              // Perform the migration.
              std::move(web_state_lists_migrations)
                  .Run(active_web_state_list, inactive_web_state_list);
            }));
      }));
}

}  // namespace

void MoveTabsFromActiveToInactive(Browser* active_browser,
                                  Browser* inactive_browser) {
  CHECK(IsInactiveTabsEnabled());
  CHECK_NE(active_browser, inactive_browser);
  PerformBatchMigration(
      active_browser, inactive_browser,
      base::BindOnce(^(WebStateList* active_web_state_list,
                       WebStateList* inactive_web_state_list) {
        // Remove cross-duplicates in the active web state list.
        int duplicate_count = FilterCrossDuplicates(inactive_web_state_list,
                                                    active_web_state_list);
        base::UmaHistogramCounts100(
            "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive",
            duplicate_count);

        // Move all now-considered inactive tabs to the inactive web state list.
        const base::TimeDelta inactivity_threshold =
            InactiveTabsTimeThreshold();
        int removed_web_state_number = 0;
        for (int index = active_web_state_list->pinned_tabs_count();
             index < active_web_state_list->count() &&
             !IsInactiveTabsMoveNumberExceeded(removed_web_state_number);) {
          web::WebState* current_web_state =
              active_web_state_list->GetWebStateAt(index);
          if (!IsVisibleURLNewTabPage(current_web_state) &&
              IsInactive(inactivity_threshold, current_web_state)) {
            MoveTabFromBrowserToBrowser(active_browser, index, inactive_browser,
                                        inactive_web_state_list->count());
            removed_web_state_number++;
          } else {
            ++index;
          }
        }
      }));
}

void MoveTabsFromInactiveToActive(Browser* inactive_browser,
                                  Browser* active_browser) {
  CHECK(IsInactiveTabsEnabled());
  CHECK_NE(active_browser, inactive_browser);
  PerformBatchMigration(
      active_browser, inactive_browser,
      base::BindOnce(^(WebStateList* active_web_state_list,
                       WebStateList* inactive_web_state_list) {
        // Remove cross-duplicates in the inactive web state list.
        int duplicate_count = FilterCrossDuplicates(active_web_state_list,
                                                    inactive_web_state_list);
        base::UmaHistogramCounts100(
            "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive",
            duplicate_count);

        // Move all now-considered active tabs to the active web state list.
        const base::TimeDelta inactivity_threshold =
            InactiveTabsTimeThreshold();
        int removed_web_state_number = 0;
        for (int index = 0;
             index < inactive_web_state_list->count() &&
             !IsInactiveTabsMoveNumberExceeded(removed_web_state_number);) {
          if (!IsInactive(inactivity_threshold,
                          inactive_web_state_list->GetWebStateAt(index))) {
            int insertion_index = active_web_state_list->pinned_tabs_count() +
                                  removed_web_state_number++;
            MoveTabFromBrowserToBrowser(inactive_browser, index, active_browser,
                                        insertion_index);
          } else {
            ++index;
          }
        }
      }));
}

void RestoreAllInactiveTabs(Browser* inactive_browser,
                            Browser* active_browser) {
  CHECK(!IsInactiveTabsEnabled());
  CHECK_NE(active_browser, inactive_browser);
  // Record the number of tabs restored from the inactive browser after Inactive
  // Tabs has been disabled.
  base::UmaHistogramCounts100("Tabs.RestoredFromInactiveCount",
                              inactive_browser->GetWebStateList()->count());

  PerformBatchMigration(
      active_browser, inactive_browser,
      base::BindOnce(^(WebStateList* active_web_state_list,
                       WebStateList* inactive_web_state_list) {
        // Remove cross-duplicates in the inactive web state list.
        int duplicate_count = FilterCrossDuplicates(active_web_state_list,
                                                    inactive_web_state_list);
        base::UmaHistogramCounts100(
            "Tabs.DroppedDuplicatesCountOnRestoreAllInactive", duplicate_count);

        // Move all tabs to the active web state list.
        for (int index = inactive_web_state_list->count() - 1; index >= 0;
             index--) {
          MoveTabFromBrowserToBrowser(
              inactive_browser, index, active_browser,
              active_web_state_list->pinned_tabs_count());
        }
      }));
}

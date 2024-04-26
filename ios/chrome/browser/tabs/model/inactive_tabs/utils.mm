// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/inactive_tabs/utils.h"

#import "base/metrics/histogram_functions.h"
#import "base/ranges/algorithm.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns true if the given web state last is inactive determined by the given
// threshold.
bool IsInactive(base::TimeDelta threshold, web::WebState* web_state) {
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
    // TODO(crbug.com/40890696): Remove this once the experimental flag is
    // removed.
    return time_since_last_activation > threshold;
  }
}

// Policy used by `MoveTabsAccordingToPolicy(...)`.
struct MovePolicy {
  enum Policy {
    kAll,
    kActiveOnly,
    kInactiveOnly,
  };

  // Returns a policy requesting to moving all tabs.
  static MovePolicy All() { return MovePolicy{.policy = kAll}; }

  // Returns a policy requesting to move all active tabs with `threshold`.
  static MovePolicy ActiveOnly(base::TimeDelta threshold) {
    return MovePolicy{.policy = kActiveOnly, .threshold = threshold};
  }

  // Returns a policy requesting to move all inactive tabs with `threshold`.
  static MovePolicy InactiveOnly(base::TimeDelta threshold) {
    return MovePolicy{.policy = kInactiveOnly, .threshold = threshold};
  }

  // The policy controlling which tabs to move.
  const Policy policy;

  // The threshold used to decide whether a tab is inactive or not.
  const base::TimeDelta threshold;
};

// Moves tabs from `source_browser` to `target_browser` after removing the
// duplicates (if any, as determined by their unique identifiers) following
// `move_policy`. The histogram `histogram` is used to record the number of
// duplicates found.
void MoveTabsAccordingToPolicy(Browser* source_browser,
                               Browser* target_browser,
                               MovePolicy move_policy,
                               const char* histogram) {
  WebStateList* const source_list = source_browser->GetWebStateList();
  WebStateList* const target_list = target_browser->GetWebStateList();

  const int source_count = source_list->count();
  const int target_count = target_list->count();

  std::vector<web::WebStateID> target_ids;
  for (int index = 0; index < target_count; ++index) {
    web::WebState* web_state = target_list->GetWebStateAt(index);
    target_ids.push_back(web_state->GetUniqueIdentifier());
  }

  // Sort the vector of identifiers in order to use binary_search(...).
  std::sort(target_ids.begin(), target_ids.end());

  std::vector<int> indexes_closing;
  std::vector<int> indexes_moving;
  std::vector<int> indexes_moving_or_closing;
  for (int index = 0; index < source_count; ++index) {
    web::WebState* web_state = source_list->GetWebStateAt(index);
    const web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
    if (base::ranges::binary_search(target_ids, web_state_id)) {
      indexes_closing.push_back(index);
      indexes_moving_or_closing.push_back(index);
      continue;
    }

    if (move_policy.policy == MovePolicy::kAll) {
      indexes_moving.push_back(index);
      indexes_moving_or_closing.push_back(index);
      continue;
    }

    // Don't consider tabs presenting the NTP, pinned tabs or tabs in a group as
    // inactive.
    if (move_policy.policy == MovePolicy::kInactiveOnly) {
      if (IsVisibleURLNewTabPage(web_state)) {
        continue;
      }

      if (index < source_list->pinned_tabs_count()) {
        continue;
      }

      if (source_list->GetGroupOfWebStateAt(index)) {
        continue;
      }
    }

    const bool is_inactive = IsInactive(move_policy.threshold, web_state);
    if (is_inactive == (move_policy.policy == MovePolicy::kInactiveOnly)) {
      indexes_moving.push_back(index);
      indexes_moving_or_closing.push_back(index);
      continue;
    }
  }

  // Record the number of duplicates found.
  base::UmaHistogramCounts100(histogram, indexes_closing.size());

  // If there are no WebState to move or close, then there is nothing to do.
  if (indexes_moving_or_closing.empty()) {
    return;
  }

  // Start a batch operation on the two WebStateList at the same time.
  const auto source_lock = source_list->StartBatchOperation();
  const auto target_lock = target_list->StartBatchOperation();

  // Determine and activate the new active WebState before performing the
  // close and move operations. This will prevents over-realisation.
  OrderControllerSourceFromWebStateList order_controller_source(*source_list);
  OrderController order_controller(order_controller_source);

  const int new_active_index = order_controller.DetermineNewActiveIndex(
      source_list->active_index(),
      RemovingIndexes(std::move(indexes_moving_or_closing)));
  source_list->ActivateWebStateAt(new_active_index);

  // Determine the index at which tabs are inserted in `target_list`. When
  // moving inactive tabs, they are inserted at the end of the destination
  // but when moving active ones, they are moved after the pinned tabs.
  const int insertion_index = move_policy.policy == MovePolicy::kInactiveOnly
                                  ? target_count
                                  : target_list->pinned_tabs_count();

  // If the target list has no active WebState, mark the first tab moved out
  // of the source list as the active one during the insertion.
  int index_to_activate = WebStateList::kInvalidIndex;
  if (!target_list->GetActiveWebState()) {
    if (!indexes_moving.empty()) {
      index_to_activate = indexes_moving.front();
    }
  }

  // Perform the close and move operations by iterating backwards in the
  // WebStateList (this avoid having to update the indexes).
  for (int iter = 0; iter < source_count; ++iter) {
    const int index = source_count - iter - 1;
    if (base::ranges::binary_search(indexes_closing, index)) {
      source_list->CloseWebStateAt(index, WebStateList::CLOSE_NO_FLAGS);
      continue;
    }

    if (base::ranges::binary_search(indexes_moving, index)) {
      // Using `AtIndex` allows to insert all the moved tabs with a desired
      // location and `Activate` allows to activate the tab if needed (e.g. the
      // first moved tab from source when target has no active WebState).
      const WebStateList::InsertionParams params =
          WebStateList::InsertionParams::AtIndex(insertion_index)
              .Activate(index == index_to_activate);

      MoveTabFromBrowserToBrowser(source_browser, index, target_browser,
                                  params);
      continue;
    }
  }
}

}  // namespace

void MoveTabsFromActiveToInactive(Browser* active_browser,
                                  Browser* inactive_browser) {
  CHECK(IsInactiveTabsEnabled());
  CHECK_NE(active_browser, inactive_browser);

  MoveTabsAccordingToPolicy(
      active_browser, inactive_browser,
      MovePolicy::InactiveOnly(InactiveTabsTimeThreshold()),
      "Tabs.DroppedDuplicatesCountOnMigrateActiveToInactive");
}

void MoveTabsFromInactiveToActive(Browser* inactive_browser,
                                  Browser* active_browser) {
  CHECK(IsInactiveTabsEnabled());
  CHECK_NE(active_browser, inactive_browser);

  MoveTabsAccordingToPolicy(
      inactive_browser, active_browser,
      MovePolicy::ActiveOnly(InactiveTabsTimeThreshold()),
      "Tabs.DroppedDuplicatesCountOnMigrateInactiveToActive");
}

void RestoreAllInactiveTabs(Browser* inactive_browser,
                            Browser* active_browser) {
  CHECK(!IsInactiveTabsEnabled());
  CHECK_NE(active_browser, inactive_browser);

  // Record the number of tabs restored from the inactive browser after Inactive
  // Tabs has been disabled.
  base::UmaHistogramCounts100("Tabs.RestoredFromInactiveCount",
                              inactive_browser->GetWebStateList()->count());

  MoveTabsAccordingToPolicy(inactive_browser, active_browser, MovePolicy::All(),
                            "Tabs.DroppedDuplicatesCountOnRestoreAllInactive");
}

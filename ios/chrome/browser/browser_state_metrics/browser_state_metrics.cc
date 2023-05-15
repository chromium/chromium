// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state_metrics/browser_state_metrics.h"

#include <stddef.h>

#include "components/profile_metrics/browser_profile_type.h"
#include "components/profile_metrics/counts.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#include "ios/web/public/browser_state.h"

bool CountBrowserStateInformation(ios::ChromeBrowserStateManager* manager,
                                  profile_metrics::Counts* counts) {
  BrowserStateInfoCache* info_cache = manager->GetBrowserStateInfoCache();
  size_t number_of_browser_states = info_cache->GetNumberOfBrowserStates();
  counts->total = number_of_browser_states;

  // Ignore other metrics if we have no browser states.
  if (!number_of_browser_states)
    return false;

  for (size_t i = 0; i < number_of_browser_states; ++i) {
    if (info_cache->BrowserStateIsAuthenticatedAtIndex(i))
      counts->signedin++;
  }
  return true;
}

void LogNumberOfBrowserStates(ios::ChromeBrowserStateManager* manager) {
  profile_metrics::Counts counts;
  CountBrowserStateInformation(manager, &counts);
  profile_metrics::LogProfileMetricsCounts(counts);
}
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_COORDINATOR_QUICK_DELETE_UTIL_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_COORDINATOR_QUICK_DELETE_UTIL_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"

class TemplateURLService;

namespace browsing_data {
enum class TimePeriod;
}

namespace quick_delete_util {

// Default search engine states.
enum class DefaultSearchEngineState {
  // The default search engine is Google.
  kGoogle,
  // The default search engine is not Google.
  kNotGoogle,
  // The default search engine is null.
  kError,
};

// Returns the appropriate summary subtitle for the given counter result per
// data type on the browsing data page.
NSString* GetCounterTextFromResult(
    const browsing_data::BrowsingDataCounter::Result& result,
    browsing_data::TimePeriod time_range);

// Returns the user's default search engine state.
DefaultSearchEngineState GetDefaultSearchEngineState(
    TemplateURLService* template_url_service);

}  // namespace quick_delete_util

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_COORDINATOR_QUICK_DELETE_UTIL_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_UTIL_H_

#include "components/browsing_data/core/counters/browsing_data_counter.h"

namespace browsing_data {
enum class TimePeriod;
}

namespace quick_delete_util {

// Returns the appropriate summary subtitle for the given counter result per
// data type on the browsing data page.
NSString* GetCounterTextFromResult(
    const browsing_data::BrowsingDataCounter::Result& result,
    browsing_data::TimePeriod time_range);

}  // namespace quick_delete_util

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_UTIL_H_

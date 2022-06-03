// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"

namespace chrome_test_util {

// Clears browsing cache and returns whether clearing the history was
// successful or timed out.
bool RemoveBrowsingCache() WARN_UNUSED_RESULT;

// Clears browsing history and returns whether clearing the history was
// successful or timed out.
bool ClearBrowsingHistory() WARN_UNUSED_RESULT;

// Clears browsing data and returns whether clearing was successful or timed
// out.
// TODO(crbug.com/1016960): The method will time out if it's called from
// EarlGrey2 with |off_the_record| = true.
bool ClearAllBrowsingData(bool off_the_record) WARN_UNUSED_RESULT;

// Clears all the default WKWebsiteDataStore data including the WK back/forward
// cache.
bool ClearAllWebStateBrowsingData() WARN_UNUSED_RESULT;

// Clears user decisions cache and returns whether clearing was successful or
// timed out.
bool ClearCertificatePolicyCache(bool off_the_record) WARN_UNUSED_RESULT;

// Returns the number of entries in the history database. Returns -1 if there
// was an error.
int GetBrowsingHistoryEntryCount(NSError** error) WARN_UNUSED_RESULT;

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_

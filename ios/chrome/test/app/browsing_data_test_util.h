// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_

#import <Foundation/Foundation.h>

namespace chrome_test_util {

// Clears browsing cache and returns whether clearing the history was
// successful or timed out.
[[nodiscard]] bool RemoveBrowsingCache();

// Clears browsing history and returns whether clearing the history was
// successful or timed out.
[[nodiscard]] bool ClearBrowsingHistory();

// Clears cookies and site data and returns whether the operation was
// successful or timed out.
[[nodiscard]] bool ClearCookiesAndSiteData();

// Clears all the default WKWebsiteDataStore data including the WK back/forward
// cache.
// NOTE: This leaves objects inside //ios/web which manage JavaScriptFeatures in
// an unknown state, relaunch the app after calling to ensure Chrome functions
// correctly.
[[nodiscard]] bool ClearAllWebStateBrowsingData();

// Clears user decisions cache and returns whether clearing was successful or
// timed out.
[[nodiscard]] bool ClearCertificatePolicyCache(bool off_the_record);

// Returns the number of entries in the history database. Returns -1 if there
// was an error.
[[nodiscard]] int GetBrowsingHistoryEntryCount(NSError** error);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_BROWSING_DATA_TEST_UTIL_H_

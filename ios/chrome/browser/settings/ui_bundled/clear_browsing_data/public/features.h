// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

// Feature for the removal of passwords from the delete browsing data flow.
BASE_DECLARE_FEATURE(kPasswordRemovalFromDeleteBrowsingData);

// Whether the password removal from delete browsing data feature is enabled.
bool IsPasswordRemovalFromDeleteBrowsingDataEnabled();

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_PUBLIC_FEATURES_H_

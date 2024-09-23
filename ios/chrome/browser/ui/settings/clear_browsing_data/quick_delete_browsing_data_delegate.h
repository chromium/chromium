// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_DELEGATE_H_

// Delegate for `QuickDeleteBrowsingDataCoordinator`.
@protocol QuickDeleteBrowsingDataDelegate

// Method invoked when browsing data page should be stopped.
- (void)stopBrowsingDataPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_BROWSING_DATA_DELEGATE_H_

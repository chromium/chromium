// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_URL_LOADER_DELEGATE_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_URL_LOADER_DELEGATE_H_

class GURL;

// Delegate for new tab page to load urls.
@protocol NewTabPageURLLoaderDelegate

// Loads the tab in current tab.
- (void)loadURLInTab:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_URL_LOADER_DELEGATE_H_

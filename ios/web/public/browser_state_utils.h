// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_BROWSER_STATE_UTILS_H_
#define IOS_WEB_PUBLIC_BROWSER_STATE_UTILS_H_

@class WKWebsiteDataStore;

namespace web {

class BrowserState;

// Returns a data store associated with BrowserState.
WKWebsiteDataStore* GetDataStoreForBrowserState(BrowserState* browser_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_BROWSER_STATE_UTILS_H_

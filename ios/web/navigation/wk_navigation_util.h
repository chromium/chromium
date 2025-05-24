// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains utility functions for creating and managing magic URLs
// used to implement NavigationManagerImpl.
//
// A restore session URL is a specific local file that is used to inject history
// into a new web view. See ios/web/navigation/resources/restore_session.html.
//
// A placeholder navigation is an "about:blank" page loaded into the WKWebView
// that corresponds to Native View or WebUI URL. This navigation is inserted to
// generate a WKBackForwardListItem for the Native View or WebUI URL in the
// WebView so that the WKBackForwardList contains the full list of user-visible
// navigations. See "Handling App-specific URLs" section in
// go/bling-navigation-experiment for more details.

#ifndef IOS_WEB_NAVIGATION_WK_NAVIGATION_UTIL_H_
#define IOS_WEB_NAVIGATION_WK_NAVIGATION_UTIL_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

namespace web {

namespace wk_navigation_util {

// Session restoration algorithm has this limitation on maximum session size.
extern const int kMaxSessionSize;

// The "Referer" [sic] HTTP header.
extern NSString* const kReferrerHeaderName;

// Sets (offset, size) and returns an updated last committed index, so the final
// size is less or equal to kMaxSessionSize. If item_count is greater than
// kMaxSessionSize, then this function will trim navigation items, which are the
// furthest to `last_committed_item_index`.
int GetSafeItemRange(int last_committed_item_index,
                     int item_count,
                     int* offset,
                     int* size);

// Returns true if `url` is an app specific url or an about:// scheme
// non-placeholder url.
bool URLNeedsUserAgentType(const GURL& url);

}  // namespace wk_navigation_util
}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_NAVIGATION_UTIL_H_

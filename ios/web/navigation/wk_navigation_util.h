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
#include <memory>
#include <vector>

#include "url/gurl.h"

namespace web {

class NavigationItem;

namespace wk_navigation_util {

// Session restoration algorithm has this limitation on maximum session size.
extern const int kMaxSessionSize;

// URL fragment prefix used to encode the session history to inject in a
// restore_session.html URL.
extern const char kRestoreSessionSessionHashPrefix[];

// URL fragment prefix used to encode target URL in a restore_session.html URL.
extern const char kRestoreSessionTargetUrlHashPrefix[];

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

// Returns true if `url` is a placeholder URL or restore_session.html URL.
bool IsWKInternalUrl(const GURL& url);
bool IsWKInternalUrl(NSURL* url);

// Returns true if `url` is an app specific url or an about:// scheme
// non-placeholder url.
bool URLNeedsUserAgentType(const GURL& url);

// Returns a file:// URL that points to the magic restore_session.html file.
// This is used in unit tests.
GURL GetRestoreSessionBaseUrl();

// Creates a restore_session.html `url` with the provided session
// history encoded in the URL fragment, such that when this URL is loaded in the
// web view, recreates all the history entries in `items` and the current loaded
// item is the entry at `last_committed_item_index`.  Sets `first_index` to the
// new beginning of items.
void CreateRestoreSessionUrl(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items,
    GURL* url,
    int* first_index);

// Returns true if the base URL of `url` is restore_session.html.
bool IsRestoreSessionUrl(const GURL& url);
bool IsRestoreSessionUrl(NSURL* url);

// Extracts the URL encoded in the URL fragment of `restore_session_url` to
// `target_url` and returns true. If the URL fragment does not have a
// "targetUrl=" prefix, returns false.
bool ExtractTargetURL(const GURL& restore_session_url, GURL* target_url);

}  // namespace wk_navigation_util
}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WK_NAVIGATION_UTIL_H_

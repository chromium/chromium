// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_OFFLINE_URL_UTILS_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_OFFLINE_URL_UTILS_H_

#include "base/files/file_path.h"
#include "url/gurl.h"

namespace reading_list {

// Returns the offline URL for `entry_url`, the URL of the ReadingListEntry
// which must not be empty or invalid.
GURL OfflineURLForURL(const GURL& entry_url);

// Create a chrome://offline/ URL that embeds entry_url in a "reload"
// parameters.
GURL OfflineReloadURLForURL(const GURL& entry_url);

// If `offline_url` has a "entryURL" query params that is a URL, returns it.
// If not, return GURL::EmptyURL().
GURL EntryURLForOfflineURL(const GURL& offline_url);

// If `offline_url` has a "reload" query params that is a URL, returns it.
// If not, return GURL::EmptyURL().
GURL ReloadURLForOfflineURL(const GURL& offline_url);

// Returns whether the URL points to a chrome offline URL.
bool IsOfflineURL(const GURL& url);

// Returns whether the URL points to a chrome offline URL with entry data.
bool IsOfflineEntryURL(const GURL& url);

// Returns whether the URL points to a chrome offline URL with reload data.
bool IsOfflineReloadURL(const GURL& url);

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_OFFLINE_URL_UTILS_H_

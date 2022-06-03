// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_
#define IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_


#include "base/files/file_path.h"
#include "url/gurl.h"

namespace reading_list {

// The distilled URL chrome://offline/... that will load the file at |path|.
// |entry_url| is the URL of the ReadingListEntry.
// |virtual_url| is the URL to display in the omnibox. This can be different
// from |entry_url| if the distillation was done after a redirection.
// |distilled_path|, |entry_url| and |virtual_url| are required and must not be
// empty or invalid.
GURL OfflineURLForPath(const base::FilePath& distilled_path,
                       const GURL& entry_url,
                       const GURL& virtual_url);

// Create a chrome://offline/ URL that embeds entry_url in a "reload"
// parameters.
GURL OfflineReloadURLForURL(const GURL& entry_url);

// If |offline_url| has a "entryURL" query params that is a URL, returns it.
// If not, return GURL::EmptyURL().
GURL EntryURLForOfflineURL(const GURL& offline_url);

// If |offline_url| has a "virtualURL" query params that is a URL, returns it.
// If not, return GURL::EmptyURL().
GURL VirtualURLForOfflineURL(const GURL& offline_url);

// The file URL pointing to the local file to load to display |distilled_url|.
// If |resources_root_url| is not nullptr, it is set to a file URL to the
// directory conatining all the resources needed by |distilled_url|.
// |offline_path| is the root path to the directory containing offline files.
GURL FileURLForDistilledURL(const GURL& distilled_url,
                            const base::FilePath& offline_path,
                            GURL* resources_root_url);

// If |offline_url| has a "reload" query params that is a URL, returns it.
// If not, return GURL::EmptyURL().
GURL ReloadURLForOfflineURL(const GURL& offline_url);

// Returns whether the URL points to a chrome offline URL.
bool IsOfflineURL(const GURL& url);

// Returns whether the URL points to a chrome offline URL with entry data.
bool IsOfflineEntryURL(const GURL& url);

// Returns whether the URL points to a chrome offline URL with reload data.
bool IsOfflineReloadURL(const GURL& url);

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_

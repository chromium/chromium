// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_UTIL_H_
#define EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

#include <string>

#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class StoragePartition;
}

namespace extensions {
class Extension;
class ExtensionSet;

namespace util {

// TODO(benwells): Move functions from
// chrome/browser/extensions/extension_util.h/cc that are only dependent on
// extensions/ here.

// Returns true if the extension can be enabled in incognito mode.
bool CanBeIncognitoEnabled(const Extension* extension);

// Returns true if |extension_id| can run in an incognito window.
bool IsIncognitoEnabled(const std::string& extension_id,
                        content::BrowserContext* context);

// Returns true if |extension| can see events and data from another sub-profile
// (incognito to original profile, or vice versa).
bool CanCrossIncognito(const extensions::Extension* extension,
                       content::BrowserContext* context);

// Returns the site of the |extension_id|, given the associated |context|.
// Suitable for use with BrowserContext::GetStoragePartitionForSite().
GURL GetSiteForExtensionId(const std::string& extension_id,
                           content::BrowserContext* context);

content::StoragePartition* GetStoragePartitionForExtensionId(
    const std::string& extension_id,
    content::BrowserContext* browser_context);

// Maps a |file_url| to a |file_path| on the local filesystem, including
// resources in extensions. Returns true on success. See NaClBrowserDelegate for
// full details. If |use_blocking_api| is false, only a subset of URLs will be
// handled. If |use_blocking_api| is true, blocking file operations may be used,
// and this must be called on threads that allow blocking. Otherwise this can be
// called on any thread.
bool MapUrlToLocalFilePath(const ExtensionSet* extensions,
                           const GURL& file_url,
                           bool use_blocking_api,
                           base::FilePath* file_path);

}  // namespace util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

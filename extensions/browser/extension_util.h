// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_UTIL_H_
#define EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "extensions/common/manifest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
class StoragePartition;
class StoragePartitionConfig;
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

// Returns the StoragePartition domain for |extension|.
// Note: The reference returned has the same lifetime as |extension|.
const std::string& GetPartitionDomainForExtension(const Extension* extension);

// Returns an extension specific StoragePartitionConfig if the extension
// associated with |extension_id| has isolated storage.
// Otherwise, return the default StoragePartitionConfig.
content::StoragePartitionConfig GetStoragePartitionConfigForExtensionId(
    const std::string& extension_id,
    content::BrowserContext* browser_context);

content::StoragePartition* GetStoragePartitionForExtensionId(
    const std::string& extension_id,
    content::BrowserContext* browser_context,
    bool can_create = true);

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

// Returns true if the browser can potentially withhold permissions from the
// extension.
bool CanWithholdPermissionsFromExtension(const Extension& extension);
bool CanWithholdPermissionsFromExtension(
    const std::string& extension_id,
    const Manifest::Type type,
    const mojom::ManifestLocation location);

// Returns a unique int id for each context.
int GetBrowserContextId(content::BrowserContext* context);

// Calculates the allowlist and blocklist for |extension| and forwards the
// request to |browser_contexts|.
void SetCorsOriginAccessListForExtension(
    const std::vector<content::BrowserContext*>& browser_contexts,
    const Extension& extension,
    base::OnceClosure closure);

// Resets the allowlist and blocklist for |extension| to empty lists for
// |browser_context| and for all related regular+incognito contexts.
void ResetCorsOriginAccessListForExtension(
    content::BrowserContext* browser_context,
    const Extension& extension);

// Returns whether the |extension| should be loaded in the given
// |browser_context|.
bool IsExtensionVisibleToContext(const Extension& extension,
                                 content::BrowserContext* browser_context);

}  // namespace util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

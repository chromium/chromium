// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_UTIL_H_
#define EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace content {
class BrowserContext;
class ServiceWorkerContext;
class SiteInstance;
class StoragePartition;
class StoragePartitionConfig;
class RenderFrameHost;
}  // namespace content

namespace extensions {
class Extension;
class ExtensionSet;

namespace util {

// TODO(crbug.com/40893821): Move functions from
// chrome/browser/extensions/extension_util.h/cc that are only dependent on
// extensions/ here.

// Returns true if the extension can be enabled in incognito mode.
bool CanBeIncognitoEnabled(const Extension* extension);

// Returns true if |extension_id| can run in an incognito window.
bool IsIncognitoEnabled(const ExtensionId& extension_id,
                        content::BrowserContext* context);

// Returns true if |extension| can see events and data from another sub-profile
// (incognito to original profile, or vice versa).
bool CanCrossIncognito(const Extension* extension,
                       content::BrowserContext* context);

// Returns true if this extension can inject scripts into pages with file URLs.
bool AllowFileAccess(const ExtensionId& extension_id,
                     content::BrowserContext* context);

// Returns the StoragePartition domain for |extension|.
// Note: The reference returned has the same lifetime as |extension|.
const std::string& GetPartitionDomainForExtension(const Extension* extension);

// Returns an extension specific StoragePartitionConfig if the extension
// associated with |extension_id| has isolated storage.
// Otherwise, return the default StoragePartitionConfig.
content::StoragePartitionConfig GetStoragePartitionConfigForExtensionId(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context);

content::StoragePartition* GetStoragePartitionForExtensionId(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context,
    bool can_create = true);

// Returns the ServiceWorkerContext associated with the given `extension_id`.
content::ServiceWorkerContext* GetServiceWorkerContextForExtensionId(
    const ExtensionId& extension_id,
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

// Returns true if the browser can potentially withhold permissions from the
// extension.
bool CanWithholdPermissionsFromExtension(const Extension& extension);
bool CanWithholdPermissionsFromExtension(
    const ExtensionId& extension_id,
    const Manifest::Type type,
    const mojom::ManifestLocation location);

// Returns a unique int id for each context. Prefer using
// `BrowserContext::UniqueId()` directly.
// TODO(crbug.com/40267637):  Migrate callers to use the `context` unique id
// directly. For that we need to update all data keyed by integer context ids to
// be keyed by strings instead.
int GetBrowserContextId(content::BrowserContext* context);

// Returns whether the |extension| should be loaded in the given
// |browser_context|.
bool IsExtensionVisibleToContext(const Extension& extension,
                                 content::BrowserContext* browser_context);

// Initializes file scheme access if the extension has such permission.
void InitializeFileSchemeAccessForExtension(
    int render_process_id,
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context);

// Returns the default extension/app icon (for extensions or apps that don't
// have one).
const gfx::ImageSkia& GetDefaultExtensionIcon();
const gfx::ImageSkia& GetDefaultAppIcon();

// Gets the ExtensionId associated with the given `site_instance`.  An empty
// string is returned when `site_instance` is not associated with an extension.
ExtensionId GetExtensionIdForSiteInstance(content::SiteInstance& site_instance);

// Returns the extension id associated with the given `render_frame_host`, or
// the empty string if there is none.
std::string GetExtensionIdFromFrame(
    content::RenderFrameHost* render_frame_host);

// Returns true if the process corresponding to `render_process_id` can host an
// extension with `extension_id`.  (It doesn't necessarily mean that the process
// *does* host this specific extension at this point in time.) `is_sandboxed`
// specifies whether this is asking about a sandboxed extension document and is
// needed to accurately compute the expected extension origin for that case.
bool CanRendererHostExtensionOrigin(int render_process_id,
                                    const ExtensionId& extension_id,
                                    bool is_sandboxed);

// Returns `true` if `render_process_host` can legitimately claim to send IPC
// messages on behalf of `extension_id`.  `render_frame_host` parameter is
// needed to account for scenarios involving a Chrome Web Store frame.
bool CanRendererActOnBehalfOfExtension(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host,
    bool include_user_scripts);

// Returns true if the extension associated with `extension_id` is a Chrome App.
bool IsChromeApp(const ExtensionId& extension_id,
                 content::BrowserContext* context);

// Returns true if `extension_id` can be launched (possibly only after being
// enabled).
bool IsAppLaunchable(const ExtensionId& extension_id,
                     content::BrowserContext* context);

// Returns true if `extension_id` can be launched without being enabled first.
bool IsAppLaunchableWithoutEnabling(const ExtensionId& extension_id,
                                    content::BrowserContext* context);

}  // namespace util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_UTIL_H_

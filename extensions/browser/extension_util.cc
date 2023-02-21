// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "base/barrier_closure.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#endif

namespace extensions {
namespace util {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsSigninProfileTestExtensionOnTestImage(const Extension* extension) {
  if (extension->id() != extension_misc::kSigninProfileTestExtensionId)
    return false;
  base::SysInfo::CrashIfChromeOSNonTestImage();
  return true;
}
#endif

}  // namespace

bool CanBeIncognitoEnabled(const Extension* extension) {
  return IncognitoInfo::IsIncognitoAllowed(extension) &&
         (!extension->is_platform_app() ||
          extension->location() == mojom::ManifestLocation::kComponent);
}

bool IsIncognitoEnabled(const ExtensionId& extension_id,
                        content::BrowserContext* context) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::ENABLED);
  if (extension) {
    if (!CanBeIncognitoEnabled(extension))
      return false;
    // If this is an existing component extension we always allow it to
    // work in incognito mode.
    if (Manifest::IsComponentLocation(extension->location()))
      return true;
    if (extension->is_login_screen_extension())
      return true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (IsSigninProfileTestExtensionOnTestImage(extension))
      return true;
#endif
  }
  return ExtensionPrefs::Get(context)->IsIncognitoEnabled(extension_id);
}

bool CanCrossIncognito(const Extension* extension,
                       content::BrowserContext* context) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return IsIncognitoEnabled(extension->id(), context) &&
         !IncognitoInfo::IsSplitMode(extension);
}

const std::string& GetPartitionDomainForExtension(const Extension* extension) {
  // Extensions use their own ID for a partition domain.
  return extension->id();
}

content::StoragePartitionConfig GetStoragePartitionConfigForExtensionId(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  if (ExtensionsBrowserClient::Get()->HasIsolatedStorage(extension_id,
                                                         browser_context)) {
    // For extensions with isolated storage, the |extension_id| is
    // the |partition_domain|. The |in_memory| and |partition_name| are only
    // used in guest schemes so they are cleared here.
    return content::StoragePartitionConfig::Create(
        browser_context, extension_id, std::string() /* partition_name */,
        false /*in_memory */);
  }

  return content::StoragePartitionConfig::CreateDefault(browser_context);
}

content::StoragePartition* GetStoragePartitionForExtensionId(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context,
    bool can_create) {
  auto storage_partition_config =
      GetStoragePartitionConfigForExtensionId(extension_id, browser_context);
  content::StoragePartition* storage_partition =
      browser_context->GetStoragePartition(storage_partition_config,
                                           can_create);
  return storage_partition;
}

content::ServiceWorkerContext* GetServiceWorkerContextForExtensionId(
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  return GetStoragePartitionForExtensionId(extension_id, browser_context)
      ->GetServiceWorkerContext();
}

// This function is security sensitive. Bugs could cause problems that break
// restrictions on local file access or NaCl's validation caching. If you modify
// this function, please get a security review from a NaCl person.
bool MapUrlToLocalFilePath(const ExtensionSet* extensions,
                           const GURL& file_url,
                           bool use_blocking_api,
                           base::FilePath* file_path) {
  // Check that the URL is recognized by the extension system.
  const Extension* extension = extensions->GetExtensionOrAppByURL(file_url);
  if (!extension)
    return false;

  // This is a short-cut which avoids calling a blocking file operation
  // (GetFilePath()), so that this can be called on the non blocking threads. It
  // only handles a subset of the urls.
  if (!use_blocking_api) {
    if (file_url.SchemeIs(extensions::kExtensionScheme)) {
      std::string path = file_url.path();
      base::TrimString(path, "/", &path);  // Remove first slash
      *file_path = extension->path().AppendASCII(path);
      return true;
    }
    return false;
  }

  std::string path = file_url.path();
  ExtensionResource resource;

  if (SharedModuleInfo::IsImportedPath(path)) {
    // Check if this is a valid path that is imported for this extension.
    ExtensionId new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const Extension* new_extension = extensions->GetByID(new_extension_id);
    if (!new_extension)
      return false;

    if (!SharedModuleInfo::ImportsExtensionById(extension, new_extension_id))
      return false;

    resource = new_extension->GetResource(new_relative_path);
  } else {
    // Check that the URL references a resource in the extension.
    resource = extension->GetResource(path);
  }

  if (resource.empty())
    return false;

  // GetFilePath is a blocking function call.
  const base::FilePath resource_file_path = resource.GetFilePath();
  if (resource_file_path.empty())
    return false;

  *file_path = resource_file_path;
  return true;
}

bool CanWithholdPermissionsFromExtension(const Extension& extension) {
  return CanWithholdPermissionsFromExtension(
      extension.id(), extension.GetType(), extension.location());
}

bool CanWithholdPermissionsFromExtension(const ExtensionId& extension_id,
                                         Manifest::Type type,
                                         mojom::ManifestLocation location) {
  // Some extensions must retain privilege to all requested host permissions.
  // Specifically, extensions that don't show up in chrome:extensions (where
  // withheld permissions couldn't be granted), extensions that are part of
  // chrome or corporate policy, and extensions that are allowlisted to script
  // everywhere must always have permission to run on a page.
  return ui_util::ShouldDisplayInExtensionSettings(type, location) &&
         !Manifest::IsPolicyLocation(location) &&
         !Manifest::IsComponentLocation(location) &&
         !PermissionsData::CanExecuteScriptEverywhere(extension_id, location);
}

// The below functionality maps a context to a unique id by increasing a static
// counter.
int GetBrowserContextId(content::BrowserContext* context) {
  using ContextIdMap = std::map<content::BrowserContext*, int>;

  static int next_id = 0;
  static base::NoDestructor<ContextIdMap> context_map;

  // we need to get the original context to make sure we take the right context.
  content::BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetOriginalContext(context);
  auto iter = context_map->find(original_context);
  if (iter == context_map->end()) {
    iter =
        context_map->insert(std::make_pair(original_context, next_id++)).first;
  }
  DCHECK(iter->second != kUnspecifiedContextId);
  return iter->second;
}

// Returns whether the |extension| should be loaded in the given
// |browser_context|.
bool IsExtensionVisibleToContext(const Extension& extension,
                                 content::BrowserContext* browser_context) {
  // Renderers don't need to know about themes.
  if (extension.is_theme())
    return false;

  // Only extensions enabled in incognito mode should be loaded in an incognito
  // renderer. However extensions which can't be enabled in the incognito mode
  // (e.g. platform apps) should also be loaded in an incognito renderer to
  // ensure connections from incognito tabs to such extensions work.
  return !browser_context->IsOffTheRecord() ||
         !CanBeIncognitoEnabled(&extension) ||
         IsIncognitoEnabled(extension.id(), browser_context);
}

// Initializes file scheme access if the extension has such permission.
void InitializeFileSchemeAccessForExtension(
    int render_process_id,
    const std::string& extension_id,
    content::BrowserContext* browser_context) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  // TODO(karandeepb): This should probably use
  // extensions::util::AllowFileAccess.
  if (prefs->AllowFileAccess(extension_id)) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
        render_process_id, url::kFileScheme);
  }
}

ExtensionId GetExtensionIdForSiteInstance(
    content::SiteInstance& site_instance) {
  // <webview> guests always store the ExtensionId in the partition domain.
  if (site_instance.IsGuest())
    return site_instance.GetStoragePartitionConfig().partition_domain();

  // This works for both apps and extensions because the site has been
  // normalized to the extension URL for hosted apps.
  const GURL& site_url = site_instance.GetSiteURL();
  if (!site_url.SchemeIs(kExtensionScheme))
    return ExtensionId();

  // Navigating to a disabled (or uninstalled or not-yet-installed) extension
  // will set the site URL to chrome-extension://invalid.
  ExtensionId maybe_extension_id = site_url.host();
  if (maybe_extension_id == "invalid")
    return ExtensionId();

  // Otherwise,`site_url.host()` should always be a valid extension id.  In
  // particular, navigations should never commit a URL that uses a dynamic,
  // GUID-based hostname (such navigations should redirect to the statically
  // known, extension-id-based hostname).
  DCHECK(crx_file::id_util::IdIsValid(maybe_extension_id))
      << "; maybe_extension_id = " << maybe_extension_id;
  return maybe_extension_id;
}

std::string GetExtensionIdFromFrame(
    content::RenderFrameHost* render_frame_host) {
  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (!site.SchemeIs(kExtensionScheme))
    return std::string();

  return site.host();
}

bool CanRendererHostExtensionOrigin(int render_process_id,
                                    const ExtensionId& extension_id) {
  url::Origin extension_origin =
      Extension::CreateOriginFromExtensionId(extension_id);
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  return policy->CanAccessDataForOrigin(render_process_id, extension_origin);
}

bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context) {
  int reason = ExtensionPrefs::Get(context)->GetDisableReasons(extension_id);
  return !((reason & disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT) ||
           (reason & disable_reason::DISABLE_CORRUPTED));
}

bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
             extension_id, ExtensionRegistry::ENABLED) != nullptr;
}

}  // namespace util
}  // namespace extensions

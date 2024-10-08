// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_partition_config.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/system/sys_info.h"
#endif

namespace extensions {
namespace util {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
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
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  if (extension) {
    if (!CanBeIncognitoEnabled(extension))
      return false;
    // If this is an existing component extension we always allow it to
    // work in incognito mode.
    if (Manifest::IsComponentLocation(extension->location()))
      return true;
    if (extension->is_login_screen_extension())
      return true;
#if BUILDFLAG(IS_CHROMEOS)
    if (IsSigninProfileTestExtensionOnTestImage(extension))
      return true;
#endif
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsCaptivePortalPopupWindowEnabled()) {
    // An OTR Profile is used for captive portal signin to hide PII from
    // captive portals (which require HTTP redirects to function).
    // However, for captive portal signin we do not want want to disable
    // extensions by default. (Proxies are explicitly disabled elsewhere).
    // See b/261727502 for details.
    PrefService* prefs =
        ExtensionsBrowserClient::Get()->GetPrefServiceForContext(context);
    if (prefs) {
      const PrefService::Preference* captive_portal_pref =
          prefs->FindPreference(chromeos::prefs::kCaptivePortalSignin);
      if (captive_portal_pref && captive_portal_pref->GetValue()->GetBool()) {
        return true;
      }
    }
  }
#endif
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

bool AllowFileAccess(const ExtensionId& extension_id,
                     content::BrowserContext* context) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableExtensionsFileAccessCheck) ||
         ExtensionPrefs::Get(context)->AllowFileAccess(extension_id);
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
    if (file_url.SchemeIs(kExtensionScheme)) {
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

int GetBrowserContextId(content::BrowserContext* context) {
  using ContextIdMap = std::map<std::string, int>;

  static int next_id = 0;
  static base::NoDestructor<ContextIdMap> context_map;

  // we need to get the original context to make sure we take the right context.
  content::BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetOriginalContext(context);
  const std::string& context_id = original_context->UniqueId();
  auto iter = context_map->find(context_id);
  if (iter == context_map->end()) {
    iter = context_map->insert(std::make_pair(context_id, next_id++)).first;
  }
  DCHECK(iter->second != kUnspecifiedContextId);
  return iter->second;
}

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

void InitializeFileSchemeAccessForExtension(
    int render_process_id,
    const ExtensionId& extension_id,
    content::BrowserContext* browser_context) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  // TODO(karandeepb): This should probably use
  // extensions::util::AllowFileAccess.
  if (prefs->AllowFileAccess(extension_id)) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantRequestScheme(
        render_process_id, url::kFileScheme);
  }
}

const gfx::ImageSkia& GetDefaultAppIcon() {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_DEFAULT_ICON);
}

const gfx::ImageSkia& GetDefaultExtensionIcon() {
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_EXTENSION_DEFAULT_ICON);
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
                                    const ExtensionId& extension_id,
                                    bool is_sandboxed) {
  url::Origin extension_origin =
      Extension::CreateOriginFromExtensionId(extension_id);
  if (is_sandboxed) {
    // If the extension frame is sandboxed, the corresponding process is only
    // allowed to host opaque origins, per crbug.com/325410297. Therefore,
    // convert the origin into an opaque origin, and note that HostsOrigin()
    // will still validate the extension ID in the origin's precursor.
    extension_origin = extension_origin.DeriveNewOpaqueOrigin();
  }
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  return policy->HostsOrigin(render_process_id, extension_origin);
}

bool CanRendererActOnBehalfOfExtension(
    const ExtensionId& extension_id,
    content::RenderFrameHost* render_frame_host,
    content::RenderProcessHost& render_process_host,
    bool include_user_scripts) {
  // TODO(lukasza): Some of the checks below can be restricted to specific
  // context types (e.g. an empty `extension_id` should not happen in an
  // extension context;  and the SiteInstance-based check should only be needed
  // for hosted apps).  Consider leveraging ProcessMap::GetMostLikelyContextType
  // to implement this kind of restrictions.  Note that
  // ExtensionFunctionDispatcher::CreateExtensionFunction already calls
  // GetMostLikelyContextType - some refactoring might be needed to avoid
  // duplicating the work.

  // Allow empty extension id (it seems okay to assume that no
  // extension-specific special powers will be granted without an extension id).
  // For instance, WebUI pages may call private APIs like developerPrivate,
  // settingsPrivate, metricsPrivate, and others. In these cases, there is no
  // associated extension ID.
  //
  // TODO(lukasza): Investigate if the exception below can be avoided if
  // `render_process_host` hosts HTTP origins (i.e. if the exception can be
  // restricted to NTP, and/or chrome://... cases.
  if (extension_id.empty()) {
    return true;
  }

  // Did `render_process_id` run a content script or user script from
  // `extension_id`?
  // TODO(crbug.com/40055126): Ideally, we'd only check content script/
  // user script status if the renderer claimed to be acting on behalf of the
  // corresponding type (e.g. mojom::ContextType::kContentScript). We evaluate
  // this later in ProcessMap::CanProcessHostContextType(), but we could be
  // stricter by including it here.
  if (ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
          render_process_host, extension_id) ||
      (ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
           render_process_host, extension_id) &&
       include_user_scripts)) {
    return true;
  }

  // CanRendererHostExtensionOrigin() needs to know if the extension is
  // sandboxed, so check the sandbox flags if this request is for an extension
  // frame. Note that extension workers cannot be sandboxed since workers aren't
  // supported in opaque origins.
  bool is_sandboxed =
      render_frame_host &&
      render_frame_host->IsSandboxed(network::mojom::WebSandboxFlags::kOrigin);

  // Can `render_process_id` host a chrome-extension:// origin (frame, worker,
  // etc.)?
  if (CanRendererHostExtensionOrigin(render_process_host.GetID(), extension_id,
                                     is_sandboxed)) {
    return true;
  }

  if (render_frame_host) {
    DCHECK_EQ(render_process_host.GetID(),
              render_frame_host->GetProcess()->GetID());
    content::SiteInstance& site_instance =
        *render_frame_host->GetSiteInstance();

    // Chrome Extension APIs can be accessed from some hosted apps.
    //
    // Today this is mostly needed by the Chrome Web Store's hosted app, but the
    // code below doesn't make this assumption and allows *all* hosted apps
    // based on the trustworthy, Browser-side information from the SiteInstance
    // / SiteURL.  This way the code is resilient to future changes + there are
    // concerns that `chrome.test.sendMessage` might already be exposed to
    // hosted apps (but maybe not covered by tests).
    //
    // Note that the condition below allows all extensions (i.e. not just hosted
    // apps), but hosted apps aren't covered by the
    // `CanRendererHostExtensionOrigin` call above (because the process lock of
    // hosted apps is based on a https://, rather than chrome-extension:// url).
    //
    // GuestView is explicitly excluded, because we don't want to allow
    // GuestViews to spoof the extension id of their host.
    if (!site_instance.IsGuest() &&
        extension_id == util::GetExtensionIdForSiteInstance(site_instance)) {
      return true;
    }
  }

  // Disallow any other cases.
  return false;
}

bool IsChromeApp(const ExtensionId& extension_id,
                 content::BrowserContext* context) {
  const Extension* extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  return extension->is_platform_app();
}

bool IsAppLaunchable(const ExtensionId& extension_id,
                     content::BrowserContext* context) {
  int reason = ExtensionPrefs::Get(context)->GetDisableReasons(extension_id);
  return !((reason & disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT) ||
           (reason & disable_reason::DISABLE_CORRUPTED));
}

bool IsAppLaunchableWithoutEnabling(const ExtensionId& extension_id,
                                    content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->enabled_extensions().Contains(
      extension_id);
}

}  // namespace util
}  // namespace extensions

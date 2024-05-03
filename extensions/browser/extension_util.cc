// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_util.h"

#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "components/crx_file/id_util.h"
#include "components/guest_view/browser/guest_view_base.h"
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
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/mojom/host_id.mojom.h"
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

mojom::HostID::HostType HostIdTypeFromGuestView(
    const guest_view::GuestViewBase& guest) {
  if (guest.IsOwnedByWebUI()) {
    return mojom::HostID::HostType::kWebUi;
  }

  if (guest.IsOwnedByControlledFrameEmbedder()) {
    return mojom::HostID::HostType::kControlledFrameEmbedder;
  }

  // Note: We return a type of kExtensions for all cases where
  // |guest.IsOwnedByExtension()| are true, as well as some additional cases
  // where that call is false but also |guest.IsOwnedByWebUI()| and
  // |guest.IsOwnedByControlledFrameEmbedder()| are false. Those appear to be
  // when the provided extension identifier is blank. Future work in this area
  // could improve the checks here so all the cases are declared relative to
  // what the GuestView instance asserts itself to be.
  return mojom::HostID::HostType::kExtensions;
}

mojom::HostID GenerateHostIdFromGuestView(
    const guest_view::GuestViewBase& guest) {
  return mojom::HostID(HostIdTypeFromGuestView(guest), guest.owner_host());
}

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
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

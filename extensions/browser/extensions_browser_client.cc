// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_client.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/updater/scoped_extension_updater_keep_alive.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/permission_set.h"
#include "url/gurl.h"

namespace extensions {

namespace {

ExtensionsBrowserClient* g_extension_browser_client = nullptr;

}  // namespace

ExtensionsBrowserClient::ExtensionsBrowserClient() = default;
ExtensionsBrowserClient::~ExtensionsBrowserClient() = default;

ExtensionsBrowserClient* ExtensionsBrowserClient::Get() {
  return g_extension_browser_client;
}

void ExtensionsBrowserClient::Set(ExtensionsBrowserClient* client) {
  g_extension_browser_client = client;
}

void ExtensionsBrowserClient::RegisterExtensionFunctions(
    ExtensionFunctionRegistry* registry) {
  for (const auto& provider : providers_)
    provider->RegisterExtensionFunctions(registry);
}

void ExtensionsBrowserClient::AddAPIProvider(
    std::unique_ptr<ExtensionsBrowserAPIProvider> provider) {
  providers_.push_back(std::move(provider));
}

void ExtensionsBrowserClient::StartTearDown() {}

scoped_refptr<update_client::UpdateClient>
ExtensionsBrowserClient::CreateUpdateClient(
    scoped_refptr<update_client::Configurator> configurator) {
  return scoped_refptr<update_client::UpdateClient>(nullptr);
}

scoped_refptr<update_client::Configurator>
ExtensionsBrowserClient::CreateUpdateClientConfigurator(
    content::BrowserContext* context) {
  return scoped_refptr<update_client::Configurator>(nullptr);
}

std::unique_ptr<ScopedExtensionUpdaterKeepAlive>
ExtensionsBrowserClient::CreateUpdaterKeepAlive(
    content::BrowserContext* context) {
  return nullptr;
}

void ExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  LOG(ERROR) << error->GetDebugString();
}

bool ExtensionsBrowserClient::IsActivityLoggingEnabled(
    content::BrowserContext* context) {
  return false;
}

void ExtensionsBrowserClient::GetTabAndWindowIdForWebContents(
    content::WebContents* web_contents,
    int* tab_id,
    int* window_id) {
  *tab_id = -1;
  *window_id = -1;
}

bool ExtensionsBrowserClient::IsExtensionEnabled(
    const ExtensionId& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool ExtensionsBrowserClient::IsWebUIAllowedToMakeNetworkRequests(
    const url::Origin& origin) {
  return false;
}

network::mojom::NetworkContext*
ExtensionsBrowserClient::GetSystemNetworkContext() {
  return nullptr;
}

UserScriptListener* ExtensionsBrowserClient::GetUserScriptListener() {
  return nullptr;
}

void ExtensionsBrowserClient::SignalContentScriptsLoaded(
    content::BrowserContext* context) {}

bool ExtensionsBrowserClient::ShouldSchemeBypassNavigationChecks(
    const std::string& scheme) const {
  return false;
}

base::FilePath ExtensionsBrowserClient::GetSaveFilePath(
    content::BrowserContext* context) {
  return base::FilePath();
}

void ExtensionsBrowserClient::SetLastSaveFilePath(
    content::BrowserContext* context,
    const base::FilePath& path) {}

bool ExtensionsBrowserClient::HasIsolatedStorage(
    const ExtensionId& extension_id,
    content::BrowserContext* context) {
  return false;
}

bool ExtensionsBrowserClient::IsScreenshotRestricted(
    content::WebContents* web_contents) const {
  return false;
}

bool ExtensionsBrowserClient::IsValidTabId(
    content::BrowserContext* browser_context,
    int tab_id,
    bool include_incognito,
    content::WebContents** web_contents) const {
  return false;
}

ScriptExecutor* ExtensionsBrowserClient::GetScriptExecutorForTab(
    content::WebContents& web_contents) {
  return nullptr;
}

bool ExtensionsBrowserClient::IsUsbDeviceAllowedByPolicy(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    int vendor_id,
    int product_id) const {
  return false;
}

void ExtensionsBrowserClient::GetFavicon(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const GURL& url,
    base::CancelableTaskTracker* tracker,
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> bitmap_data)>
        callback) const {}

std::vector<content::BrowserContext*>
ExtensionsBrowserClient::GetRelatedContextsForExtension(
    content::BrowserContext* browser_context,
    const Extension& extension) const {
  return {browser_context};
}

void ExtensionsBrowserClient::AddAdditionalAllowedHosts(
    const PermissionSet& desired_permissions,
    PermissionSet* granted_permissions) const {}

void ExtensionsBrowserClient::AddAPIActionToActivityLog(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {}

void ExtensionsBrowserClient::AddEventToActivityLog(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const std::string& extra) {}

void ExtensionsBrowserClient::AddDOMActionToActivityLog(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& call_name,
    base::Value::List args,
    const GURL& url,
    const std::u16string& url_title,
    int call_type) {}

void ExtensionsBrowserClient::GetWebViewStoragePartitionConfig(
    content::BrowserContext* browser_context,
    content::SiteInstance* owner_site_instance,
    const std::string& partition_name,
    bool in_memory,
    base::OnceCallback<void(std::optional<content::StoragePartitionConfig>)>
        callback) {
  const GURL& owner_site_url = owner_site_instance->GetSiteURL();
  auto partition_config = content::StoragePartitionConfig::Create(
      browser_context, owner_site_url.GetHost(), partition_name, in_memory);

  if (owner_site_url.SchemeIs(extensions::kExtensionScheme)) {
    const auto& owner_config = owner_site_instance->GetStoragePartitionConfig();
#if DCHECK_IS_ON()
    if (browser_context->IsOffTheRecord()) {
      DCHECK(owner_config.in_memory());
    }
#endif
    if (!owner_config.is_default()) {
      partition_config.set_fallback_to_partition_domain_for_blob_urls(
          owner_config.in_memory()
              ? content::StoragePartitionConfig::FallbackMode::
                    kFallbackPartitionInMemory
              : content::StoragePartitionConfig::FallbackMode::
                    kFallbackPartitionOnDisk);
      DCHECK_EQ(owner_config,
                partition_config.GetFallbackForBlobUrls().value());
    }
  }
  std::move(callback).Run(partition_config);
}

media_device_salt::MediaDeviceSaltService*
ExtensionsBrowserClient::GetMediaDeviceSaltService(
    content::BrowserContext* context) {
  return nullptr;
}

bool ExtensionsBrowserClient::HasControlledFrameCapability(
    content::BrowserContext* context,
    const GURL& url) {
  return false;
}

custom_handlers::ProtocolHandlerRegistry*
ExtensionsBrowserClient::GetProtocolHandlerRegistry(
    content::BrowserContext* context) {
  return nullptr;
}

void ExtensionsBrowserClient::CheckManagementPolicy(
    content::BrowserContext* context) {}

scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
ExtensionsBrowserClient::GetSafeBrowsingDatabaseManager() const {
  return nullptr;
}

std::optional<safe_browsing::V4ProtocolConfig>
ExtensionsBrowserClient::GetV4ProtocolConfig() const {
  return std::nullopt;
}

void ExtensionsBrowserClient::OnActiveTabPermissionGranted(
    const Extension* extension,
    content::WebContents* web_contents) const {}

ExtensionManagementClient*
ExtensionsBrowserClient::GetExtensionManagementClient(
    content::BrowserContext* context) {
  return nullptr;
}

void ExtensionsBrowserClient::RunBlockActionsIfNeeded(
    const Extension* extension,
    content::WebContents* web_contents,
    SitePermissionsHelper* permission_helper,
    bool* reload_required) {}

void ExtensionsBrowserClient::ShowReloadBubbleForAllExtensions(
    const std::vector<const Extension*>& extensions,
    content::WebContents* web_contents) {}

bool ExtensionsBrowserClient::HasBeenBlocked(
    const Extension& extension,
    content::WebContents* web_contents) const {
  return false;
}

}  // namespace extensions

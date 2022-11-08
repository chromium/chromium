// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_client.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/updater/scoped_extension_updater_keep_alive.h"
#include "extensions/common/permissions/permission_set.h"

namespace extensions {

namespace {

ExtensionsBrowserClient* g_extension_browser_client = nullptr;

}  // namespace

ExtensionsBrowserClient::ExtensionsBrowserClient() {}
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

scoped_refptr<update_client::UpdateClient>
ExtensionsBrowserClient::CreateUpdateClient(content::BrowserContext* context) {
  return scoped_refptr<update_client::UpdateClient>(nullptr);
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
    const std::string& extension_id,
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

std::string ExtensionsBrowserClient::GetUserAgent() const {
  return std::string();
}

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
    const std::string& extension_id,
    content::BrowserContext* context) {
  return false;
}

bool ExtensionsBrowserClient::IsScreenshotRestricted(
    content::WebContents* web_contents) const {
  return false;
}

bool ExtensionsBrowserClient::IsValidTabId(content::BrowserContext* context,
                                           int tab_id) const {
  return false;
}

void ExtensionsBrowserClient::NotifyExtensionApiTabExecuteScript(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const std::string& code) const {}

bool ExtensionsBrowserClient::IsExtensionTelemetryServiceEnabled(
    content::BrowserContext* context) const {
  return false;
}

void ExtensionsBrowserClient::NotifyExtensionRemoteHostContacted(
    content::BrowserContext* context,
    const ExtensionId& extension_id,
    const GURL& url) const {}

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

}  // namespace extensions

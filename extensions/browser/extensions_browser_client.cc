// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extensions_browser_client.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "components/update_client/update_client.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_error.h"

namespace extensions {

namespace {

ExtensionsBrowserClient* g_extension_browser_client = NULL;

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

std::unique_ptr<content::BluetoothChooser>
ExtensionsBrowserClient::CreateBluetoothChooser(
    content::RenderFrameHost* frame,
    const content::BluetoothChooser::EventHandler& event_handler) {
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

const MediaRouterExtensionAccessLogger*
ExtensionsBrowserClient::GetMediaRouterAccessLogger() const {
  return nullptr;
}

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

}  // namespace extensions

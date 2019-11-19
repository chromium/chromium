// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extensions_api_client.h"

#include "base/logging.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

namespace extensions {
class AppViewGuestDelegate;

namespace {
ExtensionsAPIClient* g_instance = NULL;
}  // namespace

ExtensionsAPIClient::ExtensionsAPIClient() { g_instance = this; }

ExtensionsAPIClient::~ExtensionsAPIClient() { g_instance = NULL; }

// static
ExtensionsAPIClient* ExtensionsAPIClient::Get() { return g_instance; }

void ExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<ValueStoreFactory>& factory,
    const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
        observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {}

void ExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
}

bool ExtensionsAPIClient::ShouldHideResponseHeader(
    const GURL& url,
    const std::string& header_name) const {
  return false;
}

bool ExtensionsAPIClient::ShouldHideBrowserNetworkRequest(
    content::BrowserContext* context,
    const WebRequestInfo& request) const {
  return false;
}

void ExtensionsAPIClient::NotifyWebRequestWithheld(
    int render_process_id,
    int render_frame_id,
    const ExtensionId& extension_id) {}

void ExtensionsAPIClient::UpdateActionCount(content::BrowserContext* context,
                                            const ExtensionId& extension_id,
                                            int tab_id,
                                            int action_count,
                                            bool clear_badge_text) {}

void ExtensionsAPIClient::ClearActionCount(content::BrowserContext* context,
                                           const Extension& extension) {}

AppViewGuestDelegate* ExtensionsAPIClient::CreateAppViewGuestDelegate() const {
  return NULL;
}

ExtensionOptionsGuestDelegate*
ExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return NULL;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionsGuestViewManagerDelegate>(context);
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return std::unique_ptr<MimeHandlerViewGuestDelegate>();
}

WebViewGuestDelegate* ExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return NULL;
}

WebViewPermissionHelperDelegate* ExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new WebViewPermissionHelperDelegate(web_view_permission_helper);
}

scoped_refptr<ContentRulesRegistry>
ExtensionsAPIClient::CreateContentRulesRegistry(
    content::BrowserContext* browser_context,
    RulesCacheDelegate* cache_delegate) const {
  return scoped_refptr<ContentRulesRegistry>();
}

std::unique_ptr<DevicePermissionsPrompt>
ExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  return nullptr;
}

std::unique_ptr<VirtualKeyboardDelegate>
ExtensionsAPIClient::CreateVirtualKeyboardDelegate(
    content::BrowserContext* context) const {
  return nullptr;
}

ManagementAPIDelegate* ExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return nullptr;
}

std::unique_ptr<DisplayInfoProvider>
ExtensionsAPIClient::CreateDisplayInfoProvider() const {
  return nullptr;
}

MetricsPrivateDelegate* ExtensionsAPIClient::GetMetricsPrivateDelegate() {
  return nullptr;
}

NetworkingCastPrivateDelegate*
ExtensionsAPIClient::GetNetworkingCastPrivateDelegate() {
  return nullptr;
}

FileSystemDelegate* ExtensionsAPIClient::GetFileSystemDelegate() {
  return nullptr;
}

MessagingDelegate* ExtensionsAPIClient::GetMessagingDelegate() {
  return nullptr;
}

FeedbackPrivateDelegate* ExtensionsAPIClient::GetFeedbackPrivateDelegate() {
  return nullptr;
}

#if defined(OS_CHROMEOS)
NonNativeFileSystemDelegate*
ExtensionsAPIClient::GetNonNativeFileSystemDelegate() {
  return nullptr;
}

MediaPerceptionAPIDelegate*
ExtensionsAPIClient::GetMediaPerceptionAPIDelegate() {
  return nullptr;
}

void ExtensionsAPIClient::SaveImageDataToClipboard(
    const std::vector<char>& image_data,
    api::clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    const base::Closure& success_callback,
    const base::Callback<void(const std::string&)>& error_callback) {}
#endif

AutomationInternalApiDelegate*
ExtensionsAPIClient::GetAutomationInternalApiDelegate() {
  return nullptr;
}

std::vector<KeyedServiceBaseFactory*>
ExtensionsAPIClient::GetFactoryDependencies() {
  return {};
}

}  // namespace extensions

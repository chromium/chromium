// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extensions_api_client.h"

#include "build/build_config.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"
#endif

namespace extensions {
class AppViewGuestDelegate;

namespace {
ExtensionsAPIClient* g_instance = nullptr;
}  // namespace

ExtensionsAPIClient::ExtensionsAPIClient() { g_instance = this; }

ExtensionsAPIClient::~ExtensionsAPIClient() {
  g_instance = nullptr;
}

// static
ExtensionsAPIClient* ExtensionsAPIClient::Get() { return g_instance; }

void ExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<value_store::ValueStoreFactory>& factory,
    SettingsChangedCallback observer,
    std::map<settings_namespace::Namespace,
             raw_ptr<ValueStoreCache, CtnExperimental>>* caches) {}

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

void ExtensionsAPIClient::OpenFileUrl(
    const GURL& file_url,
    content::BrowserContext* browser_context) {}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
AppViewGuestDelegate* ExtensionsAPIClient::CreateAppViewGuestDelegate() const {
  return nullptr;
}

ExtensionOptionsGuestDelegate*
ExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return nullptr;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ExtensionsAPIClient::CreateGuestViewManagerDelegate() const {
  return std::make_unique<ExtensionsGuestViewManagerDelegate>();
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return nullptr;
}

WebViewGuestDelegate* ExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return nullptr;
}

WebViewPermissionHelperDelegate* ExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new WebViewPermissionHelperDelegate(web_view_permission_helper);
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ConsentProvider> ExtensionsAPIClient::CreateConsentProvider(
    content::BrowserContext* browser_context) const {
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

#if BUILDFLAG(IS_CHROMEOS)
bool ExtensionsAPIClient::ShouldAllowDetachingUsb(int vid, int pid) const {
  return false;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<VirtualKeyboardDelegate>
ExtensionsAPIClient::CreateVirtualKeyboardDelegate(
    content::BrowserContext* context) const {
  return nullptr;
}

ManagementAPIDelegate* ExtensionsAPIClient::CreateManagementAPIDelegate()
    const {
  return nullptr;
}

std::unique_ptr<SupervisedUserExtensionsDelegate>
ExtensionsAPIClient::CreateSupervisedUserExtensionsDelegate(
    content::BrowserContext* context) const {
  return nullptr;
}

std::unique_ptr<DisplayInfoProvider>
ExtensionsAPIClient::CreateDisplayInfoProvider() const {
  return nullptr;
}

MetricsPrivateDelegate* ExtensionsAPIClient::GetMetricsPrivateDelegate() {
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

#if BUILDFLAG(IS_CHROMEOS)
NonNativeFileSystemDelegate*
ExtensionsAPIClient::GetNonNativeFileSystemDelegate() {
  return nullptr;
}

MediaPerceptionAPIDelegate*
ExtensionsAPIClient::GetMediaPerceptionAPIDelegate() {
  return nullptr;
}

void ExtensionsAPIClient::SaveImageDataToClipboard(
    std::vector<uint8_t> image_data,
    api::clipboard::ImageType type,
    AdditionalDataItemList additional_items,
    base::OnceClosure success_callback,
    base::OnceCallback<void(const std::string&)> error_callback) {}
#endif  // BUILDFLAG(IS_CHROMEOS)

AutomationInternalApiDelegate*
ExtensionsAPIClient::GetAutomationInternalApiDelegate() {
  return nullptr;
}

std::vector<KeyedServiceBaseFactory*>
ExtensionsAPIClient::GetFactoryDependencies() {
  return {};
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/extensions_api_client.h"

#include "build/build_config.h"
#include "extensions/browser/api/messaging/native_message_host.h"

namespace extensions {

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
std::unique_ptr<AppViewGuestDelegate>
ExtensionsAPIClient::CreateAppViewGuestDelegate() const {
  return nullptr;
}

std::unique_ptr<ExtensionOptionsGuestDelegate>
ExtensionsAPIClient::CreateExtensionOptionsGuestDelegate(
    ExtensionOptionsGuest* guest) const {
  return nullptr;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ExtensionsAPIClient::CreateGuestViewManagerDelegate() const {
  return nullptr;
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
ExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return nullptr;
}

std::unique_ptr<WebViewGuestDelegate>
ExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return nullptr;
}

std::unique_ptr<WebViewPermissionHelperDelegate>
ExtensionsAPIClient::CreateWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper) const {
  return nullptr;
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
  return nullptr;
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

MessagingDelegate* ExtensionsAPIClient::GetMessagingDelegate() {
  return nullptr;
}

#if !BUILDFLAG(IS_ANDROID)
FileSystemDelegate* ExtensionsAPIClient::GetFileSystemDelegate() {
  return nullptr;
}

FeedbackPrivateDelegate* ExtensionsAPIClient::GetFeedbackPrivateDelegate() {
  return nullptr;
}

AutomationInternalApiDelegate*
ExtensionsAPIClient::GetAutomationInternalApiDelegate() {
  return nullptr;
}
#endif  // !BUILDFLAG(IS_ANDROID)

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

std::unique_ptr<NativeMessagePortDispatcher>
ExtensionsAPIClient::CreateNativeMessagePortDispatcher(
    std::unique_ptr<NativeMessageHost> host,
    base::WeakPtr<NativeMessagePort> port,
    scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner) {
  return nullptr;
}

std::vector<KeyedServiceBaseFactory*>
ExtensionsAPIClient::GetFactoryDependencies() {
  return {};
}

}  // namespace extensions

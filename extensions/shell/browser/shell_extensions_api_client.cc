// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_api_client.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/shell/browser/api/feedback_private/shell_feedback_private_delegate.h"
#include "extensions/shell/browser/delegates/shell_kiosk_delegate.h"
#include "extensions/shell/browser/shell_display_info_provider.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "extensions/shell/browser/shell_virtual_keyboard_delegate.h"

#if BUILDFLAG(IS_LINUX)
#include "extensions/shell/browser/api/file_system/shell_file_system_delegate.h"
#endif

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"
#include "extensions/shell/browser/shell_app_view_guest_delegate.h"
#include "extensions/shell/browser/shell_web_view_guest_delegate.h"
#endif

namespace extensions {

ShellExtensionsAPIClient::ShellExtensionsAPIClient() = default;

ShellExtensionsAPIClient::~ShellExtensionsAPIClient() = default;

void ShellExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  ShellExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

#if BUILDFLAG(ENABLE_GUEST_VIEW)
std::unique_ptr<AppViewGuestDelegate>
ShellExtensionsAPIClient::CreateAppViewGuestDelegate() const {
  return std::make_unique<ShellAppViewGuestDelegate>();
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
ShellExtensionsAPIClient::CreateGuestViewManagerDelegate() const {
  return std::make_unique<ExtensionsGuestViewManagerDelegate>();
}

std::unique_ptr<WebViewGuestDelegate>
ShellExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return std::make_unique<ShellWebViewGuestDelegate>();
}

std::unique_ptr<WebViewPermissionHelperDelegate>
ShellExtensionsAPIClient::CreateWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper) const {
  return std::make_unique<WebViewPermissionHelperDelegate>(
      web_view_permission_helper);
}
#endif  // BUILDFLAG(ENABLE_GUEST_VIEW)

std::unique_ptr<VirtualKeyboardDelegate>
ShellExtensionsAPIClient::CreateVirtualKeyboardDelegate(
    content::BrowserContext* browser_context) const {
  return std::make_unique<ShellVirtualKeyboardDelegate>();
}

std::unique_ptr<DisplayInfoProvider>
ShellExtensionsAPIClient::CreateDisplayInfoProvider() const {
  return std::make_unique<ShellDisplayInfoProvider>();
}

#if BUILDFLAG(IS_LINUX)
FileSystemDelegate* ShellExtensionsAPIClient::GetFileSystemDelegate() {
  if (!file_system_delegate_)
    file_system_delegate_ = std::make_unique<ShellFileSystemDelegate>();
  return file_system_delegate_.get();
}
#endif

MessagingDelegate* ShellExtensionsAPIClient::GetMessagingDelegate() {
  // The default implementation does nothing, which is fine.
  if (!messaging_delegate_)
    messaging_delegate_ = std::make_unique<MessagingDelegate>();
  return messaging_delegate_.get();
}

FeedbackPrivateDelegate*
ShellExtensionsAPIClient::GetFeedbackPrivateDelegate() {
  if (!feedback_private_delegate_) {
    feedback_private_delegate_ =
        std::make_unique<ShellFeedbackPrivateDelegate>();
  }
  return feedback_private_delegate_.get();
}

}  // namespace extensions

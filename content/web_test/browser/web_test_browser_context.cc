// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_browser_context.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"
#include "content/web_test/browser/web_test_background_fetch_delegate.h"
#include "content/web_test/browser/web_test_download_manager_delegate.h"
#include "content/web_test/browser/web_test_permission_manager.h"
#include "content/web_test/browser/web_test_push_messaging_service.h"
#include "content/web_test/browser/web_test_storage_access_manager.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/nix/xdg_util.h"
#elif BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "base/base_paths_mac.h"
#endif

namespace content {

WebTestBrowserContext::WebTestBrowserContext(bool off_the_record)
    : ShellBrowserContext(off_the_record) {
  // Configure the Geolocation API to provide no location by default.
  geolocation_overrider_ = std::make_unique<device::ScopedGeolocationOverrider>(
      /*position=*/nullptr);
}

WebTestBrowserContext::~WebTestBrowserContext() {
  NotifyWillBeDestroyed();
}

DownloadManagerDelegate* WebTestBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    download_manager_delegate_ =
        std::make_unique<WebTestDownloadManagerDelegate>();
    download_manager_delegate_->SetDownloadManager(GetDownloadManager());
    download_manager_delegate_->SetDownloadBehaviorForTesting(
        GetPath().Append(FILE_PATH_LITERAL("downloads")));
  }

  return download_manager_delegate_.get();
}

PlatformNotificationService*
WebTestBrowserContext::GetPlatformNotificationService() {
  if (!platform_notification_service_) {
    platform_notification_service_ =
        std::make_unique<MockPlatformNotificationService>(this);
  }
  return platform_notification_service_.get();
}

PushMessagingService* WebTestBrowserContext::GetPushMessagingService() {
  if (!push_messaging_service_)
    push_messaging_service_ = std::make_unique<WebTestPushMessagingService>();
  return push_messaging_service_.get();
}

PermissionControllerDelegate*
WebTestBrowserContext::GetPermissionControllerDelegate() {
  if (!permission_manager_.get())
    permission_manager_ = std::make_unique<WebTestPermissionManager>(*this);
  return permission_manager_.get();
}

BackgroundFetchDelegate* WebTestBrowserContext::GetBackgroundFetchDelegate() {
  if (!background_fetch_delegate_) {
    background_fetch_delegate_ =
        std::make_unique<WebTestBackgroundFetchDelegate>(this);
  }
  return background_fetch_delegate_.get();
}

BackgroundSyncController* WebTestBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_) {
    background_sync_controller_ =
        std::make_unique<MockBackgroundSyncController>();
  }
  return background_sync_controller_.get();
}

WebTestPermissionManager* WebTestBrowserContext::GetWebTestPermissionManager() {
  return static_cast<WebTestPermissionManager*>(
      GetPermissionControllerDelegate());
}

WebTestStorageAccessManager*
WebTestBrowserContext::GetWebTestStorageAccessManager() {
  if (!storage_access_.get())
    storage_access_ = std::make_unique<WebTestStorageAccessManager>(this);
  return storage_access_.get();
}

ClientHintsControllerDelegate*
WebTestBrowserContext::GetClientHintsControllerDelegate() {
  if (!client_hints_controller_delegate_) {
    client_hints_controller_delegate_ =
        std::make_unique<content::MockClientHintsControllerDelegate>(
            content::GetShellUserAgentMetadata());
  }
  return client_hints_controller_delegate_.get();
}

ReduceAcceptLanguageControllerDelegate*
WebTestBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  if (!reduce_accept_lang_controller_delegate_) {
    reduce_accept_lang_controller_delegate_ =
        std::make_unique<content::MockReduceAcceptLanguageControllerDelegate>(
            content::GetShellLanguage());
  }
  return reduce_accept_lang_controller_delegate_.get();
}

}  // namespace content

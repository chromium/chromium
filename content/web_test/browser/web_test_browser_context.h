// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_CONTEXT_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/shell/browser/shell_browser_context.h"

namespace device {
class ScopedGeolocationOverrider;
}  // namespace device

namespace content {

class BackgroundSyncController;
class DownloadManagerDelegate;
class PermissionControllerDelegate;
class PushMessagingService;
class WebTestBackgroundFetchDelegate;
class WebTestPermissionManager;
class WebTestPushMessagingService;
class WebTestStorageAccessManager;

class WebTestBrowserContext final : public ShellBrowserContext {
 public:
  explicit WebTestBrowserContext(bool off_the_record);
  ~WebTestBrowserContext() override;

  // ShellBrowserContext overrides.
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  PushMessagingService* GetPushMessagingService() override;
  PermissionControllerDelegate* GetPermissionControllerDelegate() override;
  BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  BackgroundSyncController* GetBackgroundSyncController() override;
  ClientHintsControllerDelegate* GetClientHintsControllerDelegate() override;

  WebTestPermissionManager* GetWebTestPermissionManager();
  WebTestStorageAccessManager* GetWebTestStorageAccessManager();

 private:
  std::unique_ptr<WebTestPushMessagingService> push_messaging_service_;
  std::unique_ptr<PermissionControllerDelegate> permission_manager_;
  std::unique_ptr<WebTestBackgroundFetchDelegate> background_fetch_delegate_;
  std::unique_ptr<BackgroundSyncController> background_sync_controller_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
  std::unique_ptr<ClientHintsControllerDelegate>
      client_hints_controller_delegate_;
  std::unique_ptr<WebTestStorageAccessManager> storage_access_;

  DISALLOW_COPY_AND_ASSIGN(WebTestBrowserContext);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_BROWSER_CONTEXT_H_

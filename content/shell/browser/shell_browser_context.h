// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"

class SimpleFactoryKey;

namespace content {

class BackgroundSyncController;
class ContentIndexProvider;
class ClientHintsControllerDelegate;
class DownloadManagerDelegate;
class PermissionControllerDelegate;
class ShellDownloadManagerDelegate;
#if !defined(OS_ANDROID)
class ZoomLevelDelegate;
#endif  // !defined(OS_ANDROID)

class ShellBrowserContext : public BrowserContext {
 public:
  // If |delay_services_creation| is true, the owner is responsible for calling
  // CreateBrowserContextServices() for this BrowserContext.
  ShellBrowserContext(bool off_the_record,
                      bool delay_services_creation = false);
  ~ShellBrowserContext() override;

  void set_guest_manager_for_testing(
      BrowserPluginGuestManager* guest_manager) {
    guest_manager_ = guest_manager;
  }

  // BrowserContext implementation.
  base::FilePath GetPath() override;
#if !defined(OS_ANDROID)
  std::unique_ptr<ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif  // !defined(OS_ANDROID)
  bool IsOffTheRecord() override;
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  ResourceContext* GetResourceContext() override;
  BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  PushMessagingService* GetPushMessagingService() override;
  StorageNotificationService* GetStorageNotificationService() override;
  SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  PermissionControllerDelegate* GetPermissionControllerDelegate() override;
  BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  BackgroundSyncController* GetBackgroundSyncController() override;
  BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate() override;
  ContentIndexProvider* GetContentIndexProvider() override;
  ClientHintsControllerDelegate* GetClientHintsControllerDelegate() override;

 protected:
  // Contains URLRequestContextGetter required for resource loading.
  class ShellResourceContext : public ResourceContext {
   public:
    ShellResourceContext();
    ~ShellResourceContext() override;

  private:
    DISALLOW_COPY_AND_ASSIGN(ShellResourceContext);
  };

  bool ignore_certificate_errors() const { return ignore_certificate_errors_; }

  std::unique_ptr<ShellResourceContext> resource_context_;
  std::unique_ptr<ShellDownloadManagerDelegate> download_manager_delegate_;
  std::unique_ptr<PermissionControllerDelegate> permission_manager_;
  std::unique_ptr<BackgroundSyncController> background_sync_controller_;
  std::unique_ptr<ContentIndexProvider> content_index_provider_;

 private:
  // Performs initialization of the ShellBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();
  void FinishInitWhileIOAllowed();

  bool ignore_certificate_errors_;
  bool off_the_record_;
  base::FilePath path_;
  BrowserPluginGuestManager* guest_manager_;
  std::unique_ptr<SimpleFactoryKey> key_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_BROWSER_CONTEXT_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_CONTEXT_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "content/public/browser/browser_context.h"

class WebEngineNetLogObserver;
class WebEnginePermissionDelegate;

class WebEngineBrowserContext : public content::BrowserContext {
 public:
  // |force_incognito|: If set, then this BrowserContext will run in incognito
  // mode even if /data is available.
  explicit WebEngineBrowserContext(bool force_incognito);
  ~WebEngineBrowserContext() override;

  // BrowserContext implementation.
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() override;
  bool IsOffTheRecord() override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  media::VideoDecodePerfHistory* GetVideoDecodePerfHistory() override;

 private:
  // Contains URLRequestContextGetter required for resource loading.
  class ResourceContext;

  media::VideoDecodePerfHistory* GetInMemoryVideoDecodePerfHistory();

  base::FilePath data_dir_path_;

  std::unique_ptr<WebEngineNetLogObserver> net_log_observer_;
  std::unique_ptr<SimpleFactoryKey> simple_factory_key_;
  std::unique_ptr<ResourceContext> resource_context_;
  std::unique_ptr<WebEnginePermissionDelegate> permission_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineBrowserContext);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_BROWSER_CONTEXT_H_

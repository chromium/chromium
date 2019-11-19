// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"

namespace content {

class MockBackgroundSyncController;
class MockResourceContext;
class MockSSLHostStateDelegate;
#if !defined(OS_ANDROID)
class ZoomLevelDelegate;
#endif  // !defined(OS_ANDROID)

class TestBrowserContext : public BrowserContext {
 public:
  explicit TestBrowserContext(
      base::FilePath browser_context_dir_path = base::FilePath());
  ~TestBrowserContext() override;

  // Takes ownership of the temporary directory so that it's not deleted when
  // this object is destructed.
  base::FilePath TakePath();

  void SetSpecialStoragePolicy(storage::SpecialStoragePolicy* policy);
  void SetPermissionControllerDelegate(
      std::unique_ptr<PermissionControllerDelegate> delegate);

  // Allow clients to make this an incognito context.
  void set_is_off_the_record(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
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
  ClientHintsControllerDelegate* GetClientHintsControllerDelegate() override;
  BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  BackgroundSyncController* GetBackgroundSyncController() override;
  BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate() override;

 private:
  // Hold a reference here because BrowserContext owns lifetime.
  base::ScopedTempDir browser_context_dir_;
  std::unique_ptr<MockResourceContext> resource_context_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
  std::unique_ptr<MockSSLHostStateDelegate> ssl_host_state_delegate_;
  std::unique_ptr<PermissionControllerDelegate> permission_controller_delegate_;
  std::unique_ptr<MockBackgroundSyncController> background_sync_controller_;
  bool is_off_the_record_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_BROWSER_CONTEXT_H_

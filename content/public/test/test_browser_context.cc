// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_context.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/null_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/test/mock_resource_context.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/mock_ssl_host_state_delegate.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestBrowserContext::TestBrowserContext(
    base::FilePath browser_context_dir_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "Please construct content::BrowserTaskEnvironment before "
      << "constructing TestBrowserContext instances.  "
      << BrowserThread::GetDCheckCurrentlyOnErrorMessage(BrowserThread::UI);

  if (browser_context_dir_path.empty()) {
    EXPECT_TRUE(browser_context_dir_.CreateUniqueTempDir());
  } else {
    EXPECT_TRUE(browser_context_dir_.Set(browser_context_dir_path));
  }
  BrowserContext::Initialize(this, browser_context_dir_.GetPath());
}

TestBrowserContext::~TestBrowserContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "Please destruct content::TestBrowserContext before destructing "
      << "the BrowserTaskEnvironment instance.  "
      << BrowserThread::GetDCheckCurrentlyOnErrorMessage(BrowserThread::UI);

  NotifyWillBeDestroyed(this);
  ShutdownStoragePartitions();

  // disk_cache::SimpleBackendImpl performs all disk IO on the ThreadPool
  // threads. The cache is initialized in the directory owned by
  // |browser_context_dir_| and so ScopedTempDir destructor may race with cache
  // IO (see https://crbug.com/910029 for example). Let all pending IO
  // operations finish before destroying |browser_context_dir_|.
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

base::FilePath TestBrowserContext::TakePath() {
  return browser_context_dir_.Take();
}

void TestBrowserContext::SetSpecialStoragePolicy(
    storage::SpecialStoragePolicy* policy) {
  special_storage_policy_ = policy;
}

void TestBrowserContext::SetPermissionControllerDelegate(
    std::unique_ptr<PermissionControllerDelegate> delegate) {
  permission_controller_delegate_ = std::move(delegate);
}

base::FilePath TestBrowserContext::GetPath() {
  return browser_context_dir_.GetPath();
}

#if !defined(OS_ANDROID)
std::unique_ptr<ZoomLevelDelegate> TestBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return std::unique_ptr<ZoomLevelDelegate>();
}
#endif  // !defined(OS_ANDROID)

bool TestBrowserContext::IsOffTheRecord() {
  return is_off_the_record_;
}

DownloadManagerDelegate* TestBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

ResourceContext* TestBrowserContext::GetResourceContext() {
  if (!resource_context_)
    resource_context_.reset(new MockResourceContext);
  return resource_context_.get();
}

BrowserPluginGuestManager* TestBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* TestBrowserContext::GetSpecialStoragePolicy() {
  return special_storage_policy_.get();
}

PushMessagingService* TestBrowserContext::GetPushMessagingService() {
  return nullptr;
}

StorageNotificationService*
TestBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

SSLHostStateDelegate* TestBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_)
    ssl_host_state_delegate_.reset(new MockSSLHostStateDelegate());
  return ssl_host_state_delegate_.get();
}

PermissionControllerDelegate*
TestBrowserContext::GetPermissionControllerDelegate() {
  return permission_controller_delegate_.get();
}

ClientHintsControllerDelegate*
TestBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

BackgroundFetchDelegate* TestBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

BackgroundSyncController* TestBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_)
    background_sync_controller_.reset(new MockBackgroundSyncController());

  return background_sync_controller_.get();
}

BrowsingDataRemoverDelegate*
TestBrowserContext::GetBrowsingDataRemoverDelegate() {
  // Most BrowsingDataRemover tests do not require a delegate
  // (not even a mock one).
  return nullptr;
}

}  // namespace content

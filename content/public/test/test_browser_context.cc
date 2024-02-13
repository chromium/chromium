// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_browser_context.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/browser/resource_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"
#include "content/test/mock_ssl_host_state_delegate.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

TestBrowserContext::TestBrowserContext(
    base::FilePath browser_context_dir_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "Please construct content::BrowserTaskEnvironment before "
      << "constructing TestBrowserContext instances.  "
      << BrowserThread::GetCurrentlyOnErrorMessage(BrowserThread::UI);

  if (browser_context_dir_path.empty()) {
    EXPECT_TRUE(browser_context_dir_.CreateUniqueTempDir());
  } else {
    EXPECT_TRUE(browser_context_dir_.Set(browser_context_dir_path));
  }
}

TestBrowserContext::~TestBrowserContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
      << "Please destruct content::TestBrowserContext before destructing "
      << "the BrowserTaskEnvironment instance.  "
      << BrowserThread::GetCurrentlyOnErrorMessage(BrowserThread::UI);

  NotifyWillBeDestroyed();
  ShutdownStoragePartitions();

  // Various things that were just torn down above post tasks to other
  // sequences that eventually bounce back to the main thread and out again.
  // Run all such tasks now before the instance is destroyed so that the
  // |browser_context_dir_| can be fully cleaned up.
  RunAllPendingInMessageLoop(BrowserThread::IO);
  RunAllTasksUntilIdle();

  EXPECT_TRUE(!browser_context_dir_.IsValid() || browser_context_dir_.Delete())
      << browser_context_dir_.GetPath();
}

base::FilePath TestBrowserContext::TakePath() {
  return browser_context_dir_.Take();
}

void TestBrowserContext::SetReduceAcceptLanguageControllerDelegate(
    std::unique_ptr<MockReduceAcceptLanguageControllerDelegate> delegate) {
  reduce_accept_language_controller_delegate_ = std::move(delegate);
}

void TestBrowserContext::SetSpecialStoragePolicy(
    storage::SpecialStoragePolicy* policy) {
  special_storage_policy_ = policy;
}

void TestBrowserContext::SetPermissionControllerDelegate(
    std::unique_ptr<PermissionControllerDelegate> delegate) {
  permission_controller_delegate_ = std::move(delegate);
}

void TestBrowserContext::SetPlatformNotificationService(
    std::unique_ptr<PlatformNotificationService> service) {
  platform_notification_service_ = std::move(service);
}

void TestBrowserContext::SetOriginTrialsControllerDelegate(
    OriginTrialsControllerDelegate* delegate) {
  origin_trials_controller_delegate_ = delegate;
}

void TestBrowserContext::SetClientHintsControllerDelegate(
    ClientHintsControllerDelegate* delegate) {
  client_hints_controller_delegate_ = delegate;
}

base::FilePath TestBrowserContext::GetPath() {
  return browser_context_dir_.GetPath();
}

std::unique_ptr<ZoomLevelDelegate> TestBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  return nullptr;
}

bool TestBrowserContext::IsOffTheRecord() {
  return is_off_the_record_;
}

DownloadManagerDelegate* TestBrowserContext::GetDownloadManagerDelegate() {
  return nullptr;
}

BrowserPluginGuestManager* TestBrowserContext::GetGuestManager() {
  return nullptr;
}

storage::SpecialStoragePolicy* TestBrowserContext::GetSpecialStoragePolicy() {
  return special_storage_policy_.get();
}

PlatformNotificationService*
TestBrowserContext::GetPlatformNotificationService() {
  return platform_notification_service_.get();
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
    ssl_host_state_delegate_ = std::make_unique<MockSSLHostStateDelegate>();
  return ssl_host_state_delegate_.get();
}

PermissionControllerDelegate*
TestBrowserContext::GetPermissionControllerDelegate() {
  return permission_controller_delegate_.get();
}

ClientHintsControllerDelegate*
TestBrowserContext::GetClientHintsControllerDelegate() {
  return client_hints_controller_delegate_;
}

BackgroundFetchDelegate* TestBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

BackgroundSyncController* TestBrowserContext::GetBackgroundSyncController() {
  if (!background_sync_controller_) {
    background_sync_controller_ =
        std::make_unique<MockBackgroundSyncController>();
  }

  return background_sync_controller_.get();
}

BrowsingDataRemoverDelegate*
TestBrowserContext::GetBrowsingDataRemoverDelegate() {
  // Most BrowsingDataRemover tests do not require a delegate
  // (not even a mock one).
  return nullptr;
}

ReduceAcceptLanguageControllerDelegate*
TestBrowserContext::GetReduceAcceptLanguageControllerDelegate() {
  return reduce_accept_language_controller_delegate_.get();
}

OriginTrialsControllerDelegate*
TestBrowserContext::GetOriginTrialsControllerDelegate() {
  return origin_trials_controller_delegate_.get();
}

// static
TestBrowserContext* TestBrowserContext::FromBrowserContext(
    BrowserContext* browser_context) {
  return static_cast<TestBrowserContext*>(browser_context);
}

}  // namespace content

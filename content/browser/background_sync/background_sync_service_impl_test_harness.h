// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_TEST_HARNESS_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_TEST_HARNESS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "content/browser/background_sync/background_sync_context_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

class BackgroundSyncServiceImplTestHarness : public testing::Test {
 public:
  static void RegisterServiceWorkerCallback(
      bool* called,
      int64_t* store_registration_id,
      blink::ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);
  static void FindServiceWorkerRegistrationCallback(
      scoped_refptr<ServiceWorkerRegistration>* out_registration,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  static void ErrorAndRegistrationCallback(
      bool* called,
      blink::mojom::BackgroundSyncError* out_error,
      blink::mojom::SyncRegistrationOptionsPtr* out_registration,
      blink::mojom::BackgroundSyncError error,
      blink::mojom::SyncRegistrationOptionsPtr registration);
  static void ErrorAndRegistrationListCallback(
      bool* called,
      blink::mojom::BackgroundSyncError* out_error,
      unsigned long* out_array_size,
      blink::mojom::BackgroundSyncError error,
      std::vector<blink::mojom::SyncRegistrationOptionsPtr> registrations);
  static void ErrorCallback(bool* called,
                            blink::mojom::BackgroundSyncError* out_error,
                            blink::mojom::BackgroundSyncError error);

  BackgroundSyncServiceImplTestHarness();
  ~BackgroundSyncServiceImplTestHarness() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_refptr<BackgroundSyncContextImpl> background_sync_context_;
  blink::mojom::SyncRegistrationOptionsPtr default_sync_registration_;
  std::vector<std::string> mojo_bad_messages_;
  int64_t sw_registration_id_;

 private:
  // SetUp helper methods
  void CreateTestHelper();
  void CreateStoragePartition();
  void CreateBackgroundSyncContext();
  void CreateServiceWorkerRegistration();
  void CollectMojoError(const std::string& message);

  // Helpers for testing *BackgroundSyncServiceImpl methods
  void RegisterOneShotSync(
      blink::mojom::SyncRegistrationOptionsPtr sync,
      blink::mojom::OneShotBackgroundSyncService::RegisterCallback callback);
  void GetOneShotSyncRegistrations(
      blink::mojom::OneShotBackgroundSyncService::GetRegistrationsCallback
          callback);

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncServiceImplTestHarness);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_SERVICE_IMPL_TEST_HARNESS_H_

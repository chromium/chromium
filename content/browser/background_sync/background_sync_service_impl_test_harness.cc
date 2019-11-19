// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl_test_harness.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_background_sync_context.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

using ::testing::_;

const char kServiceWorkerScope[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";

void BackgroundSyncServiceImplTestHarness::RegisterServiceWorkerCallback(
    bool* called,
    int64_t* out_registration_id,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *called = true;
  *out_registration_id = registration_id;
}

void BackgroundSyncServiceImplTestHarness::
    FindServiceWorkerRegistrationCallback(
        scoped_refptr<ServiceWorkerRegistration>* out_registration,
        blink::ServiceWorkerStatusCode status,
        scoped_refptr<ServiceWorkerRegistration> registration) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *out_registration = std::move(registration);
}

void BackgroundSyncServiceImplTestHarness::ErrorAndRegistrationCallback(
    bool* called,
    blink::mojom::BackgroundSyncError* out_error,
    blink::mojom::SyncRegistrationOptionsPtr* out_registration,
    blink::mojom::BackgroundSyncError error,
    blink::mojom::SyncRegistrationOptionsPtr registration) {
  *called = true;
  *out_error = error;
  *out_registration = registration.Clone();
}

void BackgroundSyncServiceImplTestHarness::ErrorAndRegistrationListCallback(
    bool* called,
    blink::mojom::BackgroundSyncError* out_error,
    unsigned long* out_array_size,
    blink::mojom::BackgroundSyncError error,
    std::vector<blink::mojom::SyncRegistrationOptionsPtr> registrations) {
  *called = true;
  *out_error = error;
  if (error == blink::mojom::BackgroundSyncError::NONE)
    *out_array_size = registrations.size();
}

void BackgroundSyncServiceImplTestHarness::ErrorCallback(
    bool* called,
    blink::mojom::BackgroundSyncError* out_error,
    blink::mojom::BackgroundSyncError error) {
  *called = true;
  *out_error = error;
}

BackgroundSyncServiceImplTestHarness::BackgroundSyncServiceImplTestHarness()
    : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {
  default_sync_registration_ = blink::mojom::SyncRegistrationOptions::New();
}

BackgroundSyncServiceImplTestHarness::~BackgroundSyncServiceImplTestHarness() =
    default;

void BackgroundSyncServiceImplTestHarness::SetUp() {
  // Don't let the tests be confused by the real-world device connectivity
  background_sync_test_util::SetIgnoreNetworkChanges(true);

  mojo::core::SetDefaultProcessErrorCallback(base::AdaptCallbackForRepeating(
      base::BindOnce(&BackgroundSyncServiceImplTestHarness::CollectMojoError,
                     base::Unretained(this))));

  CreateTestHelper();
  CreateStoragePartition();
  CreateBackgroundSyncContext();
  ASSERT_NO_FATAL_FAILURE(CreateServiceWorkerRegistration());
}

void BackgroundSyncServiceImplTestHarness::CollectMojoError(
    const std::string& message) {
  mojo_bad_messages_.push_back(message);
}

void BackgroundSyncServiceImplTestHarness::TearDown() {
  // This must be explicitly destroyed here to ensure that destruction
  // of both the BackgroundSyncContextImpl and the BackgroundSyncManager
  // occurs on the correct thread.
  background_sync_context_->Shutdown();
  base::RunLoop().RunUntilIdle();
  background_sync_context_ = nullptr;

  // Restore the network observer functionality for subsequent tests
  background_sync_test_util::SetIgnoreNetworkChanges(false);

  mojo::core::SetDefaultProcessErrorCallback(
      mojo::core::ProcessErrorCallback());
}

// SetUp helper methods
void BackgroundSyncServiceImplTestHarness::CreateTestHelper() {
  embedded_worker_helper_ =
      std::make_unique<EmbeddedWorkerTestHelper>((base::FilePath()));
  std::unique_ptr<MockPermissionManager> mock_permission_manager =
      std::make_unique<testing::NiceMock<MockPermissionManager>>();
  ON_CALL(*mock_permission_manager,
          GetPermissionStatus(PermissionType::BACKGROUND_SYNC, _, _))
      .WillByDefault(testing::Return(blink::mojom::PermissionStatus::GRANTED));
  embedded_worker_helper_->browser_context()->SetPermissionControllerDelegate(
      std::move(mock_permission_manager));
}

void BackgroundSyncServiceImplTestHarness::CreateStoragePartition() {
  // Creates a StoragePartition so that the BackgroundSyncManager can
  // use it to access the BrowserContext.
  storage_partition_impl_ = StoragePartitionImpl::Create(
      embedded_worker_helper_->browser_context(), /* in_memory= */ true,
      base::FilePath(), /* partition_domain= */ "");
  storage_partition_impl_->Initialize();
  embedded_worker_helper_->context_wrapper()->set_storage_partition(
      storage_partition_impl_.get());
}

void BackgroundSyncServiceImplTestHarness::CreateBackgroundSyncContext() {
  // Registering for background sync includes a check for having a same-origin
  // main frame. Use a test context that allows control over that check.
  background_sync_context_ = base::MakeRefCounted<TestBackgroundSyncContext>();
  background_sync_context_->Init(
      embedded_worker_helper_->context_wrapper(),
      storage_partition_impl_->GetDevToolsBackgroundServicesContext());

  // Tests do not expect the sync event to fire immediately after
  // register (and cleanup up the sync registrations).  Prevent the sync
  // event from firing by setting the network state to have no connection.
  // NOTE: The setup of the network connection must happen after the
  //       BackgroundSyncManager has been setup, including any asynchronous
  //       initialization.
  base::RunLoop().RunUntilIdle();
  BackgroundSyncNetworkObserver* network_observer =
      background_sync_context_->background_sync_manager()
          ->GetNetworkObserverForTesting();
  network_observer->NotifyManagerIfConnectionChangedForTesting(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
}

void BackgroundSyncServiceImplTestHarness::CreateServiceWorkerRegistration() {
  bool called = false;
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL(kServiceWorkerScope);
  embedded_worker_helper_->context()->RegisterServiceWorker(
      GURL(kServiceWorkerScript), options,
      blink::mojom::FetchClientSettingsObject::New(),
      base::BindOnce(&RegisterServiceWorkerCallback, &called,
                     &sw_registration_id_));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  embedded_worker_helper_->context_wrapper()->FindReadyRegistrationForId(
      sw_registration_id_, GURL(kServiceWorkerScope).GetOrigin(),
      base::BindOnce(FindServiceWorkerRegistrationCallback, &sw_registration_));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(sw_registration_);
}

}  // namespace content

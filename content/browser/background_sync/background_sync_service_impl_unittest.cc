// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_service_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/browser/background_sync/background_sync_context.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_background_sync_context.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

using ::testing::_;

const char kServiceWorkerScope[] = "https://example.com/a";
const char kServiceWorkerScript[] = "https://example.com/a/script.js";

// Callbacks from SetUp methods
void RegisterServiceWorkerCallback(bool* called,
                                   int64_t* store_registration_id,
                                   blink::ServiceWorkerStatusCode status,
                                   const std::string& status_message,
                                   int64_t registration_id) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *called = true;
  *store_registration_id = registration_id;
}

void FindServiceWorkerRegistrationCallback(
    scoped_refptr<ServiceWorkerRegistration>* out_registration,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status)
      << blink::ServiceWorkerStatusToString(status);
  *out_registration = std::move(registration);
}

// Callbacks from BackgroundSyncServiceImpl methods

void ErrorAndRegistrationCallback(
    bool* called,
    blink::mojom::BackgroundSyncError* out_error,
    blink::mojom::SyncRegistrationPtr* out_registration,
    blink::mojom::BackgroundSyncError error,
    blink::mojom::SyncRegistrationPtr registration) {
  *called = true;
  *out_error = error;
  *out_registration = registration.Clone();
}

void ErrorAndRegistrationListCallback(
    bool* called,
    blink::mojom::BackgroundSyncError* out_error,
    unsigned long* out_array_size,
    blink::mojom::BackgroundSyncError error,
    std::vector<blink::mojom::SyncRegistrationPtr> registrations) {
  *called = true;
  *out_error = error;
  if (error == blink::mojom::BackgroundSyncError::NONE)
    *out_array_size = registrations.size();
}

}  // namespace

class BackgroundSyncServiceImplTest : public testing::Test {
 public:
  BackgroundSyncServiceImplTest()
      : thread_bundle_(
            new TestBrowserThreadBundle(TestBrowserThreadBundle::IO_MAINLOOP)) {
    default_sync_registration_ = blink::mojom::SyncRegistration::New();
  }

  void SetUp() override {
    // Don't let the tests be confused by the real-world device connectivity
    background_sync_test_util::SetIgnoreNetworkChanges(true);

    CreateTestHelper();
    CreateStoragePartition();
    CreateBackgroundSyncContext();
    CreateServiceWorkerRegistration();
    CreateBackgroundSyncServiceImpl();
  }

  void TearDown() override {
    // This must be explicitly destroyed here to ensure that destruction
    // of both the BackgroundSyncContext and the BackgroundSyncManager occurs on
    // the correct thread.
    background_sync_context_->Shutdown();
    base::RunLoop().RunUntilIdle();
    background_sync_context_ = nullptr;

    // Restore the network observer functionality for subsequent tests
    background_sync_test_util::SetIgnoreNetworkChanges(false);
  }

  // SetUp helper methods
  void CreateTestHelper() {
    embedded_worker_helper_.reset(
        new EmbeddedWorkerTestHelper(base::FilePath()));
    std::unique_ptr<MockPermissionManager> mock_permission_manager(
        new testing::NiceMock<MockPermissionManager>());
    ON_CALL(*mock_permission_manager,
            GetPermissionStatus(PermissionType::BACKGROUND_SYNC, _, _))
        .WillByDefault(
            testing::Return(blink::mojom::PermissionStatus::GRANTED));
    embedded_worker_helper_->browser_context()->SetPermissionControllerDelegate(
        std::move(mock_permission_manager));
  }

  void CreateStoragePartition() {
    // Creates a StoragePartition so that the BackgroundSyncManager can
    // use it to access the BrowserContext.
    storage_partition_impl_.reset(new StoragePartitionImpl(
        embedded_worker_helper_->browser_context(), base::FilePath(), nullptr));
    embedded_worker_helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());
  }

  void CreateBackgroundSyncContext() {
    // Registering for background sync includes a check for having a same-origin
    // main frame. Use a test context that allows control over that check.
    background_sync_context_ =
        base::MakeRefCounted<TestBackgroundSyncContext>();
    background_sync_context_->Init(embedded_worker_helper_->context_wrapper());

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

  void CreateServiceWorkerRegistration() {
    bool called = false;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = GURL(kServiceWorkerScope);
    embedded_worker_helper_->context()->RegisterServiceWorker(
        GURL(kServiceWorkerScript), options,
        base::BindOnce(&RegisterServiceWorkerCallback, &called,
                       &sw_registration_id_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);

    embedded_worker_helper_->context_wrapper()->FindReadyRegistrationForId(
        sw_registration_id_, GURL(kServiceWorkerScope).GetOrigin(),
        base::BindOnce(FindServiceWorkerRegistrationCallback,
                       &sw_registration_));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(sw_registration_);
  }

  void CreateBackgroundSyncServiceImpl() {
    // Create a dummy mojo channel so that the BackgroundSyncServiceImpl can be
    // instantiated.
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncService>
        service_request = mojo::MakeRequest(&service_ptr_);
    // Create a new BackgroundSyncServiceImpl bound to the dummy channel.
    background_sync_context_->CreateService(std::move(service_request));
    base::RunLoop().RunUntilIdle();

    service_impl_ = background_sync_context_->services_.begin()->first;
    ASSERT_TRUE(service_impl_);
  }

  // Helpers for testing BackgroundSyncServiceImpl methods
  void Register(
      blink::mojom::SyncRegistrationPtr sync,
      blink::mojom::BackgroundSyncService::RegisterCallback callback) {
    service_impl_->Register(std::move(sync), sw_registration_id_,
                            std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistrations(
      blink::mojom::BackgroundSyncService::GetRegistrationsCallback callback) {
    service_impl_->GetRegistrations(sw_registration_id_, std::move(callback));
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<TestBrowserThreadBundle> thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> embedded_worker_helper_;
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
  scoped_refptr<BackgroundSyncContext> background_sync_context_;
  int64_t sw_registration_id_;
  scoped_refptr<ServiceWorkerRegistration> sw_registration_;
  blink::mojom::BackgroundSyncServicePtr service_ptr_;
  BackgroundSyncServiceImpl*
      service_impl_;  // Owned by background_sync_context_
  blink::mojom::SyncRegistrationPtr default_sync_registration_;
};

// Tests

TEST_F(BackgroundSyncServiceImplTest, Register) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  blink::mojom::SyncRegistrationPtr reg;
  Register(
      default_sync_registration_.Clone(),
      base::BindOnce(&ErrorAndRegistrationCallback, &called, &error, &reg));
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  EXPECT_EQ("", reg->tag);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrations) {
  bool called = false;
  blink::mojom::BackgroundSyncError error;
  unsigned long array_size = 0UL;
  GetRegistrations(base::BindOnce(&ErrorAndRegistrationListCallback, &called,
                                  &error, &array_size));
  EXPECT_TRUE(called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, error);
  EXPECT_EQ(0UL, array_size);
}

TEST_F(BackgroundSyncServiceImplTest, GetRegistrationsWithRegisteredSync) {
  bool register_called = false;
  bool get_registrations_called = false;
  blink::mojom::BackgroundSyncError register_error;
  blink::mojom::BackgroundSyncError getregistrations_error;
  blink::mojom::SyncRegistrationPtr register_reg;
  unsigned long array_size = 0UL;
  Register(default_sync_registration_.Clone(),
           base::BindOnce(&ErrorAndRegistrationCallback, &register_called,
                          &register_error, &register_reg));
  EXPECT_TRUE(register_called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, register_error);
  GetRegistrations(base::BindOnce(&ErrorAndRegistrationListCallback,
                                  &get_registrations_called,
                                  &getregistrations_error, &array_size));
  EXPECT_TRUE(get_registrations_called);
  EXPECT_EQ(blink::mojom::BackgroundSyncError::NONE, getregistrations_error);
  EXPECT_EQ(1UL, array_size);
}

}  // namespace content

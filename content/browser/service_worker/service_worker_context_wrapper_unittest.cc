// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

// Fixture for the ServiceWorkerContextWrapper test. It uses a disk user data
// directory in order to test starting the browser with a registration already
// written to storage.
class ServiceWorkerContextWrapperTest : public testing::Test {
 public:
  ServiceWorkerContextWrapperTest() = default;

  void SetUp() override {
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());
    InitWrapper();
  }

  void TearDown() override {
    // Shutdown or else ASAN complains of leaks.
    wrapper_->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void InitWrapper() {
    browser_context_ = std::make_unique<TestBrowserContext>();
    wrapper_ = base::MakeRefCounted<ServiceWorkerContextWrapper>(
        browser_context_.get());
    url_loader_factory_getter_ = base::MakeRefCounted<URLLoaderFactoryGetter>();
    StoragePartitionImpl* storage_partition =
        static_cast<StoragePartitionImpl*>(
            BrowserContext::GetStoragePartitionForSite(
                browser_context_.get(), GURL("https://example.com")));
    wrapper_->set_storage_partition(storage_partition);
    wrapper_->Init(user_data_directory_.GetPath(), nullptr, nullptr, nullptr,
                   url_loader_factory_getter_.get());
    // Init() posts a couple tasks to the IO thread. Let them finish.
    base::RunLoop().RunUntilIdle();

    storage()->LazyInitializeForTest();
  }

  ServiceWorkerContextCore* context() { return wrapper_->context(); }
  ServiceWorkerStorage* storage() { return context()->storage(); }

 protected:
  BrowserTaskEnvironment task_environment_{BrowserTaskEnvironment::IO_MAINLOOP};
  base::ScopedTempDir user_data_directory_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContextWrapperTest);
};

// Test that the UI thread knows which origins have registrations upon
// browser startup. Regression test for https://crbug.com/991143.
TEST_F(ServiceWorkerContextWrapperTest, HasRegistration) {
  // Make a service worker.
  GURL scope("https://example.com/");
  GURL script("https://example.com/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope, script);

  // Store it.
  base::RunLoop loop;
  storage()->StoreRegistration(
      registration.get(), registration->waiting_version(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) {
            ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
            loop.Quit();
          }));
  loop.Run();

  // Simulate browser shutdown and restart.
  wrapper_->Shutdown();
  base::RunLoop().RunUntilIdle();
  wrapper_.reset();
  InitWrapper();

  // Now test that registrations are recognized.
  wrapper_->WaitForRegistrationsInitializedForTest();
  EXPECT_TRUE(wrapper_->HasRegistrationForOrigin(GURL("https://example.com")));
  EXPECT_FALSE(wrapper_->HasRegistrationForOrigin(GURL("https://example.org")));
}

}  // namespace content

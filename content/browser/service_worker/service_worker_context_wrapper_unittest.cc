// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <memory>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/origin.h"

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
  }

  ServiceWorkerContextCore* context() { return wrapper_->context(); }
  ServiceWorkerRegistry* registry() { return context()->registry(); }

  blink::ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->StoreRegistration(
        registration.get(), registration->waiting_version(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  blink::ServiceWorkerStatusCode DeleteRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration) {
    blink::ServiceWorkerStatusCode result;
    base::RunLoop loop;
    registry()->DeleteRegistration(
        registration, registration->scope().GetOrigin(),
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

  std::vector<url::Origin> GetInstalledRegistrationOrigins(
      base::Optional<std::string> host_filter) {
    std::vector<url::Origin> result;
    base::RunLoop loop;
    wrapper_->GetInstalledRegistrationOrigins(
        host_filter, base::BindLambdaForTesting(
                         [&](const std::vector<url::Origin>& origins) {
                           result = origins;
                           loop.Quit();
                         }));
    loop.Run();
    return result;
  }

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
      CreateServiceWorkerRegistrationAndVersion(context(), scope, script,
                                                /*resource_id=*/1);

  // Store it.
  base::RunLoop loop;
  registry()->StoreRegistration(
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
  EXPECT_TRUE(wrapper_->MaybeHasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example.com"))));
  EXPECT_FALSE(wrapper_->MaybeHasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example.org"))));
}

// This test involves storing two registrations for the same origin to storage
// and deleting one of them to check that MaybeHasRegistrationForOrigin still
// correctly returns TRUE since there is still one registration for the origin,
// and should only return FALSE when ALL registrations for that origin have been
// deleted from storage.
TEST_F(ServiceWorkerContextWrapperTest, DeleteRegistrationsForSameOrigin) {
  wrapper_->WaitForRegistrationsInitializedForTest();

  // Make two registrations for same origin.
  GURL scope1("https://example1.com/abc/");
  GURL script1("https://example1.com/abc/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);
  GURL scope2("https://example1.com/xyz/");
  GURL script2("https://example1.com/xyz/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope2, script2, 1);

  // Store both registrations.
  ASSERT_EQ(StoreRegistration(registration1),
            blink::ServiceWorkerStatusCode::kOk);
  ASSERT_EQ(StoreRegistration(registration2),
            blink::ServiceWorkerStatusCode::kOk);

  // Delete one of the registrations.
  ASSERT_EQ(DeleteRegistration(registration1),
            blink::ServiceWorkerStatusCode::kOk);

  // Run loop until idle to wait for
  // ServiceWorkerRegistry::DidDeleteRegistration() to be executed, and make
  // sure that NotifyAllRegistrationsDeletedForOrigin() is not called.
  base::RunLoop().RunUntilIdle();

  // Now test that a registration for an origin is still recognized.
  EXPECT_TRUE(wrapper_->MaybeHasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example1.com"))));

  // Remove second registration.
  ASSERT_EQ(DeleteRegistration(registration2),
            blink::ServiceWorkerStatusCode::kOk);

  // Run loop until idle to wait for
  // ServiceWorkerRegistry::DidDeleteRegistration() to be executed, and make
  // sure that this time NotifyAllRegistrationsDeletedForOrigin() is called.
  base::RunLoop().RunUntilIdle();

  // Now test that origin does not have any registrations.
  EXPECT_FALSE(wrapper_->MaybeHasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example1.com"))));
}

// This tests deleting registrations from storage and checking that even if live
// registrations may exist, MaybeHasRegistrationForOrigin correctly returns
// FALSE since the registrations do not exist in storage.
TEST_F(ServiceWorkerContextWrapperTest, DeleteRegistration) {
  wrapper_->WaitForRegistrationsInitializedForTest();

  // Make registration.
  GURL scope1("https://example2.com/");
  GURL script1("https://example2.com/");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);

  // Store registration.
  ASSERT_EQ(StoreRegistration(registration),
            blink::ServiceWorkerStatusCode::kOk);

  wrapper_->OnRegistrationCompleted(registration->id(), registration->scope());
  base::RunLoop().RunUntilIdle();

  // Now test that a registration is recognized.
  EXPECT_TRUE(wrapper_->MaybeHasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example2.com"))));

  // Delete registration from storage.
  ASSERT_EQ(DeleteRegistration(registration),
            blink::ServiceWorkerStatusCode::kOk);

  // Finish deleting registration from storage.
  base::RunLoop().RunUntilIdle();

  // Now test that origin does not have any registrations. This should return
  // FALSE even when live registrations may exist, as the registrations have
  // been deleted from storage.
  EXPECT_FALSE(wrapper_->MaybeHasRegistrationForOrigin(
      url::Origin::Create(GURL("https://example2.com"))));
}

// GetInstalledRegistrationOrigins tests:

// No registration.
TEST_F(ServiceWorkerContextWrapperTest, GetInstalledRegistrationOrigins_Empty) {
  wrapper_->WaitForRegistrationsInitializedForTest();

  // No registration stored yet.
  std::vector<url::Origin> registered_origins =
      GetInstalledRegistrationOrigins(base::nullopt);
  EXPECT_EQ(registered_origins.size(), 0UL);
}

// On registration.
TEST_F(ServiceWorkerContextWrapperTest, GetInstalledRegistrationOrigins_One) {
  const GURL scope("https://example.com/");
  const GURL script("https://example.com/sw.js");
  const url::Origin origin = url::Origin::Create(scope);

  wrapper_->WaitForRegistrationsInitializedForTest();
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope, script,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  std::vector<url::Origin> installed_origins =
      GetInstalledRegistrationOrigins(base::nullopt);
  ASSERT_EQ(installed_origins.size(), 1UL);
  EXPECT_EQ(*installed_origins.begin(), origin);
}

// Two registrations from the same origin.
TEST_F(ServiceWorkerContextWrapperTest,
       GetInstalledRegistrationOrigins_SameOrigin) {
  const GURL scope1("https://example.com/foo");
  const GURL script1("https://example.com/foo/sw.js");
  const url::Origin origin = url::Origin::Create(scope1);
  const GURL scope2("https://example.com/bar");
  const GURL script2("https://example.com/bar/sw.js");

  wrapper_->WaitForRegistrationsInitializedForTest();

  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration1),
            blink::ServiceWorkerStatusCode::kOk);
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope2, script2,
                                                /*resource_id=*/2);
  ASSERT_EQ(StoreRegistration(registration2),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  std::vector<url::Origin> installed_origins =
      GetInstalledRegistrationOrigins(base::nullopt);
  ASSERT_EQ(installed_origins.size(), 1UL);
  EXPECT_EQ(*installed_origins.begin(), origin);
}

// Two registrations from different origins.
TEST_F(ServiceWorkerContextWrapperTest,
       GetInstalledRegistrationOrigins_DifferentOrigin) {
  const GURL scope1("https://example1.com/foo");
  const GURL script1("https://example1.com/foo/sw.js");
  const url::Origin origin1 = url::Origin::Create(scope1);
  const GURL scope2("https://example2.com/bar");
  const GURL script2("https://example2.com/bar/sw.js");
  const url::Origin origin2 = url::Origin::Create(scope2);

  wrapper_->WaitForRegistrationsInitializedForTest();

  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration1),
            blink::ServiceWorkerStatusCode::kOk);
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope2, script2,
                                                /*resource_id=*/2);
  ASSERT_EQ(StoreRegistration(registration2),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  std::vector<url::Origin> installed_origins =
      GetInstalledRegistrationOrigins(base::nullopt);
  ASSERT_EQ(installed_origins.size(), 2UL);
  EXPECT_TRUE(base::Contains(installed_origins, origin1));
  EXPECT_TRUE(base::Contains(installed_origins, origin2));
}

// One registration, host filter matches it.
TEST_F(ServiceWorkerContextWrapperTest,
       GetInstalledRegistrationOrigins_HostFilterMatch) {
  const GURL scope("https://example.com/");
  const GURL script("https://example.com/sw.js");
  const url::Origin origin = url::Origin::Create(scope);

  wrapper_->WaitForRegistrationsInitializedForTest();
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope, script,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  std::vector<url::Origin> installed_origins =
      GetInstalledRegistrationOrigins("example.com");
  ASSERT_EQ(installed_origins.size(), 1UL);
  EXPECT_EQ(*installed_origins.begin(), origin);
}

// One registration, host filter does not match it.
TEST_F(ServiceWorkerContextWrapperTest,
       GetInstalledRegistrationOrigins_HostFilterNoMatch) {
  const GURL scope("https://example.com/");
  const GURL script("https://example.com/sw.js");
  const url::Origin origin = url::Origin::Create(scope);

  wrapper_->WaitForRegistrationsInitializedForTest();
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateServiceWorkerRegistrationAndVersion(context(), scope, script,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  std::vector<url::Origin> installed_origins =
      GetInstalledRegistrationOrigins("example.test");
  EXPECT_EQ(installed_origins.size(), 0UL);
}

// Two registrations, one is deleted, only the other one is returned.
TEST_F(ServiceWorkerContextWrapperTest,
       GetInstalledRegistrationOrigins_DeletedRegistration) {
  const GURL scope1("https://example1.com/foo");
  const GURL script1("https://example1.com/foo/sw.js");
  const url::Origin origin1 = url::Origin::Create(scope1);
  const GURL scope2("https://example2.com/bar");
  const GURL script2("https://example2.com/bar/sw.js");
  const url::Origin origin2 = url::Origin::Create(scope2);

  wrapper_->WaitForRegistrationsInitializedForTest();

  scoped_refptr<ServiceWorkerRegistration> registration1 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope1, script1,
                                                /*resource_id=*/1);
  ASSERT_EQ(StoreRegistration(registration1),
            blink::ServiceWorkerStatusCode::kOk);
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      CreateServiceWorkerRegistrationAndVersion(context(), scope2, script2,
                                                /*resource_id=*/2);
  ASSERT_EQ(StoreRegistration(registration2),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  {
    std::vector<url::Origin> installed_origins =
        GetInstalledRegistrationOrigins(base::nullopt);
    ASSERT_EQ(installed_origins.size(), 2UL);
    EXPECT_TRUE(base::Contains(installed_origins, origin1));
    EXPECT_TRUE(base::Contains(installed_origins, origin2));
  }

  // Delete |registration2|.
  ASSERT_EQ(DeleteRegistration(registration2),
            blink::ServiceWorkerStatusCode::kOk);
  base::RunLoop().RunUntilIdle();

  // After |registration2| is deleted, only |origin1| should be returned.
  {
    std::vector<url::Origin> installed_origins =
        GetInstalledRegistrationOrigins(base::nullopt);
    ASSERT_EQ(installed_origins.size(), 1UL);
    EXPECT_EQ(installed_origins[0], origin1);
  }
}

}  // namespace content

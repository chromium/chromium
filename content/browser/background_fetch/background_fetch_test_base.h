// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BASE_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/background_fetch/background_fetch_test_browser_context.h"
#include "content/browser/background_fetch/background_fetch_test_service_worker.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerRegistration;
class StoragePartitionImpl;

// Base class containing common functionality needed in unit tests written for
// the Background Fetch feature.
class BackgroundFetchTestBase : public ::testing::Test {
 public:
  using TestResponse = MockBackgroundFetchDelegate::TestResponse;
  using TestResponseBuilder = MockBackgroundFetchDelegate::TestResponseBuilder;

  BackgroundFetchTestBase();

  BackgroundFetchTestBase(const BackgroundFetchTestBase&) = delete;
  BackgroundFetchTestBase& operator=(const BackgroundFetchTestBase&) = delete;

  ~BackgroundFetchTestBase() override;

  // ::testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

  // Registers a Service Worker for the testing origin and returns its
  // |service_worker_registration_id|. If registration failed, this will be
  // |blink::mojom::kInvalidServiceWorkerRegistrationId|. The
  // ServiceWorkerRegistration will be kept alive for the test's lifetime.
  int64_t RegisterServiceWorker();

  // `RegisterServiceWorker` but for the provided |origin|.
  int64_t RegisterServiceWorkerForOrigin(const url::Origin& origin);

  // Unregisters the test Service Worker and verifies that the unregistration
  // succeeded.
  void UnregisterServiceWorker(int64_t service_worker_registration_id);

  // Creates a FetchAPIRequestPtr instance for the given details and
  // provides a faked |response|.
  blink::mojom::FetchAPIRequestPtr CreateRequestWithProvidedResponse(
      const std::string& method,
      const GURL& url,
      std::unique_ptr<TestResponse> response);

  // Creates a blink::mojom::BackgroundFetchRegistrationDataPtr object.
  blink::mojom::BackgroundFetchRegistrationDataPtr
  CreateBackgroundFetchRegistrationData(
      const std::string& developer_id,
      blink::mojom::BackgroundFetchResult result,
      blink::mojom::BackgroundFetchFailureReason failure_reason);

  // Returns the embedded worker test helper instance, which can be used to
  // influence the behavior of the Service Worker events.
  EmbeddedWorkerTestHelper* embedded_worker_test_helper() {
    return &embedded_worker_test_helper_;
  }

  // Returns the browser context that should be used for the tests.
  TestBrowserContext* browser_context() { return &browser_context_; }

  // Returns the once-initialized default storage partition to be used in tests.
  base::WeakPtr<StoragePartitionImpl> storage_partition() {
    return storage_partition_factory_.GetWeakPtr();
  }

  // Returns the storage key that should be used for Background Fetch tests.
  const blink::StorageKey& storage_key() const { return storage_key_; }

  // Returns the DevTools context for logging events.
  DevToolsBackgroundServicesContextImpl& devtools_context();

 protected:
  BrowserTaskEnvironment task_environment_;  // Must be first member.

 private:
  BackgroundFetchTestBrowserContext browser_context_;

  raw_ptr<MockBackgroundFetchDelegate> delegate_;

  EmbeddedWorkerTestHelper embedded_worker_test_helper_;

  blink::StorageKey storage_key_;

  base::WeakPtrFactory<StoragePartitionImpl> storage_partition_factory_;

  int next_pattern_id_ = 0;

  // Vector of ServiceWorkerRegistration instances that have to be kept alive
  // for the lifetime of this test.
  std::vector<scoped_refptr<ServiceWorkerRegistration>>
      service_worker_registrations_;

  bool set_up_called_ = false;
  bool tear_down_called_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_BASE_H_

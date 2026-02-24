// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_synthetic_response_manager.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class ServiceWorkerSyntheticResponseManagerTest : public testing::Test {
 public:
  ServiceWorkerSyntheticResponseManagerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
    // Create a ServiceWorkerVersion.
    scoped_refptr<ServiceWorkerRegistration> registration =
        CreateNewServiceWorkerRegistration(
            helper_->context()->registry(),
            blink::mojom::ServiceWorkerRegistrationOptions(
                GURL("https://example.com/scope"),
                blink::mojom::ScriptType::kClassic,
                blink::mojom::ServiceWorkerUpdateViaCache::kImports),
            blink::StorageKey::CreateFirstParty(
                url::Origin::Create(GURL("https://example.com/scope"))));
    version_ = CreateNewServiceWorkerVersion(
        helper_->context()->registry(), registration.get(),
        GURL("https://example.com/script.js"),
        blink::mojom::ScriptType::kClassic);
  }

  void TearDown() override {
    version_ = nullptr;
    helper_.reset();
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerVersion> version_;
};

TEST_F(ServiceWorkerSyntheticResponseManagerTest, Status) {
  // Set a response head without headers (simulating the crash condition).
  // Verify status is kNotReady.
  auto response_head = network::mojom::URLResponseHead::New();
  ASSERT_FALSE(response_head->headers);
  version_->SetResponseHeadForSyntheticResponse(std::move(response_head));
  ServiceWorkerSyntheticResponseManager manager(version_);
  EXPECT_EQ(manager.Status(), ServiceWorkerSyntheticResponseManager::
                                  SyntheticResponseStatus::kNotReady);

  // Update the response head with valid headers, and verify status is still
  // kNotReady, since the manager determines the status at the construction.
  auto valid_head = network::mojom::URLResponseHead::New();
  valid_head->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n\n");
  version_->SetResponseHeadForSyntheticResponse(std::move(valid_head));
  EXPECT_EQ(manager.Status(), ServiceWorkerSyntheticResponseManager::
                                  SyntheticResponseStatus::kNotReady);

  // Create a new manager, and verify the status is kReady, since the version
  // has a valid response head.
  ServiceWorkerSyntheticResponseManager manager_with_ready_status(version_);
  EXPECT_EQ(
      manager_with_ready_status.Status(),
      ServiceWorkerSyntheticResponseManager::SyntheticResponseStatus::kReady);
}

}  // namespace content

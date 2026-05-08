// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {

class MockPushMessagingService : public PushMessagingService {
 public:
  void SubscribeFromDocument(const GURL& requesting_origin,
                             int64_t service_worker_registration_id,
                             int render_process_id,
                             int render_frame_id,
                             blink::mojom::PushSubscriptionOptionsPtr options,
                             bool user_gesture,
                             RegisterCallback callback) override {}

  void SubscribeFromWorker(const GURL& requesting_origin,
                           int64_t service_worker_registration_id,
                           int render_process_id,
                           blink::mojom::PushSubscriptionOptionsPtr options,
                           RegisterCallback callback) override {}

  void GetSubscriptionInfo(const GURL& origin,
                           int64_t service_worker_registration_id,
                           const std::string& sender_id,
                           const std::string& subscription_id,
                           SubscriptionInfoCallback callback) override {}

  void Unsubscribe(blink::mojom::PushUnregistrationReason reason,
                   const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   UnregisterCallback callback) override {}

  bool SupportNonVisibleMessages() override { return true; }

  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override {}

  void DidDeleteServiceWorkerDatabase() override {}
};

class CustomTestBrowserContext : public TestBrowserContext {
 public:
  PushMessagingService* GetPushMessagingService() override {
    return &push_service_;
  }

 private:
  MockPushMessagingService push_service_;
};

class PushMessagingManagerTest : public ::testing::Test {
 protected:
  PushMessagingManagerTest() = default;
  ~PushMessagingManagerTest() override = default;

  BrowserTaskEnvironment task_environment_;
  CustomTestBrowserContext browser_context_;
};

// Verifies that the Origin Security Check correctly blocks unauthorized access,
// even preventing a Time-of-Check to Time-of-Use (TOCTOU) race condition where
// a dormant service worker registration bypasses the initial security check
// and then turns live during the asynchronous database fetch.
TEST_F(PushMessagingManagerTest, TimeOfCheckToTimeOfUse_OriginSecurityCheck) {
  MockRenderProcessHost mock_render_process_host(&browser_context_);

  // Lock the mock render process host to a different origin
  // ("https://good.com/"), ensuring that subsequent requests targeting
  // "https://evil.com/" will trigger the expected cross-origin security
  // violations preventing unauthorized access.
  ChildProcessSecurityPolicyImpl::GetInstance()->LockProcessForTesting(
      IsolationContext(
          BrowsingInstanceId(1), &browser_context_,
          /*is_guest=*/false, /*is_fenced=*/false,
          OriginAgentClusterIsolationState::CreateForDefaultIsolation(
              &browser_context_)),
      mock_render_process_host.GetID(), GURL("https://good.com/"));

  const int64_t kServiceWorkerRegistrationId = 123L;

  scoped_refptr<ServiceWorkerContextWrapper> sw_context =
      base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
          browser_context_.GetDefaultStoragePartition()
              ->GetServiceWorkerContext()));

  sw_context->StoreRegistrationUserData(
      kServiceWorkerRegistrationId,
      blink::StorageKey::CreateFromStringForTesting("https://evil.com/"),
      {{"push_registration_id", "sub_id_abc"},
       {"push_sender_id", "1234567890"}},
      base::DoNothing());

  auto push_manager = std::make_unique<PushMessagingManager>(
      mock_render_process_host, -1, sw_context);

  // Call GetSubscription explicitly on PushMessagingManager.
  // Because the registration is not currently live (dormant), the Origin
  // Security Check is bypassed at this check stage.
  push_manager->GetSubscription(
      kServiceWorkerRegistrationId,
      base::BindOnce([](blink::mojom::PushGetRegistrationStatus status,
                        blink::mojom::PushSubscriptionPtr subscription) {}));

  // Simulate the registration successfully turning live from the
  // database during the asynchronous lookup path.
  // We call DidGetSubscription directly to simulate the fetched callback.
  // The service worker registration must be live at this point.
  auto sw_options = blink::mojom::ServiceWorkerRegistrationOptions::New();
  sw_options->scope = GURL("https://evil.com/");
  scoped_refptr<ServiceWorkerRegistration> registration =
      ServiceWorkerRegistration::Create(
          *sw_options,
          blink::StorageKey::CreateFromStringForTesting("https://evil.com/"),
          kServiceWorkerRegistrationId, sw_context->context()->AsWeakPtr(),
          blink::mojom::AncestorFrameType::kNormalFrame);

  std::vector<std::string> subscription_info = {"sub_id_abc", "1234567890"};

  // Verify that the secondary Origin Security Check at the
  // Time-of-Use correctly denies access and terminates the sequence with
  // bad_message::PMM_GET_SUBSCRIPTION_INVALID_ORIGIN.
  base::HistogramTester histogram_tester;

  push_manager->DidGetSubscription(
      base::BindOnce([](blink::mojom::PushGetRegistrationStatus status,
                        blink::mojom::PushSubscriptionPtr subscription) {
        FAIL() << "Callback should not be called when bad message is detected.";
      }),
      kServiceWorkerRegistrationId, subscription_info,
      blink::ServiceWorkerStatusCode::kOk);

  histogram_tester.ExpectUniqueSample(
      "Stability.BadMessageTerminated.Content",
      bad_message::PMM_GET_SUBSCRIPTION_INVALID_ORIGIN, 1);
}

}  // namespace content

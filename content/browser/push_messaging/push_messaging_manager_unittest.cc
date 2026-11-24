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
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/bad_message.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registry.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"
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
                   UnregisterCallback callback) override {}

  bool SupportNonVisibleMessages() override { return true; }

  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override {}

  void DidDeleteServiceWorkerDatabase() override {
    did_delete_service_worker_database_ = true;
  }

  bool did_delete_service_worker_database() const {
    return did_delete_service_worker_database_;
  }

 private:
  bool did_delete_service_worker_database_ = false;
};

class CustomTestBrowserContext : public TestBrowserContext {
 public:
  PushMessagingService* GetPushMessagingService() override {
    return &push_service_;
  }

  MockPushMessagingService& push_service() { return push_service_; }

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

TEST_F(PushMessagingManagerTest,
       InvalidRegistrationIdRejectedWithoutStorageWipe) {
  struct StorageWipeObserver : public ServiceWorkerContextCoreObserver {
    void OnDeleteAndStartOver() override { ++delete_and_start_over_count; }
    void OnStorageWiped() override { ++storage_wiped_count; }
    int delete_and_start_over_count = 0;
    int storage_wiped_count = 0;
  } observer;

  scoped_refptr<ServiceWorkerContextWrapper> sw_context =
      base::WrapRefCounted(static_cast<ServiceWorkerContextWrapper*>(
          browser_context_.GetDefaultStoragePartition()
              ->GetServiceWorkerContext()));

  base::ScopedObservation<ServiceWorkerContextWrapper,
                          ServiceWorkerContextCoreObserver>
      observation(&observer);
  observation.Observe(sw_context.get());

  // Persist a Service Worker registration and user data for another origin.
  const GURL kOtherScope("https://other.example.com/scope/");
  const GURL kOtherScript("https://other.example.com/sw.js");
  const blink::StorageKey kOtherKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kOtherScope));
  scoped_refptr<ServiceWorkerRegistration> stored_reg =
      CreateServiceWorkerRegistrationAndVersion(
          sw_context->context(), kOtherScope, kOtherScript, kOtherKey,
          /*resource_id=*/1);
  const int64_t kStoredRegistrationId = stored_reg->id();
  {
    base::test::TestFuture<blink::ServiceWorkerStatusCode> store_reg_future;
    sw_context->context()->registry().StoreRegistration(
        stored_reg.get(), stored_reg->waiting_version(),
        store_reg_future.GetCallback());
    ASSERT_EQ(store_reg_future.Get(), blink::ServiceWorkerStatusCode::kOk);
  }
  {
    base::test::TestFuture<blink::ServiceWorkerStatusCode> store_data_future;
    sw_context->StoreRegistrationUserData(kStoredRegistrationId, kOtherKey,
                                          {{"sample_key", "sample_value"}},
                                          store_data_future.GetCallback());
    ASSERT_EQ(store_data_future.Get(), blink::ServiceWorkerStatusCode::kOk);
  }

  MockRenderProcessHost mock_render_process_host(&browser_context_);
  ChildProcessSecurityPolicyImpl::GetInstance()->LockProcessForTesting(
      IsolationContext(
          BrowsingInstanceId(2), &browser_context_,
          /*is_guest=*/false, /*is_fenced=*/false,
          OriginAgentClusterIsolationState::CreateForDefaultIsolation(
              &browser_context_)),
      mock_render_process_host.GetID(), GURL("https://caller.example.com/"));

  auto push_manager = std::make_unique<PushMessagingManager>(
      mock_render_process_host, ChildProcessHost::kInvalidUniqueID, sw_context);

  // GetSubscription with kInvalidServiceWorkerRegistrationId must report a bad
  // Mojo message, disconnect the pipe, and leave stored registrations intact.
  {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::Remote<blink::mojom::PushMessaging> push_remote;
    push_manager->AddPushMessagingReceiver(
        push_remote.BindNewPipeAndPassReceiver());
    push_remote->GetSubscription(
        blink::mojom::kInvalidServiceWorkerRegistrationId, base::DoNothing());
    EXPECT_EQ("Invalid service worker registration ID",
              bad_message_observer.WaitForBadMessage());
    push_remote.FlushForTesting();
    EXPECT_FALSE(push_remote.is_connected());
  }

  // Subscribe and Unsubscribe with kInvalidServiceWorkerRegistrationId must
  // also report bad Mojo messages and disconnect the offending pipes.
  {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::Remote<blink::mojom::PushMessaging> push_remote;
    push_manager->AddPushMessagingReceiver(
        push_remote.BindNewPipeAndPassReceiver());
    auto options = blink::mojom::PushSubscriptionOptions::New();
    options->user_visible_only = true;
    push_remote->Subscribe(blink::mojom::kInvalidServiceWorkerRegistrationId,
                           std::move(options), /*user_gesture=*/true,
                           base::DoNothing());
    EXPECT_EQ("Invalid service worker registration ID",
              bad_message_observer.WaitForBadMessage());
    push_remote.FlushForTesting();
    EXPECT_FALSE(push_remote.is_connected());
  }
  {
    mojo::test::BadMessageObserver bad_message_observer;
    mojo::Remote<blink::mojom::PushMessaging> push_remote;
    push_manager->AddPushMessagingReceiver(
        push_remote.BindNewPipeAndPassReceiver());
    push_remote->Unsubscribe(blink::mojom::kInvalidServiceWorkerRegistrationId,
                             base::DoNothing());
    EXPECT_EQ("Invalid service worker registration ID",
              bad_message_observer.WaitForBadMessage());
    push_remote.FlushForTesting();
    EXPECT_FALSE(push_remote.is_connected());
  }

  // Positive controls: unknown positive registration IDs and synthetic response
  // registration IDs (<= -10) return REGISTRATION_NOT_FOUND without reporting a
  // bad message or wiping storage.
  for (int64_t non_sentinel_id :
       {int64_t{31337},
        blink::mojom::kSyntheticResponseServiceWorkerRegistrationId}) {
    mojo::Remote<blink::mojom::PushMessaging> push_remote;
    push_manager->AddPushMessagingReceiver(
        push_remote.BindNewPipeAndPassReceiver());
    base::test::TestFuture<blink::mojom::PushGetRegistrationStatus,
                           blink::mojom::PushSubscriptionPtr>
        get_sub_future;
    push_remote->GetSubscription(non_sentinel_id, get_sub_future.GetCallback());
    EXPECT_EQ(get_sub_future.Get<0>(),
              blink::mojom::PushGetRegistrationStatus::REGISTRATION_NOT_FOUND);
    EXPECT_FALSE(get_sub_future.Get<1>());
    EXPECT_TRUE(push_remote.is_connected());
  }

  content::RunAllTasksUntilIdle();
  EXPECT_EQ(0, observer.delete_and_start_over_count);
  EXPECT_EQ(0, observer.storage_wiped_count);
  EXPECT_FALSE(
      browser_context_.push_service().did_delete_service_worker_database());

  // Verify the other origin's stored registration and user data remain intact.
  {
    base::test::TestFuture<blink::ServiceWorkerStatusCode,
                           scoped_refptr<ServiceWorkerRegistration>>
        find_future;
    sw_context->context()->registry().FindRegistrationForId(
        kStoredRegistrationId, kOtherKey, find_future.GetCallback());
    EXPECT_EQ(find_future.Get<0>(), blink::ServiceWorkerStatusCode::kOk);
    EXPECT_TRUE(find_future.Get<1>());
  }
  {
    base::test::TestFuture<const std::vector<std::string>&,
                           blink::ServiceWorkerStatusCode>
        get_data_future;
    sw_context->GetRegistrationUserData(kStoredRegistrationId, {"sample_key"},
                                        get_data_future.GetCallback());
    ASSERT_EQ(get_data_future.Get<1>(), blink::ServiceWorkerStatusCode::kOk);
    ASSERT_EQ(1u, get_data_future.Get<0>().size());
    EXPECT_EQ("sample_value", get_data_future.Get<0>()[0]);
  }
}

}  // namespace content

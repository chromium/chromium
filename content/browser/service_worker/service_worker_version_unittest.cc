// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_ping_controller.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_service.mojom.h"
#include "content/public/test/test_utils.h"
#include "net/base/test_completion_callback.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_router_rule.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace content {
namespace service_worker_version_unittest {

constexpr base::TimeDelta kTestTimeoutBeyondRequestTimeout =
    // Value of kRequestTimeout in service_worker_version.cc
    base::Minutes(5) +
    // A little past that.
    base::Minutes(1);

base::OnceCallback<void()> VerifyCalled(
    bool* called,
    base::OnceClosure quit_closure = base::OnceClosure()) {
  return base::BindOnce(
      [](bool* called, base::OnceClosure quit_closure) {
        *called = true;
        if (!quit_closure.is_null())
          std::move(quit_closure).Run();
      },
      called, std::move(quit_closure));
}

void ObserveStatusChanges(ServiceWorkerVersion* version,
                          std::vector<ServiceWorkerVersion::Status>* statuses) {
  statuses->push_back(version->status());
  version->RegisterStatusChangeCallback(base::BindOnce(
      &ObserveStatusChanges, base::Unretained(version), statuses));
}

base::Time GetYesterday() {
  return base::Time::Now() - base::Days(1) - base::Seconds(1);
}

enum class StorageKeyTestCase {
  kFirstParty,
  kThirdParty,
};

class ServiceWorkerVersionTest
    : public testing::Test,
      public testing::WithParamInterface<StorageKeyTestCase> {
 public:
  ServiceWorkerVersionTest(const ServiceWorkerVersionTest&) = delete;
  ServiceWorkerVersionTest& operator=(const ServiceWorkerVersionTest&) = delete;

 protected:
  using FetchHandlerExistence = blink::mojom::FetchHandlerExistence;

  struct CachedMetadataUpdateListener : public ServiceWorkerVersion::Observer {
    CachedMetadataUpdateListener() = default;
    ~CachedMetadataUpdateListener() override = default;
    void OnCachedMetadataUpdated(ServiceWorkerVersion* version,
                                 size_t size) override {
      ++updated_count;
    }
    int updated_count = 0;
  };

  ServiceWorkerVersionTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    helper_ = GetHelper();

    scope_ = GURL("https://www.example.com/test/");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ = CreateNewServiceWorkerRegistration(
        helper_->context()->registry(), options, GetTestStorageKey(scope_));
    version_ = CreateNewServiceWorkerVersion(
        helper_->context()->registry(), registration_.get(),
        GURL("https://www.example.com/test/service_worker.js"),
        blink::mojom::ScriptType::kClassic);
    EXPECT_EQ(url::Origin::Create(scope_), version_->key().origin());
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    records.push_back(WriteToDiskCacheWithIdSync(
        helper_->context()->GetStorageControl(), version_->script_url(), 10,
        {} /* headers */, "I'm a body", "I'm a meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptResponse(
        EmbeddedWorkerTestHelper::CreateMainScriptResponse());
    if (GetFetchHandlerType()) {
      version_->set_fetch_handler_type(*GetFetchHandlerType());
    }

    // Make the registration findable via storage functions.
    std::optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    helper_->context()->registry()->StoreRegistration(
        registration_.get(), version_.get(),
        ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  }

  virtual std::unique_ptr<EmbeddedWorkerTestHelper> GetHelper() {
    return std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
  }

  void TearDown() override {
    client_render_process_hosts_.clear();
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  bool IsPingActivated(ServiceWorkerVersion* version) const {
    return version->ping_controller_.IsActivated();
  }

  void NotifyScriptEvaluationStart(ServiceWorkerVersion* version) {
    version->OnScriptEvaluationStart();
  }

  void SimulateDispatchEvent(ServiceWorkerMetrics::EventType event_type) {
    std::optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;

    // Make sure worker is running.
    version_->RunAfterStartWorker(
        event_type,
        ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
              version_->running_status());

    // Start request, as if an event is being dispatched.
    int request_id = version_->StartRequest(event_type, base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // And finish request, as if a response to the event was received.
    EXPECT_TRUE(version_->FinishRequest(request_id, /*was_handled=*/true));
  }

  void SetupTestTickClock() { version_->SetTickClockForTesting(&tick_clock_); }

  virtual std::optional<ServiceWorkerVersion::FetchHandlerType>
  GetFetchHandlerType() const {
    return ServiceWorkerVersion::FetchHandlerType::kNotSkippable;
  }

  // Make the client in a different process from the service worker when
  // |in_different_process| is true.
  CommittedServiceWorkerClient ActivateWithControllee(
      bool in_different_process = false) {
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_);
    int controllee_process_id = ChildProcessHost::kInvalidUniqueID;

    if (in_different_process) {
      auto client_render_process_host =
          std::make_unique<MockRenderProcessHost>(helper_->browser_context());
      controllee_process_id = client_render_process_host->GetID();
      client_render_process_hosts_.push_back(
          std::move(client_render_process_host));
    } else {
      controllee_process_id = version_->embedded_worker()->process_id();
    }

    ScopedServiceWorkerClient service_worker_client =
        CreateServiceWorkerClient(helper_->context());
    service_worker_client->UpdateUrls(registration_->scope(),
                                      registration_->key().origin(),
                                      registration_->key());

    // Just to set `controllee_process_id`.
    auto committed_service_worker_client = CommittedServiceWorkerClient(
        std::move(service_worker_client),
        GlobalRenderFrameHostId(controllee_process_id,
                                /*mock frame_routing_id=*/1));

    committed_service_worker_client->SetControllerRegistration(
        registration_, false /* notify_controllerchange */);
    EXPECT_TRUE(version_->HasControllee());
    EXPECT_TRUE(committed_service_worker_client->controller());
    return committed_service_worker_client;
  }

  bool UseFirstPartyStorageKey() {
    return GetParam() == StorageKeyTestCase::kFirstParty;
  }

  blink::StorageKey GetTestStorageKey(const GURL& scope_url) {
    auto scope_origin = url::Origin::Create(scope_url);
    if (UseFirstPartyStorageKey()) {
      return blink::StorageKey::CreateFirstParty(std::move(scope_origin));
    } else {
      // For simplicity create a third-party storage key by setting the ancestor
      // chain bit to kCrossSite.
      auto storage_key = blink::StorageKey::Create(
          scope_origin, net::SchemefulSite(scope_origin),
          blink::mojom::AncestorChainBit::kCrossSite,
          /*third_party_partitioning_allowed=*/true);
      EXPECT_TRUE(storage_key.IsThirdPartyContext());
      return storage_key;
    }
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  // Used to hold render process hosts for clients which reside in different
  // processes from the service worker.
  std::vector<std::unique_ptr<MockRenderProcessHost>>
      client_render_process_hosts_;
  GURL scope_;
  // Some tests sets a custom tick clock, store it here to ensure that it
  // outlives `version_`.
  base::SimpleTestTickClock tick_clock_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerVersionTest,
    testing::ValuesIn({StorageKeyTestCase::kFirstParty,
                       StorageKeyTestCase::kThirdParty}),
    [](const testing::TestParamInfo<StorageKeyTestCase>& info) {
      switch (info.param) {
        case (StorageKeyTestCase::kFirstParty):
          return "FirstPartyStorageKey";
        case (StorageKeyTestCase::kThirdParty):
          return "ThirdPartyStorageKey";
      }
    });

// An instance client that breaks the Mojo connection upon receiving the
// Start() message.
class FailStartInstanceClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  FailStartInstanceClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}

  FailStartInstanceClient(const FailStartInstanceClient&) = delete;
  FailStartInstanceClient& operator=(const FailStartInstanceClient&) = delete;

  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override {
    // Don't save the Mojo ptrs. The connection breaks.
  }
};

TEST_P(ServiceWorkerVersionTest, ConcurrentStartAndStop) {
  // Call StartWorker() multiple times.
  std::optional<blink::ServiceWorkerStatusCode> status1;
  std::optional<blink::ServiceWorkerStatusCode> status2;
  std::optional<blink::ServiceWorkerStatusCode> status3;
  base::RunLoop run_loop_1;
  base::RunLoop run_loop_2;
  base::RunLoop run_loop_3;

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status1, run_loop_1.QuitClosure()));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveStartingServiceWorker(
      version_->version_id()));
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status2, run_loop_2.QuitClosure()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));

  // Call StartWorker() after it's started.
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status3, run_loop_3.QuitClosure()));

  run_loop_1.Run();
  run_loop_2.Run();
  run_loop_3.Run();

  // All should just succeed.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status1.value());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status2.value());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status3.value());

  {
    // Call StopWorker() multiple times.
    bool has_stopped1 = false;
    bool has_stopped2 = false;
    base::RunLoop run_loop_4;
    base::RunLoop run_loop_5;

    version_->StopWorker(VerifyCalled(&has_stopped1, run_loop_4.QuitClosure()));
    version_->StopWorker(VerifyCalled(&has_stopped2, run_loop_5.QuitClosure()));

    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping,
              version_->running_status());
    EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
        version_->version_id()));
    run_loop_4.Run();
    run_loop_5.Run();
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped,
              version_->running_status());
    EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
        version_->version_id()));

    // All StopWorker should just succeed.
    EXPECT_TRUE(has_stopped1);
    EXPECT_TRUE(has_stopped2);
  }

  // Start worker again.
  status1.reset();
  status2.reset();

  base::RunLoop run_loop_6;
  base::RunLoop run_loop_7;

  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status1, run_loop_6.QuitClosure()));

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveStartingServiceWorker(
      version_->version_id()));
  run_loop_6.Run();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));

  {
    // Call StopWorker()
    bool has_stopped = false;
    version_->StopWorker(VerifyCalled(&has_stopped));

    // And try calling StartWorker while StopWorker is in queue.
    version_->StartWorker(
        ServiceWorkerMetrics::EventType::UNKNOWN,
        ReceiveServiceWorkerStatus(&status2, run_loop_7.QuitClosure()));

    EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping,
              version_->running_status());
    EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
        version_->version_id()));
    run_loop_7.Run();
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
              version_->running_status());
    EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
        version_->version_id()));

    // All should just succeed.
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status1.value());
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status2.value());
    EXPECT_TRUE(has_stopped);
  }
}

TEST_P(ServiceWorkerVersionTest, DispatchEventToStoppedWorker) {
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());

  // Dispatch an event without starting the worker.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  EXPECT_TRUE(version_->HasNoWork());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  // The worker may still be handling events dispatched directly from
  // controllees. We cannot say the version doesn't handle any tasks until the
  // worker reports "No Work" (= ServiceWorkerVersion::OnRequestTermination()
  // is called).
  EXPECT_FALSE(version_->HasNoWork());

  // The worker should be now started.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());

  // Stop the worker, and then dispatch an event immediately after that.
  bool has_stopped = false;
  version_->StopWorker(VerifyCalled(&has_stopped));
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);
  EXPECT_TRUE(has_stopped);

  // The worker may still be handling events dispatched directly from
  // controllees. We cannot say the version doesn't handle any tasks until the
  // worker reports "No Work" (= ServiceWorkerVersion::OnRequestTermination()
  // is called).
  EXPECT_FALSE(version_->HasNoWork());

  // The worker should be now started again.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, StartUnregisteredButStillLiveWorker) {
  // Start the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Delete the registration.
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  helper_->context()->registry()->DeleteRegistration(
      registration_,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
  run_loop.Run();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // The live registration is marked as uninstalling, but still exists.
  ASSERT_TRUE(registration_->is_uninstalling());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));

  // Stop the worker.
  StopServiceWorker(version_.get());
  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));

  // Dispatch an event on the unregistered and stopped but still live worker.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME);

  // The worker should be now started again.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));
}

TEST_P(ServiceWorkerVersionTest, InstallAndWaitCompletion) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(VerifyCalled(&status_change_called));

  // Dispatch an install event.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  // Version's status must not have changed during installation.
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version_->status());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));
}

TEST_P(ServiceWorkerVersionTest, ActivateAndWaitCompletion) {
  // TODO(mek): This test (and the one above for the install event) made more
  // sense back when ServiceWorkerVersion was responsible for updating the
  // status. Now a better version of this test should probably be added to
  // ServiceWorkerRegistrationTest instead.

  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // Wait for the completion.
  bool status_change_called = false;
  version_->RegisterStatusChangeCallback(VerifyCalled(&status_change_called));

  // Dispatch an activate event.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::ACTIVATE);

  // Version's status must not have changed during activation.
  EXPECT_FALSE(status_change_called);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, version_->status());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));
}

TEST_P(ServiceWorkerVersionTest, RepeatedlyObserveStatusChanges) {
  EXPECT_EQ(ServiceWorkerVersion::NEW, version_->status());

  // Repeatedly observe status changes (the callback re-registers itself).
  std::vector<ServiceWorkerVersion::Status> statuses;
  version_->RegisterStatusChangeCallback(base::BindOnce(
      &ObserveStatusChanges, base::RetainedRef(version_), &statuses));

  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATING);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->SetStatus(ServiceWorkerVersion::REDUNDANT);

  // Verify that we could successfully observe repeated status changes.
  ASSERT_EQ(5U, statuses.size());
  ASSERT_EQ(ServiceWorkerVersion::INSTALLING, statuses[0]);
  ASSERT_EQ(ServiceWorkerVersion::INSTALLED, statuses[1]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATING, statuses[2]);
  ASSERT_EQ(ServiceWorkerVersion::ACTIVATED, statuses[3]);
  ASSERT_EQ(ServiceWorkerVersion::REDUNDANT, statuses[4]);
}

TEST_P(ServiceWorkerVersionTest, Doom) {
  // Add a controllee.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  ScopedServiceWorkerClient service_worker_client =
      CreateServiceWorkerClient(helper_->context());
  service_worker_client->UpdateUrls(registration_->scope(),
                                    registration_->key().origin(),
                                    registration_->key());
  service_worker_client->SetControllerRegistration(registration_, false);
  EXPECT_TRUE(version_->HasControllee());
  EXPECT_TRUE(service_worker_client->controller());

  // Set main_script_load_params_.
  version_->set_main_script_load_params(
      blink::mojom::WorkerMainScriptLoadParams::New());

  // Doom the version.
  version_->Doom();

  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));

  // The controllee should have been removed.
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version_->status());
  EXPECT_FALSE(version_->HasControllee());
  EXPECT_FALSE(service_worker_client->controller());

  // Ensure that the params are released.
  EXPECT_TRUE(version_->main_script_load_params_.is_null());
}

TEST_P(ServiceWorkerVersionTest, SetDevToolsAttached) {
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));

  ASSERT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());

  ASSERT_TRUE(version_->timeout_timer_.IsRunning());
  ASSERT_FALSE(version_->start_time_.is_null());
  ASSERT_FALSE(version_->skip_recording_startup_time_);

  // Simulate DevTools is attached. This should deactivate the timer for start
  // timeout, but not stop the timer itself.
  version_->SetDevToolsAttached(true);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_TRUE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  // Simulate DevTools is detached. This should reactivate the timer for start
  // timeout.
  version_->SetDevToolsAttached(false);
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_FALSE(version_->start_time_.is_null());
  EXPECT_TRUE(version_->skip_recording_startup_time_);

  run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
}

// Tests that a worker containing external request with
// ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout does not stop
// if devtools is detached (after it is attached).
//
// Regression test for crbug.com/1152255#c144
TEST_P(ServiceWorkerVersionTest, DevToolsAttachThenDetach) {
  SetupTestTickClock();
  std::optional<blink::ServiceWorkerStatusCode> status;

  auto start_external_request_test =
      [&](ServiceWorkerExternalRequestTimeoutType timeout_type,
          bool expect_running) {
        SCOPED_TRACE(testing::Message()
                     << std::boolalpha << "expect_running: " << expect_running);
        {
          // Start worker.
          base::RunLoop run_loop;
          version_->StartWorker(
              ServiceWorkerMetrics::EventType::UNKNOWN,
              ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
          ASSERT_EQ(blink::EmbeddedWorkerStatus::kStarting,
                    version_->running_status());

          // Add an external request.
          EXPECT_EQ(ServiceWorkerExternalRequestResult::kOk,
                    version_->StartExternalRequest(
                        base::Uuid::GenerateRandomV4(), timeout_type));
          run_loop.Run();
          EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
          EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
                    version_->running_status());
        }

        {
          // Simulate DevTools is attached.
          version_->SetDevToolsAttached(true);
          EXPECT_TRUE(version_->timeout_timer_.IsRunning());
          EXPECT_TRUE(version_->start_time_.is_null());
          EXPECT_TRUE(version_->skip_recording_startup_time_);

          // Simulate DevTools is detached.
          version_->SetDevToolsAttached(false);
          EXPECT_TRUE(version_->timeout_timer_.IsRunning());
          EXPECT_TRUE(version_->skip_recording_startup_time_);

          EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
          EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
                    version_->running_status());
        }

        // Now advance time to check worker's running state.
        tick_clock_.Advance(kTestTimeoutBeyondRequestTimeout);
        version_->timeout_timer_.user_task().Run();
        base::RunLoop().RunUntilIdle();

        const bool worker_stopped_or_stopping =
            version_->OnRequestTermination();

        EXPECT_EQ(!expect_running, worker_stopped_or_stopping);
        const bool worker_running =
            version_->running_status() == blink::EmbeddedWorkerStatus::kRunning;
        EXPECT_EQ(expect_running, worker_running);

        // Ensure the worker is stopped, so that start_external_request_test()
        // works next time.
        {
          bool has_stopped = false;
          base::RunLoop run_loop;
          version_->StopWorker(
              VerifyCalled(&has_stopped, run_loop.QuitClosure()));
          run_loop.Run();
          EXPECT_TRUE(has_stopped);
          EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped,
                    version_->running_status());
        }
      };

  // kDoesNotTimeout timeout external request would continue to keep the worker
  // running.
  start_external_request_test(
      ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
      true /* expect_running */);

  // And ensure that kDefault timeout external request does not keep the worker
  // running.
  start_external_request_test(ServiceWorkerExternalRequestTimeoutType::kDefault,
                              false /* expect_running */);
}

TEST_P(ServiceWorkerVersionTest, RequestTerminationWithDevToolsAttached) {
  auto* service_worker =
      helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get());

  version_->SetDevToolsAttached(true);

  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());

  // Idle delay is not set at this point. The renderer process uses the default
  // value.
  EXPECT_FALSE(service_worker->idle_delay().has_value());

  // If OnRequestTermination() is called when DevTools is attached, then the
  // worker's idle timeout is set to the default value forcefully because the
  // worker needs to be running until DevTools is detached even if there's no
  // inflight event.
  version_->OnRequestTermination();
  service_worker->FlushForTesting();
  EXPECT_EQ(blink::mojom::kServiceWorkerDefaultIdleDelayInSeconds,
            service_worker->idle_delay()->InSeconds());
}

// Test that update isn't triggered for a non-stale worker.
TEST_P(ServiceWorkerVersionTest, StaleUpdate_FreshWorker) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);

  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that update isn't triggered for a non-active worker.
TEST_P(ServiceWorkerVersionTest, StaleUpdate_NonActiveWorker) {
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  registration_->SetInstallingVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);

  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected when starting a worker.
TEST_P(ServiceWorkerVersionTest, StaleUpdate_StartWorker) {
  // Starting the worker marks it as stale.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Update is actually scheduled after the worker stops.
  StopServiceWorker(version_.get());
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that staleness is detected on a running worker.
TEST_P(ServiceWorkerVersionTest, StaleUpdate_RunningWorker) {
  // Start a fresh worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(base::Time::Now());
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  EXPECT_TRUE(version_->stale_time_.is_null());

  // Simulate it running for a day. It will be marked stale.
  registration_->set_last_update_check(GetYesterday());
  version_->OnTimeoutTimer();
  EXPECT_FALSE(version_->stale_time_.is_null());
  EXPECT_FALSE(version_->update_timer_.IsRunning());

  // Simulate it running for past the wait threshold. The update will be
  // scheduled.
  version_->stale_time_ = base::TimeTicks::Now() -
                          ServiceWorkerVersion::kStartNewWorkerTimeout -
                          base::Minutes(1);
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());
}

// Test that a stream of events doesn't restart the timer.
TEST_P(ServiceWorkerVersionTest, StaleUpdate_DoNotDeferTimer) {
  // Make a stale worker.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_last_update_check(GetYesterday());
  base::TimeTicks stale_time = base::TimeTicks::Now() -
                               ServiceWorkerVersion::kStartNewWorkerTimeout -
                               base::Minutes(1);
  version_->stale_time_ = stale_time;

  // Stale time is not deferred.
  version_->RunAfterStartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                                base::DoNothing());
  version_->RunAfterStartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                                base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(stale_time, version_->stale_time_);

  // Timeout triggers the update.
  version_->OnTimeoutTimer();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_TRUE(version_->update_timer_.IsRunning());

  // Update timer is not deferred.
  base::TimeTicks run_time = version_->update_timer_.desired_run_time();
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->stale_time_.is_null());
  EXPECT_EQ(run_time, version_->update_timer_.desired_run_time());
}

TEST_P(ServiceWorkerVersionTest, StartRequestWithNullContext) {
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  version_->context_ = nullptr;
  version_->StartRequest(ServiceWorkerMetrics::EventType::PUSH,
                         base::DoNothing());
  // Test passes if it doesn't crash.
}

// Tests the delay mechanism for self-updating service workers, to prevent
// them from running forever (see https://crbug.com/805496).
TEST_P(ServiceWorkerVersionTest, ResetUpdateDelay) {
  const base::TimeDelta kMinute = base::Minutes(1);
  const base::TimeDelta kNoDelay = base::TimeDelta();

  // Initialize the delay.
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  registration_->set_self_update_delay(kMinute);

  // Events that can be triggered by a worker should not reset the delay.
  // See the comment in ServiceWorkerVersion::StartRequestWithCustomTimeout.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::INSTALL);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::ACTIVATE);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::MESSAGE);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kMinute, registration_->self_update_delay());

  // Events that can only be triggered externally reset the delay.
  // Repeat the test for several such events.
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::SYNC);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNoDelay, registration_->self_update_delay());

  registration_->set_self_update_delay(kMinute);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNoDelay, registration_->self_update_delay());

  registration_->set_self_update_delay(kMinute);
  SimulateDispatchEvent(ServiceWorkerMetrics::EventType::PUSH);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kNoDelay, registration_->self_update_delay());
}

TEST_P(ServiceWorkerVersionTest, UpdateCachedMetadata) {
  CachedMetadataUpdateListener listener;
  version_->AddObserver(&listener);
  ASSERT_EQ(0, listener.updated_count);
  auto* service_worker =
      helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get());
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  service_worker->RunUntilInitializeGlobalScope();

  // Simulate requesting SetCachedMetadata from the service worker global scope.
  std::vector<uint8_t> data{1, 2, 3};
  service_worker->host()->SetCachedMetadata(version_->script_url(), data);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, listener.updated_count);

  // Simulate requesting ClearCachedMetadata from the service worker global
  // scope.
  service_worker->host()->ClearCachedMetadata(version_->script_url());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, listener.updated_count);
  version_->RemoveObserver(&listener);
}

TEST_P(ServiceWorkerVersionTest, RestartWorker) {
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  bool has_stopped = false;

  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));

  // Restart the worker. The inflight event should have been failed.
  version_->StopWorker(VerifyCalled(&has_stopped));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping, version_->running_status());
  run_loop.Run();

  // Restart the worker.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // The worker should have been stopped.
  EXPECT_TRUE(has_stopped);
  // All inflight events should have been aborted.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed, status.value());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());

  // SetAllRequestExpirations() after restarting should not crash since all
  // events should have been removed at this point: crbug.com/791451.
  version_->SetAllRequestExpirations(base::TimeTicks());
}

class DelayMessageWorker : public FakeServiceWorker {
 public:
  explicit DelayMessageWorker(EmbeddedWorkerTestHelper* helper)
      : FakeServiceWorker(helper) {}

  DelayMessageWorker(const DelayMessageWorker&) = delete;
  DelayMessageWorker& operator=(const DelayMessageWorker&) = delete;

  ~DelayMessageWorker() override = default;

  void DispatchExtendableMessageEvent(
      blink::mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override {
    event_ = std::move(event);
    callback_ = std::move(callback);
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  void AbortMessageEvent() {
    std::move(callback_).Run(blink::mojom::ServiceWorkerEventStatus::ABORTED);
  }

  void RunUntilDispatchMessageEvent() {
    if (event_)
      return;
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  blink::mojom::ExtendableMessageEventPtr event_;
  DispatchExtendableMessageEventCallback callback_;
  base::OnceClosure quit_closure_;
};

TEST_P(ServiceWorkerVersionTest, RequestTimeout) {
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  auto* worker =
      helper_->AddNewPendingServiceWorker<DelayMessageWorker>(helper_.get());

  std::optional<blink::ServiceWorkerStatusCode> error_status;
  base::RunLoop run_loop;
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  client->UnblockStartWorker();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Create a request.
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      ReceiveServiceWorkerStatus(&error_status, run_loop.QuitClosure()));

  // Dispatch a dummy event.
  auto message_event = blink::mojom::ExtendableMessageEvent::New();
  message_event->message.sender_agent_cluster_id =
      base::UnguessableToken::Create();
  version_->endpoint()->DispatchExtendableMessageEvent(
      std::move(message_event),
      version_->CreateSimpleEventCallback(request_id));
  worker->RunUntilDispatchMessageEvent();

  // Request callback has not completed yet.
  EXPECT_FALSE(error_status);

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->SetAllRequestExpirations(base::TimeTicks::Now());
  version_->timeout_timer_.user_task().Run();

  // The renderer should have received a StopWorker request.
  client->RunUntilStopWorker();

  // The request should have timed out.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout,
            error_status.value());
  // Calling FinishRequest should be no-op, since the request timed out.
  EXPECT_FALSE(version_->FinishRequest(request_id, /*was_handled=*/true));

  // Simulate the renderer aborting the inflight event.
  // This should not crash: https://crbug.com/676984.
  worker->AbortMessageEvent();
  base::RunLoop().RunUntilIdle();

  // Simulate the renderer stopping the worker.
  client->UnblockStopWorker();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, RequestNowTimeout) {
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  auto* service_worker =
      helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get());
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Create a request that should expire Now().
  int request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()),
      base::TimeDelta(), ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status.value());

  service_worker->FlushForTesting();
  // Should try to set idle timeout if the last request has expired and is
  // CONTINUE_ON_TIMEOUT.
  EXPECT_TRUE(service_worker->idle_delay().has_value());

  EXPECT_FALSE(version_->FinishRequest(request_id, /*was_handled=*/true));

  // CONTINUE_ON_TIMEOUT timeouts don't stop the service worker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, RequestNowTimeoutKill) {
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Create a request that should expire Now().
  int request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()),
      base::TimeDelta(), ServiceWorkerVersion::KILL_ON_TIMEOUT);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status.value());

  EXPECT_FALSE(version_->FinishRequest(request_id, /*was_handled=*/true));

  // KILL_ON_TIMEOUT timeouts should stop the service worker.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, RequestCustomizedTimeout) {
  std::optional<blink::ServiceWorkerStatusCode> first_status;
  std::optional<blink::ServiceWorkerStatusCode> second_status;
  base::RunLoop first_run_loop;
  base::RunLoop second_run_loop;

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  SetupTestTickClock();

  // Create two requests. One which times out in 10 seconds, one in 20 seconds.
  int timeout_seconds = 10;
  int first_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      ReceiveServiceWorkerStatus(&first_status, first_run_loop.QuitClosure()),
      base::Seconds(2 * timeout_seconds),
      ServiceWorkerVersion::KILL_ON_TIMEOUT);

  int second_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      ReceiveServiceWorkerStatus(&second_status, second_run_loop.QuitClosure()),
      base::Seconds(timeout_seconds),
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  // The status should not have changed since neither task has timed out yet.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(first_status);
  EXPECT_FALSE(second_status);

  // Now advance time until the second task timeout should expire.
  tick_clock_.Advance(base::Seconds(timeout_seconds + 1));
  version_->timeout_timer_.user_task().Run();
  second_run_loop.Run();
  EXPECT_FALSE(first_status);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout,
            second_status.value());

  // CONTINUE_ON_TIMEOUT timeouts don't stop the service worker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());

  // Now advance time until both tasks should be expired.
  tick_clock_.Advance(base::Seconds(timeout_seconds + 1));
  version_->timeout_timer_.user_task().Run();
  first_run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout,
            first_status.value());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout,
            second_status.value());

  EXPECT_FALSE(version_->FinishRequest(first_request_id, /*was_handled=*/true));

  EXPECT_FALSE(
      version_->FinishRequest(second_request_id, /*was_handled=*/true));
  base::RunLoop().RunUntilIdle();

  // KILL_ON_TIMEOUT timeouts should stop the service worker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, MixedRequestTimeouts) {
  std::optional<blink::ServiceWorkerStatusCode> sync_status;
  std::optional<blink::ServiceWorkerStatusCode> fetch_status;
  base::RunLoop sync_run_loop;
  base::RunLoop fetch_run_loop;

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Create a fetch request that should expire sometime later.
  int fetch_request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      ReceiveServiceWorkerStatus(&fetch_status, fetch_run_loop.QuitClosure()));
  // Create a request that should expire Now().
  int sync_request_id = version_->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC,
      ReceiveServiceWorkerStatus(&sync_status, sync_run_loop.QuitClosure()),
      base::TimeDelta(), ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(sync_status);

  // Verify the sync has timed out but not the fetch.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->timeout_timer_.user_task().Run();
  sync_run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, sync_status.value());
  EXPECT_FALSE(fetch_status);

  // Background sync timeouts don't stop the service worker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());

  // Gracefully handle the sync event finishing after the timeout.
  EXPECT_FALSE(version_->FinishRequest(sync_request_id, /*was_handled=*/true));

  // Verify that the fetch times out later.
  version_->SetAllRequestExpirations(base::TimeTicks::Now());
  version_->timeout_timer_.user_task().Run();
  fetch_run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout,
            fetch_status.value());

  // Fetch request should no longer exist.
  EXPECT_FALSE(version_->FinishRequest(fetch_request_id, /*was_handled=*/true));
  base::RunLoop().RunUntilIdle();

  // Other timeouts do stop the service worker.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, FailToStart_RendererCrash) {
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_FALSE(status);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveStartingServiceWorker(
      version_->version_id()));

  // Simulate renderer crash: break EmbeddedWorkerInstance's Mojo connection to
  // the renderer-side client.
  client->Disconnect();
  run_loop.Run();
  // Callback completed.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed,
            status.value());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));
}

TEST_P(ServiceWorkerVersionTest, FailToStart_Timeout) {
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;

  // Start starting the worker.
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStopWorker();
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
  base::RunLoop().RunUntilIdle();

  // Callback has not completed yet.
  EXPECT_FALSE(status);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveStartingServiceWorker(
      version_->version_id()));

  // Simulate timeout.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->start_time_ = base::TimeTicks::Now() -
                          ServiceWorkerVersion::kStartNewWorkerTimeout -
                          base::Minutes(1);
  version_->timeout_timer_.user_task().Run();
  run_loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout, status.value());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));
}

// Test that a service worker stalled in stopping will timeout and not get in a
// sticky error state.
TEST_P(ServiceWorkerVersionTest, StallInStopping_DetachThenStart) {
  // Start a worker.
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStartWorker();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Try to stop the worker.
  bool has_stopped = false;
  base::RunLoop run_loop;
  version_->StopWorker(VerifyCalled(&has_stopped, run_loop.QuitClosure()));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping, version_->running_status());
  base::RunLoop().RunUntilIdle();

  // Worker is now stalled in stopping. Verify a fast timeout is in place.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_EQ(ServiceWorkerVersion::kStopWorkerTimeout,
            version_->timeout_timer_.GetCurrentDelay());

  // Simulate timeout.
  version_->stop_time_ = base::TimeTicks::Now() -
                         ServiceWorkerVersion::kStopWorkerTimeout -
                         base::Seconds(1);
  version_->timeout_timer_.user_task().Run();
  run_loop.Run();
  EXPECT_TRUE(has_stopped);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());

  // Try to start the worker again. It should work.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // The timeout interval should be reset to normal.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  EXPECT_EQ(ServiceWorkerVersion::kTimeoutTimerDelay,
            version_->timeout_timer_.GetCurrentDelay());
}

// Test that a service worker stalled in stopping with a start worker
// request queued up will timeout and restart.
TEST_P(ServiceWorkerVersionTest, StallInStopping_DetachThenRestart) {
  // Start a worker.
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStartWorker();
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  // Try to stop the worker.
  bool has_stopped = false;
  version_->StopWorker(VerifyCalled(&has_stopped));
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping, version_->running_status());

  // Worker is now stalled in stopping. Add a start worker request.
  std::optional<blink::ServiceWorkerStatusCode> start_status;
  base::RunLoop run_loop;
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&start_status, run_loop.QuitClosure()));

  // Simulate timeout. The worker should stop and get restarted.
  EXPECT_TRUE(version_->timeout_timer_.IsRunning());
  version_->stop_time_ = base::TimeTicks::Now() -
                         ServiceWorkerVersion::kStopWorkerTimeout -
                         base::Seconds(1);
  version_->timeout_timer_.user_task().Run();
  run_loop.Run();
  EXPECT_TRUE(has_stopped);
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, start_status.value());
}

TEST_P(ServiceWorkerVersionTest, RendererCrashDuringEvent) {
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  auto* client =
      helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
          helper_.get());
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));

  base::RunLoop loop;
  blink::ServiceWorkerStatusCode status = blink::ServiceWorkerStatusCode::kOk;
  int request_id = version_->StartRequest(
      ServiceWorkerMetrics::EventType::SYNC,
      base::BindOnce(
          [](base::OnceClosure done, blink::ServiceWorkerStatusCode* out_status,
             blink::ServiceWorkerStatusCode result_status) {
            *out_status = result_status;
            std::move(done).Run();
          },
          loop.QuitClosure(), &status));

  // Simulate renderer crash: break EmbeddedWorkerInstance's Mojo connection to
  // the renderer-side client. The request callback should be called.
  client->Disconnect();
  loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorFailed, status);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version_->running_status());
  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));

  // Request already failed, calling finish should return false.
  EXPECT_FALSE(version_->FinishRequest(request_id, /*was_handled=*/true));
}

TEST_P(ServiceWorkerVersionTest, PingController) {
  // Start starting an worker. Ping should not be active.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::DoNothing());
  EXPECT_FALSE(IsPingActivated(version_.get()));

  // Start script evaluation. Ping should be active.
  NotifyScriptEvaluationStart(version_.get());
  EXPECT_TRUE(IsPingActivated(version_.get()));

  // Finish starting the worker. Ping should still be active.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(IsPingActivated(version_.get()));
}

// Test starting a service worker from a disallowed origin.
TEST_P(ServiceWorkerVersionTest, BadOrigin) {
  // An https URL would have been allowed but http is not trustworthy.
  const GURL scope("http://www.example.com/test/");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;

  auto registration = CreateNewServiceWorkerRegistration(
      helper_->context()->registry(), options, GetTestStorageKey(scope));
  auto version = CreateNewServiceWorkerVersion(
      helper_->context()->registry(), registration_.get(),
      GURL("http://www.example.com/test/service_worker.js"),
      blink::mojom::ScriptType::kClassic);
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kErrorDisallowed,
            StartServiceWorker(version.get()));
}

TEST_P(ServiceWorkerVersionTest,
       ForegroundServiceWorkerCountUpdatedByControllee) {
  // Start the worker before we have a controllee.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  {
    // Add a controllee in a different process from the service worker.
    auto service_worker_client =
        ActivateWithControllee(/*in_different_process=*/true);

    // RenderProcessHost should be notified of foreground worker.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(
        1,
        helper_->mock_render_process_host()->foreground_service_worker_count());

    // Remove the controllee by scoping out `service_worker_client`.
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(version_->HasControllee());

  // RenderProcessHost should be notified that there are no foreground workers.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());
}

TEST_P(ServiceWorkerVersionTest,
       ForegroundServiceWorkerCountNotUpdatedBySameProcessControllee) {
  // Start the worker before we have a controllee.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Add a controllee in the same process as the service worker.
  auto service_worker_client = ActivateWithControllee();

  // RenderProcessHost should be notified of foreground worker.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());
}

TEST_P(ServiceWorkerVersionTest,
       ForegroundServiceWorkerCountUpdatedByControlleeProcessIdChange) {
  // Start the worker before we have a controllee.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);

  // Add a controllee, but don't begin the navigation commit yet.  This will
  // cause the client to have an invalid process id like we see in real
  // navigations.
  ScopedServiceWorkerClient service_worker_client =
      CreateServiceWorkerClient(helper_->context());
  service_worker_client->UpdateUrls(registration_->scope(),
                                    registration_->key().origin(),
                                    registration_->key());
  service_worker_client->SetControllerRegistration(
      registration_, false /* notify_controllerchange */);
  EXPECT_TRUE(version_->HasControllee());
  EXPECT_TRUE(service_worker_client->controller());

  // RenderProcessHost should be notified of foreground worker.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Establish a dummy connection to allow sending messages without errors.
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      reporter;
  auto dummy = reporter.InitWithNewPipeAndPassReceiver();

  // Now begin the navigation commit with the same process id used by the
  // worker. This should cause the worker to stop being considered foreground
  // priority.
  auto container_info = service_worker_client.CommitResponseAndRelease(
      GlobalRenderFrameHostId(version_->embedded_worker()->process_id(),
                              /*frame_routing_id=*/1),
      PolicyContainerPolicies(), std::move(reporter),
      ukm::UkmRecorder::GetNewSourceID());

  // RenderProcessHost should be notified of foreground worker.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());
}

TEST_P(ServiceWorkerVersionTest,
       ForegroundServiceWorkerCountUpdatedByWorkerStatus) {
  // Add a controllee in a different process from the service worker.
  auto service_worker_client =
      ActivateWithControllee(/*in_different_process=*/true);

  // RenderProcessHost should not be notified of foreground worker yet since
  // there is no worker running.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Starting the worker should notify the RenderProcessHost of the foreground
  // worker.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(
      1,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Stopping the worker should notify the RenderProcessHost that the foreground
  // worker has been removed.
  version_->StopWorker(base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());
}

TEST_P(ServiceWorkerVersionTest,
       ForegroundServiceWorkerCountUpdatedByControlleeForegroundStateChange) {
  // Start the worker before we have a controllee.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Add a controllee in a different process from the service worker.
  auto service_worker_client =
      ActivateWithControllee(/*in_different_process=*/true);

  // RenderProcessHost should be notified of foreground worker.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      1,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Set controllee process to background priority.
  client_render_process_hosts_[0]->set_priority(
      base::Process::Priority::kBestEffort);
  version_->UpdateForegroundPriority();

  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Set controllee process to foreground priority.
  client_render_process_hosts_[0]->set_priority(
      base::Process::Priority::kUserBlocking);
  version_->UpdateForegroundPriority();

  EXPECT_EQ(
      1,
      helper_->mock_render_process_host()->foreground_service_worker_count());
}

// An instance client whose fetch handler type is kNoHandler.
class NoFetchHandlerClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  explicit NoFetchHandlerClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}

  NoFetchHandlerClient(const NoFetchHandlerClient&) = delete;
  NoFetchHandlerClient& operator=(const NoFetchHandlerClient&) = delete;

  void EvaluateScript() override {
    host()->OnScriptEvaluationStart();
    host()->OnStarted(blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
                      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
                      /*has_hid_event_handlers=*/false,
                      /*has_usb_event_handlers=*/false,
                      helper()->GetNextThreadId(),
                      blink::mojom::EmbeddedWorkerStartTiming::New());
  }
};

class ServiceWorkerVersionNoFetchHandlerTest : public ServiceWorkerVersionTest {
 protected:
  void SetUp() override {
    ServiceWorkerVersionTest::SetUp();
    // Make the service worker says no handler.
    helper_->AddPendingInstanceClient(
        std::make_unique<NoFetchHandlerClient>(helper_.get()));
  }
  std::optional<ServiceWorkerVersion::FetchHandlerType> GetFetchHandlerType()
      const override {
    return ServiceWorkerVersion::FetchHandlerType::kNoHandler;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerVersionNoFetchHandlerTest,
    testing::ValuesIn({StorageKeyTestCase::kFirstParty,
                       StorageKeyTestCase::kThirdParty}),
    [](const testing::TestParamInfo<StorageKeyTestCase>& info) {
      switch (info.param) {
        case (StorageKeyTestCase::kFirstParty):
          return "FirstPartyStorageKey";
        case (StorageKeyTestCase::kThirdParty):
          return "ThirdPartyStorageKey";
      }
    });

TEST_P(ServiceWorkerVersionNoFetchHandlerTest,
       ForegroundServiceWorkerCountNotUpdated) {
  // Start the worker before we have a controllee.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());

  // Add a controllee in a different process from the service worker.
  auto service_worker_client =
      ActivateWithControllee(/*in_different_process=*/true);

  // RenderProcessHost should not be notified if the service worker does not
  // have a FetchEvent handler.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0,
      helper_->mock_render_process_host()->foreground_service_worker_count());
}

TEST_P(ServiceWorkerVersionTest, FailToStart_UseNewRendererProcess) {
  ServiceWorkerContextCore* context = helper_->context();
  int64_t id = version_->version_id();
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Start once. It should choose the "existing process".
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(helper_->mock_render_process_id(),
            version_->embedded_worker()->process_id());

  StopServiceWorker(version_.get());

  // Fail once.
  helper_->AddPendingInstanceClient(
      std::make_unique<FailStartInstanceClient>(helper_.get()));
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(1, context->GetVersionFailureCount(id));
  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(id));

  // Fail again.
  helper_->AddPendingInstanceClient(
      std::make_unique<FailStartInstanceClient>(helper_.get()));
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(2, context->GetVersionFailureCount(id));
  EXPECT_FALSE(helper_->context_wrapper()->IsLiveRunningServiceWorker(id));

  // Succeed. It should choose the "new process".
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(helper_->new_render_process_id(),
            version_->embedded_worker()->process_id());
  EXPECT_EQ(0, context->GetVersionFailureCount(id));
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(id));
  version_->StopWorker(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Start again. It should choose the "existing process" again as we no longer
  // force creation of a new process.
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  EXPECT_EQ(helper_->mock_render_process_id(),
            version_->embedded_worker()->process_id());
  version_->StopWorker(base::DoNothing());
  base::RunLoop().RunUntilIdle();
}

TEST_P(ServiceWorkerVersionTest, FailToStart_RestartStalledWorker) {
  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  // Stall in starting.
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStopWorker();
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::FETCH_MAIN_FRAME,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(status);

  // The restart logic is triggered because OnStopped is called before
  // OnStarted. So the Start message is sent again. The delayed instance client
  // was already consumed, so a default fake instance client will be created,
  // which starts normally.
  bool has_stopped = false;
  version_->StopWorker(VerifyCalled(&has_stopped));
  run_loop.Run();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
  EXPECT_TRUE(has_stopped);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
  EXPECT_TRUE(helper_->context_wrapper()->IsLiveRunningServiceWorker(
      version_->version_id()));
}

TEST_P(ServiceWorkerVersionTest, InstalledFetchEventHandlerExists) {
  auto* service_worker =
      helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get());
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  service_worker->RunUntilInitializeGlobalScope();
  EXPECT_EQ(FetchHandlerExistence::EXISTS,
            service_worker->fetch_handler_existence());
}

TEST_P(ServiceWorkerVersionNoFetchHandlerTest,
       InstalledFetchEventHandlerDoesNotExist) {
  auto* service_worker =
      helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get());
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  service_worker->RunUntilInitializeGlobalScope();
  EXPECT_EQ(FetchHandlerExistence::DOES_NOT_EXIST,
            service_worker->fetch_handler_existence());
}

class StoreMessageServiceWorker : public FakeServiceWorker {
 public:
  explicit StoreMessageServiceWorker(EmbeddedWorkerTestHelper* helper)
      : FakeServiceWorker(helper) {}
  ~StoreMessageServiceWorker() override = default;

  // Returns messages from AddMessageToConsole.
  const std::vector<std::pair<blink::mojom::ConsoleMessageLevel, std::string>>&
  console_messages() {
    return console_messages_;
  }

  void SetAddMessageToConsoleReceivedCallback(
      const base::RepeatingClosure& closure) {
    add_message_to_console_callback_ = closure;
  }

 private:
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message) override {
    console_messages_.emplace_back(level, message);
    if (add_message_to_console_callback_)
      add_message_to_console_callback_.Run();
  }

  std::vector<std::pair<blink::mojom::ConsoleMessageLevel, std::string>>
      console_messages_;
  base::RepeatingClosure add_message_to_console_callback_;
};

TEST_P(ServiceWorkerVersionTest, AddMessageToConsole) {
  auto* service_worker =
      helper_->AddNewPendingServiceWorker<StoreMessageServiceWorker>(
          helper_.get());

  // Attempt to start the worker and immediate AddMessageToConsole should not
  // cause a crash.
  std::pair<blink::mojom::ConsoleMessageLevel, std::string> test_message =
      std::make_pair(blink::mojom::ConsoleMessageLevel::kVerbose, "");
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version_.get()));
  version_->AddMessageToConsole(test_message.first, test_message.second);
  service_worker->RunUntilInitializeGlobalScope();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());

  // Messages sent before sending StartWorker message won't be dispatched.
  ASSERT_EQ(0UL, service_worker->console_messages().size());

  // Messages sent after sending StartWorker message should be reached to
  // the renderer.
  base::RunLoop loop;
  service_worker->SetAddMessageToConsoleReceivedCallback(loop.QuitClosure());
  version_->AddMessageToConsole(test_message.first, test_message.second);
  loop.Run();
  ASSERT_EQ(1UL, service_worker->console_messages().size());
  EXPECT_EQ(test_message, service_worker->console_messages()[0]);
}

// Test that writing metadata aborts gracefully when a remote connection to
// the Storage Service is disconnected.
TEST_P(ServiceWorkerVersionTest, WriteMetadata_RemoteStorageDisconnection) {
  const std::string kMetadata("Test metadata");

  net::TestCompletionCallback completion;
  version_->script_cache_map()->WriteMetadata(
      version_->script_url(), base::as_bytes(base::make_span(kMetadata)),
      completion.callback());

  helper_->SimulateStorageRestartForTesting();

  ASSERT_EQ(completion.WaitForResult(), net::ERR_FAILED);
}

// Test that writing metadata aborts gracefully when the storage is disabled.
TEST_P(ServiceWorkerVersionTest, WriteMetadata_StorageDisabled) {
  const std::string kMetadata("Test metadata");

  base::RunLoop loop;
  helper_->context()->registry()->DisableStorageForTesting(loop.QuitClosure());
  loop.Run();

  net::TestCompletionCallback completion;
  version_->script_cache_map()->WriteMetadata(
      version_->script_url(), base::as_bytes(base::make_span(kMetadata)),
      completion.callback());

  ASSERT_EQ(completion.WaitForResult(), net::ERR_FAILED);
}

// Test that writing metadata twice at the same time finishes successfully.
TEST_P(ServiceWorkerVersionTest, WriteMetadata_MultipleWrites) {
  const std::string kMetadata("Test metadata");

  net::TestCompletionCallback completion1;
  version_->script_cache_map()->WriteMetadata(
      version_->script_url(), base::as_bytes(base::make_span(kMetadata)),
      completion1.callback());
  net::TestCompletionCallback completion2;
  version_->script_cache_map()->WriteMetadata(
      version_->script_url(), base::as_bytes(base::make_span(kMetadata)),
      completion2.callback());

  ASSERT_EQ(completion1.WaitForResult(), static_cast<int>(kMetadata.size()));
  ASSERT_EQ(completion2.WaitForResult(), static_cast<int>(kMetadata.size()));
}

// Tests that adding pending external requests with different
// ServiceWorkerExternalRequestTimeoutType is handled correctly within
// ServiceWorkerVersion::pending_external_requests_.
TEST_P(ServiceWorkerVersionTest, PendingExternalRequest) {
  using TimeoutType = ServiceWorkerExternalRequestTimeoutType;
  using Result = ServiceWorkerExternalRequestResult;
  auto get_pending_request_size = [&]() -> size_t {
    return version_->pending_external_requests_.size();
  };

  std::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop run_loop;
  version_->StartWorker(
      ServiceWorkerMetrics::EventType::UNKNOWN,
      ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
  ASSERT_EQ(blink::EmbeddedWorkerStatus::kStarting, version_->running_status());

  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  base::Uuid uuid2 = base::Uuid::GenerateRandomV4();

  // Test adding request with |uuid1| and different TimeoutType-s.
  EXPECT_EQ(Result::kOk,
            version_->StartExternalRequest(uuid1, TimeoutType::kDefault));
  EXPECT_EQ(1u, get_pending_request_size());
  // |uuid1| already exists, with same or different TimeoutType.
  EXPECT_EQ(Result::kBadRequestId, version_->StartExternalRequest(
                                       uuid1, TimeoutType::kDoesNotTimeout));
  EXPECT_EQ(Result::kBadRequestId,
            version_->StartExternalRequest(uuid1, TimeoutType::kDefault));
  EXPECT_EQ(1u, get_pending_request_size());
  // |uuid2| does not exist yet.
  EXPECT_EQ(Result::kBadRequestId, version_->FinishExternalRequest(uuid2));

  // Test adding request with |uuid2|.
  EXPECT_EQ(Result::kOk, version_->StartExternalRequest(
                             uuid2, TimeoutType::kDoesNotTimeout));
  EXPECT_EQ(2u, get_pending_request_size());
  EXPECT_EQ(Result::kOk, version_->FinishExternalRequest(uuid1));
  EXPECT_EQ(Result::kBadRequestId, version_->FinishExternalRequest(uuid1));

  run_loop.Run();
}

// Tests worker lifetime with ServiceWorkerVersion::StartExternalRequest.
TEST_P(ServiceWorkerVersionTest, WorkerLifetimeWithExternalRequest) {
  SetupTestTickClock();
  std::optional<blink::ServiceWorkerStatusCode> status;

  auto start_external_request_test =
      [&](ServiceWorkerExternalRequestTimeoutType timeout_type,
          bool expect_running) {
        SCOPED_TRACE(testing::Message()
                     << std::boolalpha << "expect_running: " << expect_running);
        {
          // Start worker.
          base::RunLoop run_loop;
          version_->StartWorker(
              ServiceWorkerMetrics::EventType::UNKNOWN,
              ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
          ASSERT_EQ(blink::EmbeddedWorkerStatus::kStarting,
                    version_->running_status());

          // Add an external request.
          EXPECT_EQ(ServiceWorkerExternalRequestResult::kOk,
                    version_->StartExternalRequest(
                        base::Uuid::GenerateRandomV4(), timeout_type));
          run_loop.Run();
          EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
          EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
                    version_->running_status());
        }

        // Now advance time to check worker's running state.
        tick_clock_.Advance(kTestTimeoutBeyondRequestTimeout);
        version_->timeout_timer_.user_task().Run();
        base::RunLoop().RunUntilIdle();

        version_->OnPongFromWorker();  // Avoids ping timeout.
        const bool worker_stopped_or_stopping =
            version_->OnRequestTermination();

        EXPECT_EQ(!expect_running, worker_stopped_or_stopping);
        const bool worker_running =
            version_->running_status() == blink::EmbeddedWorkerStatus::kRunning;
        EXPECT_EQ(expect_running, worker_running);

        // Ensure the worker is stopped, so that start_external_request_test()
        // works next time.
        {
          bool has_stopped = false;
          base::RunLoop run_loop;
          version_->StopWorker(
              VerifyCalled(&has_stopped, run_loop.QuitClosure()));
          run_loop.Run();
          EXPECT_TRUE(has_stopped);
          EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped,
                    version_->running_status());
        }
      };

  start_external_request_test(ServiceWorkerExternalRequestTimeoutType::kDefault,
                              // External request with kDefault timeout stops
                              // the worker after default timeout.
                              false /* expect_running */);
  start_external_request_test(
      ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout,
      // External request with kDoesNotTimeout timeout keeps the worker running
      // beyond the default timeout.
      true /* expect_running */);
}

// Tests that a worker containing external request with
// ServiceWorkerExternalRequestTimeoutType::kDoesNotTimeout is not
// stopped by an external request with "default" timeout.
//
// Regression test for https://crbug.com/1189678
TEST_P(ServiceWorkerVersionTest,
       DefaultTimeoutRequestDoesNotAffectMaxTimeoutRequest) {
  SetupTestTickClock();
  std::optional<blink::ServiceWorkerStatusCode> status;

  using ReqTimeoutType = ServiceWorkerExternalRequestTimeoutType;
  {
    // Start worker.
    base::RunLoop run_loop;
    version_->StartWorker(
        ServiceWorkerMetrics::EventType::UNKNOWN,
        ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
    ASSERT_EQ(blink::EmbeddedWorkerStatus::kStarting,
              version_->running_status());

    // Add an external request, with kDoesNotTimeout timeout.
    EXPECT_EQ(ServiceWorkerExternalRequestResult::kOk,
              version_->StartExternalRequest(base::Uuid::GenerateRandomV4(),
                                             ReqTimeoutType::kDoesNotTimeout));
    run_loop.Run();
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());
    EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
              version_->running_status());

    // Add another external request with kDefault timeout.
    EXPECT_EQ(ServiceWorkerExternalRequestResult::kOk,
              version_->StartExternalRequest(base::Uuid::GenerateRandomV4(),
                                             ReqTimeoutType::kDefault));
  }

  // Now advance time to check worker's running state.
  tick_clock_.Advance(kTestTimeoutBeyondRequestTimeout);
  version_->timeout_timer_.user_task().Run();
  version_->OnPongFromWorker();  // Avoids ping timeout.
  base::RunLoop().RunUntilIdle();

  // Expect the worker to be still running.
  const bool worker_stopped_or_stopping = version_->OnRequestTermination();
  EXPECT_FALSE(worker_stopped_or_stopping);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version_->running_status());
}

TEST_P(ServiceWorkerVersionTest, SetResources) {
  // Create a new version
  scoped_refptr<ServiceWorkerVersion> version = CreateNewServiceWorkerVersion(
      helper_->context()->registry(), registration_.get(),
      GURL("https://www.example.com/test/service_worker.js"),
      blink::mojom::ScriptType::kClassic);

  // The checksum is empty because still no resource records.
  EXPECT_FALSE(version->sha256_script_checksum());

  // Set resource records.
  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
  records.push_back(WriteToDiskCacheWithIdSync(
      helper_->context()->GetStorageControl(), version->script_url(), 10,
      {} /* headers */, "I'm a body", "I'm a meta data"));

  // Set fetch_handler_type, which is refereed in SetResources().
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  version->SetResources(records);

  // The checksum has been calculated after the SetResources.
  EXPECT_EQ("CBE5CFDF7C2118A9C3D78EF1D684F3AFA089201352886449A06A6511CFEF74A7",
            version->sha256_script_checksum());
}

class ServiceWorkerVersionStaticRouterTest : public ServiceWorkerVersionTest {
 public:
  ServiceWorkerVersionStaticRouterTest() {
    feature_list_.InitWithFeatures({features::kServiceWorkerStaticRouter}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerVersionStaticRouterTest,
    testing::ValuesIn({StorageKeyTestCase::kFirstParty,
                       StorageKeyTestCase::kThirdParty}),
    [](const testing::TestParamInfo<StorageKeyTestCase>& info) {
      switch (info.param) {
        case (StorageKeyTestCase::kFirstParty):
          return "FirstPartyStorageKey";
        case (StorageKeyTestCase::kThirdParty):
          return "ThirdPartyStorageKey";
      }
    });

TEST_P(ServiceWorkerVersionStaticRouterTest, SetRouterEvaluator) {
  // Create a new version
  scoped_refptr<ServiceWorkerVersion> version = CreateNewServiceWorkerVersion(
      helper_->context()->registry(), registration_.get(),
      GURL("https://www.example.com/test/service_worker.js"),
      blink::mojom::ScriptType::kClassic);
  version->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);

  // The router_evaluator should be unset on setup.
  EXPECT_FALSE(version->router_evaluator());

  // Leave the router_evaluator unset for invalid rules.
  {
    // No condition & source rule is invalid.
    blink::ServiceWorkerRouterRules rules;
    blink::ServiceWorkerRouterRule rule;
    rules.rules.emplace_back(rule);
    EXPECT_NE(version->SetupRouterEvaluator(rules),
              ServiceWorkerRouterEvaluatorErrorEnums::kNoError);
    EXPECT_FALSE(version->router_evaluator());
  }

  // Set correct rules will make the router_evaluator() return non-null.
  {
    blink::ServiceWorkerRouterRules rules;
    blink::ServiceWorkerRouterRule rule;
    rule.condition = blink::ServiceWorkerRouterCondition::WithRunningStatus(
        {blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
             kRunning});
    blink::ServiceWorkerRouterSource source;
    source.type = network::mojom::ServiceWorkerRouterSourceType::kNetwork;
    source.network_source = blink::ServiceWorkerRouterNetworkSource{};
    rule.sources.emplace_back(source);
    rules.rules.emplace_back(rule);
    EXPECT_EQ(version->SetupRouterEvaluator(rules),
              ServiceWorkerRouterEvaluatorErrorEnums::kNoError);
    EXPECT_TRUE(version->router_evaluator());
  }

  // SetupRouterEvaluator() will merge existing rules if router_evaluator()
  // already exists.
  {
    EXPECT_TRUE(version->router_evaluator());
    EXPECT_EQ(version->router_evaluator()->rules().rules.size(), 1UL);
    blink::ServiceWorkerRouterRules rules;
    blink::ServiceWorkerRouterRule rule;
    rule.condition = blink::ServiceWorkerRouterCondition::WithRunningStatus(
        {blink::ServiceWorkerRouterRunningStatusCondition::RunningStatusEnum::
             kNotRunning});
    blink::ServiceWorkerRouterSource source;
    source.type = network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
    source.fetch_event_source = blink::ServiceWorkerRouterFetchEventSource{};
    rule.sources.emplace_back(source);
    rules.rules.emplace_back(rule);
    EXPECT_EQ(version->SetupRouterEvaluator(rules),
              ServiceWorkerRouterEvaluatorErrorEnums::kNoError);
    EXPECT_TRUE(version->router_evaluator());
    EXPECT_EQ(version->router_evaluator()->rules().rules.size(), 2UL);
    auto first_rule = version->router_evaluator()->rules().rules[0];
    auto second_rule = version->router_evaluator()->rules().rules[1];
    auto&& [first_url_pattern, first_request, first_running_status,
            first_or_condition, first_not_condition] =
        first_rule.condition.get();
    EXPECT_EQ(first_running_status->status,
              blink::ServiceWorkerRouterRunningStatusCondition::
                  RunningStatusEnum::kRunning);
    EXPECT_EQ(first_rule.sources.begin()->type,
              network::mojom::ServiceWorkerRouterSourceType::kNetwork);
    auto&& [second_url_pattern, second_request, second_running_status,
            second_or_condition, second_not_condition] =
        second_rule.condition.get();
    EXPECT_EQ(second_running_status->status,
              blink::ServiceWorkerRouterRunningStatusCondition::
                  RunningStatusEnum::kNotRunning);
    EXPECT_EQ(second_rule.sources.begin()->type,
              network::mojom::ServiceWorkerRouterSourceType::kFetchEvent);
  }
}

// An instance client for controlling whether it has hid event handlers or not.
class HidEventHandlerClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  HidEventHandlerClient(EmbeddedWorkerTestHelper* helper,
                        bool has_hid_event_handlers)
      : FakeEmbeddedWorkerInstanceClient(helper),
        has_hid_event_handlers_(has_hid_event_handlers) {}

  HidEventHandlerClient(const NoFetchHandlerClient&) = delete;
  HidEventHandlerClient& operator=(const NoFetchHandlerClient&) = delete;

  void EvaluateScript() override {
    host()->OnScriptEvaluationStart();
    host()->OnStarted(
        blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
        blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable,
        has_hid_event_handlers_, /*has_usb_event_handlers_=*/false,
        helper()->GetNextThreadId(),
        blink::mojom::EmbeddedWorkerStartTiming::New());
  }

 private:
  bool has_hid_event_handlers_;
};

TEST_P(ServiceWorkerVersionTest, HasHidEventHandler) {
  helper_->AddNewPendingInstanceClient<HidEventHandlerClient>(
      helper_.get(), /*has_hid_event_handlers*/ true);
  StartServiceWorker(version_.get());
  EXPECT_TRUE(version_->has_hid_event_handlers());
}

TEST_P(ServiceWorkerVersionTest, NoHidEventHandler) {
  helper_->AddNewPendingInstanceClient<HidEventHandlerClient>(
      helper_.get(), /*has_hid_event_handlers*/ false);
  StartServiceWorker(version_.get());
  EXPECT_FALSE(version_->has_hid_event_handlers());
}

// An instance client for controlling whether it has hid event handlers or not.
class UsbEventHandlerClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  UsbEventHandlerClient(EmbeddedWorkerTestHelper* helper,
                        bool has_usb_event_handlers)
      : FakeEmbeddedWorkerInstanceClient(helper),
        has_usb_event_handlers_(has_usb_event_handlers) {}

  UsbEventHandlerClient(const NoFetchHandlerClient&) = delete;
  UsbEventHandlerClient& operator=(const NoFetchHandlerClient&) = delete;

  void EvaluateScript() override {
    host()->OnScriptEvaluationStart();
    host()->OnStarted(
        blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
        blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable,
        /*has_hid_event_handlers_=*/false, has_usb_event_handlers_,
        helper()->GetNextThreadId(),
        blink::mojom::EmbeddedWorkerStartTiming::New());
  }

 private:
  bool has_usb_event_handlers_;
};

TEST_P(ServiceWorkerVersionTest, HasUsbEventHandler) {
  helper_->AddNewPendingInstanceClient<UsbEventHandlerClient>(
      helper_.get(), /*has_usb_event_handlers*/ true);
  StartServiceWorker(version_.get());
  EXPECT_TRUE(version_->has_usb_event_handlers());
}

TEST_P(ServiceWorkerVersionTest, NoUsbEventHandler) {
  helper_->AddNewPendingInstanceClient<UsbEventHandlerClient>(
      helper_.get(), /*has_usb_event_handlers*/ false);
  StartServiceWorker(version_.get());
  EXPECT_FALSE(version_->has_usb_event_handlers());
}

}  // namespace service_worker_version_unittest
}  // namespace content

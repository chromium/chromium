// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/test/embedded_worker_instance_test_harness.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"

namespace content {

namespace {

using RegistrationAndVersionPair =
    EmbeddedWorkerTestHelper::RegistrationAndVersionPair;

}  // namespace

class EmbeddedWorkerInstanceTest : public EmbeddedWorkerInstanceTestHarness,
                                   public EmbeddedWorkerInstance::Listener {
 public:
  EmbeddedWorkerInstanceTest()
      : EmbeddedWorkerInstanceTestHarness(BrowserTaskEnvironment::IO_MAINLOOP) {
  }

 protected:
  enum EventType {
    PROCESS_ALLOCATED,
    START_WORKER_MESSAGE_SENT,
    STARTED,
    STOPPED,
    DETACHED,
  };

  struct EventLog {
    EventType type;
    std::optional<blink::EmbeddedWorkerStatus> status;
    std::optional<blink::mojom::ServiceWorkerStartStatus> start_status;
  };

  void RecordEvent(
      EventType type,
      std::optional<blink::EmbeddedWorkerStatus> status = std::nullopt,
      std::optional<blink::mojom::ServiceWorkerStartStatus> start_status =
          std::nullopt) {
    EventLog log = {type, status, start_status};
    events_.push_back(log);
  }

  void OnProcessAllocated() override { RecordEvent(PROCESS_ALLOCATED); }
  void OnStartWorkerMessageSent() override {
    RecordEvent(START_WORKER_MESSAGE_SENT);
  }
  void OnStarted(blink::mojom::ServiceWorkerStartStatus status,
                 blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type,
                 bool has_hid_event_handlers,
                 bool has_usb_event_handlers) override {
    fetch_handler_type_ = fetch_handler_type;
    RecordEvent(STARTED, std::nullopt, status);
  }
  void OnStopped(blink::EmbeddedWorkerStatus old_status) override {
    RecordEvent(STOPPED, old_status);
  }
  void OnDetached(blink::EmbeddedWorkerStatus old_status) override {
    RecordEvent(DETACHED, old_status);
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  std::vector<EventLog> events_;
  blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type_;
};

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
  worker->AddObserver(this);

  // Start should succeed.
  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));

  // The 'WorkerStarted' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());

  // Stop the worker.
  worker->Stop();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping, worker->status());
  base::RunLoop().RunUntilIdle();

  // The 'WorkerStopped' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());

  // Check if the IPCs are fired in expected order.
  ASSERT_EQ(4u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);
  EXPECT_EQ(blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
            events_[2].start_status.value());
  EXPECT_EQ(STOPPED, events_[3].type);
}

// Test that a worker that failed twice will use a new render process
// on the next attempt.
TEST_F(EmbeddedWorkerInstanceTest, ForceNewProcess) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  const int64_t service_worker_version_id = pair.second->version_id();
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());

  {
    // Start once normally.
    helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));
    // The worker should be using the default render process.
    EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());

    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }

  // Fail twice.
  context()->UpdateVersionFailureCount(
      service_worker_version_id, blink::ServiceWorkerStatusCode::kErrorFailed);
  context()->UpdateVersionFailureCount(
      service_worker_version_id, blink::ServiceWorkerStatusCode::kErrorFailed);

  {
    // Start again.
    base::RunLoop run_loop;
    helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));

    // The worker should be using the new render process.
    EXPECT_EQ(helper_->new_render_process_id(), worker->process_id());
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(EmbeddedWorkerInstanceTest, StopWhenDevToolsAttached) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());

  // Start the worker and then call StopIfNotAttachedToDevTools().
  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfNotAttachedToDevTools();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping, worker->status());
  base::RunLoop().RunUntilIdle();

  // The worker must be stopped now.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());

  // Set devtools_attached to true, and do the same.
  worker->SetDevToolsAttached(true);

  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfNotAttachedToDevTools();
  base::RunLoop().RunUntilIdle();

  // The worker must not be stopped this time.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, worker->status());

  // Calling Stop() actually stops the worker regardless of whether devtools
  // is attached or not.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
}

TEST_F(EmbeddedWorkerInstanceTest, DetachAfterSendingStartWorkerMessage) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStopWorker();
  helper_->StartWorkerUntilStartSent(worker.get(),
                                     helper_->CreateStartParams(pair.second));
  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Detach();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, events_[0].status.value());
}

TEST_F(EmbeddedWorkerInstanceTest, StopAfterSendingStartWorkerMessage) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStopWorker();

  helper_->StartWorkerUntilStartSent(worker.get(),
                                     helper_->CreateStartParams(pair.second));
  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(STOPPED, events_[0].type);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopping, events_[0].status.value());
  events_.clear();

  // Restart the worker.
  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));

  // The worker should be started.
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

TEST_F(EmbeddedWorkerInstanceTest, Detach) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  // Start the worker.
  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));

  // Detach.
  worker->Detach();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
}

// Test for when sending the start IPC failed.
TEST_F(EmbeddedWorkerInstanceTest, FailToSendStartIPC) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Let StartWorker fail; mojo IPC fails to connect to a remote interface.
  helper_->AddPendingInstanceClient(nullptr);

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Attempt to start the worker. From the browser process's point of view, the
  // start IPC was sent.
  base::test::TestFuture<blink::ServiceWorkerStatusCode> future;
  worker->Start(helper_->CreateStartParams(pair.second), future.GetCallback());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, future.Get());

  // But the renderer should not receive the message and the binding is broken.
  // Worker should handle the failure of binding on the remote side as detach.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, events_[2].status.value());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
}

TEST_F(EmbeddedWorkerInstanceTest, RemoveRemoteInterface) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Attempt to start the worker.
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  base::test::TestFuture<blink::ServiceWorkerStatusCode> future;
  worker->Start(helper_->CreateStartParams(pair.second), future.GetCallback());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, future.Get());

  // Disconnect the Mojo connection. Worker should handle the sudden shutdown as
  // detach.
  client->RunUntilBound();
  client->Disconnect();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStarting, events_[2].status.value());
}

class RecordCacheStorageInstanceClient
    : public FakeEmbeddedWorkerInstanceClient {
 public:
  explicit RecordCacheStorageInstanceClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}
  ~RecordCacheStorageInstanceClient() override = default;

  void StartWorker(
      blink::mojom::EmbeddedWorkerStartParamsPtr start_params) override {
    had_cache_storage_ = start_params->provider_info->cache_storage &&
                         start_params->provider_info->cache_storage.is_valid();
    FakeEmbeddedWorkerInstanceClient::StartWorker(std::move(start_params));
  }

  bool had_cache_storage() const { return had_cache_storage_; }

 private:
  bool had_cache_storage_ = false;
};

// Test that the worker is given a CacheStoragePtr during startup.
TEST_F(EmbeddedWorkerInstanceTest, CacheStorageOptimization) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  // We should set COEP, or cache storage pipe won't be made.
  pair.second->set_policy_container_host(
      base::MakeRefCounted<PolicyContainerHost>());
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  // Start the worker.
  auto* client =
      helper_->AddNewPendingInstanceClient<RecordCacheStorageInstanceClient>(
          helper_.get());
  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));

  // Cache storage should have been sent.
  EXPECT_TRUE(client->had_cache_storage());

  // Stop the worker.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
}

// Starts the worker with kAbruptCompletion status.
class AbruptCompletionInstanceClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  explicit AbruptCompletionInstanceClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}
  ~AbruptCompletionInstanceClient() override = default;

 protected:
  void EvaluateScript() override {
    host()->OnScriptEvaluationStart();
    host()->OnStarted(blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion,
                      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
                      /*has_hid_event_handlers=*/false,
                      /*has_usb_event_handlers=*/false,
                      helper()->GetNextThreadId(),
                      blink::mojom::EmbeddedWorkerStartTiming::New());
  }
};

// Tests that kAbruptCompletion is the OnStarted() status when the
// renderer reports abrupt completion.
TEST_F(EmbeddedWorkerInstanceTest, AbruptCompletion) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  RegistrationAndVersionPair pair =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker->status());
  worker->AddObserver(this);

  helper_->AddPendingInstanceClient(
      std::make_unique<AbruptCompletionInstanceClient>(helper_.get()));
  helper_->StartWorker(worker.get(), helper_->CreateStartParams(pair.second));

  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);
  EXPECT_EQ(blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion,
            events_[2].start_status.value());
  worker->Stop();
}

// A fake instance client for toggling whether a fetch event handler exists.
class FetchHandlerInstanceClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  explicit FetchHandlerInstanceClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}
  ~FetchHandlerInstanceClient() override = default;

  void set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type) {
    fetch_handler_type_ = fetch_handler_type;
  }

 protected:
  void EvaluateScript() override {
    host()->OnScriptEvaluationStart();
    host()->OnStarted(blink::mojom::ServiceWorkerStartStatus::kNormalCompletion,
                      fetch_handler_type_, /*has_hid_event_handlers=*/false,
                      /*has_usb_event_handlers=*/false,
                      helper()->GetNextThreadId(),
                      blink::mojom::EmbeddedWorkerStartTiming::New());
  }

 private:
  blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type_ =
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler;
};

// Tests that whether a fetch event handler exists.
TEST_F(EmbeddedWorkerInstanceTest, HasFetchHandler) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  RegistrationAndVersionPair pair1 =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker1 = std::make_unique<EmbeddedWorkerInstance>(pair1.second.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker1->status());
  worker1->AddObserver(this);

  auto* fetch_handler_worker =
      helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
          helper_.get());
  fetch_handler_worker->set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable);
  helper_->StartWorker(worker1.get(), helper_->CreateStartParams(pair1.second));

  EXPECT_NE(blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
            fetch_handler_type_);
  worker1->Stop();

  RegistrationAndVersionPair pair2 =
      helper_->PrepareRegistrationAndVersion(scope, url);
  auto worker2 = std::make_unique<EmbeddedWorkerInstance>(pair2.second.get());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, worker2->status());
  worker2->AddObserver(this);

  auto* no_fetch_handler_worker =
      helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
          helper_.get());
  no_fetch_handler_worker->set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler);
  helper_->StartWorker(worker2.get(), helper_->CreateStartParams(pair2.second));

  EXPECT_EQ(blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
            fetch_handler_type_);
  worker2->Stop();
}

}  // namespace content

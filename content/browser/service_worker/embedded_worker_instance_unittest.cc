// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/policy_container_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

EmbeddedWorkerInstance::StatusCallback ReceiveStatus(
    absl::optional<blink::ServiceWorkerStatusCode>* out_status,
    base::OnceClosure quit) {
  return base::BindOnce(
      [](absl::optional<blink::ServiceWorkerStatusCode>* out_status,
         base::OnceClosure quit, blink::ServiceWorkerStatusCode status) {
        *out_status = status;
        std::move(quit).Run();
      },
      out_status, std::move(quit));
}

}  // namespace

class EmbeddedWorkerInstanceTest : public testing::Test,
                                   public EmbeddedWorkerInstance::Listener {
 public:
  EmbeddedWorkerInstanceTest(const EmbeddedWorkerInstanceTest&) = delete;
  EmbeddedWorkerInstanceTest& operator=(const EmbeddedWorkerInstanceTest&) =
      delete;

 protected:
  EmbeddedWorkerInstanceTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  enum EventType {
    PROCESS_ALLOCATED,
    START_WORKER_MESSAGE_SENT,
    STARTED,
    STOPPED,
    DETACHED,
  };

  struct EventLog {
    EventType type;
    absl::optional<EmbeddedWorkerStatus> status;
    absl::optional<blink::mojom::ServiceWorkerStartStatus> start_status;
  };

  void RecordEvent(EventType type,
                   absl::optional<EmbeddedWorkerStatus> status = absl::nullopt,
                   absl::optional<blink::mojom::ServiceWorkerStartStatus>
                       start_status = absl::nullopt) {
    EventLog log = {type, status, start_status};
    events_.push_back(log);
  }

  void OnProcessAllocated() override { RecordEvent(PROCESS_ALLOCATED); }
  void OnStartWorkerMessageSent() override {
    RecordEvent(START_WORKER_MESSAGE_SENT);
  }
  void OnStarted(
      blink::mojom::ServiceWorkerStartStatus status,
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type) override {
    fetch_handler_type_ = fetch_handler_type;
    RecordEvent(STARTED, absl::nullopt, status);
  }
  void OnStopped(EmbeddedWorkerStatus old_status) override {
    RecordEvent(STOPPED, old_status);
  }
  void OnDetached(EmbeddedWorkerStatus old_status) override {
    RecordEvent(DETACHED, old_status);
  }

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
  }

  void TearDown() override { helper_.reset(); }

  using RegistrationAndVersionPair =
      std::pair<scoped_refptr<ServiceWorkerRegistration>,
                scoped_refptr<ServiceWorkerVersion>>;

  RegistrationAndVersionPair PrepareRegistrationAndVersion(
      const GURL& scope,
      const GURL& script_url) {
    RegistrationAndVersionPair pair;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    pair.first = CreateNewServiceWorkerRegistration(
        context()->registry(), options,
        blink::StorageKey(url::Origin::Create(scope)));
    pair.second = CreateNewServiceWorkerVersion(
        context()->registry(), pair.first, script_url,
        blink::mojom::ScriptType::kClassic);
    return pair;
  }

  // Calls worker->Start() and runs until the start IPC is sent.
  //
  // Expects success. For failure cases, call Start() manually.
  void StartWorkerUntilStartSent(
      EmbeddedWorkerInstance* worker,
      blink::mojom::EmbeddedWorkerStartParamsPtr params) {
    absl::optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop loop;
    worker->Start(std::move(params),
                  ReceiveStatus(&status, loop.QuitClosure()));
    loop.Run();
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
  }

  // Calls worker->Start() and runs until startup finishes.
  //
  // Expects success. For failure cases, call Start() manually.
  void StartWorker(EmbeddedWorkerInstance* worker,
                   blink::mojom::EmbeddedWorkerStartParamsPtr params) {
    StartWorkerUntilStartSent(worker, std::move(params));
    // TODO(falken): Listen for OnStarted() instead of this.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
  }

  blink::mojom::EmbeddedWorkerStartParamsPtr CreateStartParams(
      scoped_refptr<ServiceWorkerVersion> version) {
    auto params = blink::mojom::EmbeddedWorkerStartParams::New();
    params->service_worker_version_id = version->version_id();
    params->scope = version->scope();
    params->script_url = version->script_url();
    params->is_installed = false;

    params->service_worker_receiver = CreateServiceWorker(version);
    params->controller_receiver = CreateController();
    params->installed_scripts_info = GetInstalledScriptsInfoPtr();
    params->provider_info = CreateProviderInfo(std::move(version));
    params->policy_container = CreateStubPolicyContainer();
    return params;
  }

  blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo(
      scoped_refptr<ServiceWorkerVersion> version) {
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
    version->worker_host_ = std::make_unique<ServiceWorkerHost>(
        provider_info->host_remote.InitWithNewEndpointAndPassReceiver(),
        version.get(), context()->AsWeakPtr());
    return provider_info;
  }

  mojo::PendingReceiver<blink::mojom::ServiceWorker> CreateServiceWorker(
      scoped_refptr<ServiceWorkerVersion> version) {
    version->service_worker_remote_.reset();
    return version->service_worker_remote_.BindNewPipeAndPassReceiver();
  }

  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
  CreateController() {
    controllers_.emplace_back();
    return controllers_.back().BindNewPipeAndPassReceiver();
  }

  void SetWorkerStatus(EmbeddedWorkerInstance* worker,
                       EmbeddedWorkerStatus status) {
    worker->status_ = status;
  }

  blink::mojom::ServiceWorkerInstalledScriptsInfoPtr
  GetInstalledScriptsInfoPtr() {
    installed_scripts_managers_.emplace_back();
    auto info = blink::mojom::ServiceWorkerInstalledScriptsInfo::New();
    info->manager_receiver =
        installed_scripts_managers_.back().BindNewPipeAndPassReceiver();
    installed_scripts_manager_host_receivers_.push_back(
        info->manager_host_remote.InitWithNewPipeAndPassReceiver());
    return info;
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  // Mojo endpoints.
  std::vector<mojo::Remote<blink::mojom::ControllerServiceWorker>> controllers_;
  std::vector<mojo::Remote<blink::mojom::ServiceWorkerInstalledScriptsManager>>
      installed_scripts_managers_;
  std::vector<mojo::PendingReceiver<
      blink::mojom::ServiceWorkerInstalledScriptsManagerHost>>
      installed_scripts_manager_host_receivers_;

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<EventLog> events_;
  base::test::ScopedFeatureList scoped_feature_list_;
  blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type_;
};

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  worker->AddObserver(this);

  // Start should succeed.
  StartWorker(worker.get(), CreateStartParams(pair.second));

  // The 'WorkerStarted' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());

  // Stop the worker.
  worker->Stop();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The 'WorkerStopped' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

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

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  const int64_t service_worker_version_id = pair.second->version_id();
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  {
    // Start once normally.
    StartWorker(worker.get(), CreateStartParams(pair.second));
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
    StartWorker(worker.get(), CreateStartParams(pair.second));

    // The worker should be using the new render process.
    EXPECT_EQ(helper_->new_render_process_id(), worker->process_id());
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(EmbeddedWorkerInstanceTest, StopWhenDevToolsAttached) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // Start the worker and then call StopIfNotAttachedToDevTools().
  StartWorker(worker.get(), CreateStartParams(pair.second));
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfNotAttachedToDevTools();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The worker must be stopped now.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // Set devtools_attached to true, and do the same.
  worker->SetDevToolsAttached(true);

  StartWorker(worker.get(), CreateStartParams(pair.second));
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfNotAttachedToDevTools();
  base::RunLoop().RunUntilIdle();

  // The worker must not be stopped this time.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

  // Calling Stop() actually stops the worker regardless of whether devtools
  // is attached or not.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

TEST_F(EmbeddedWorkerInstanceTest, DetachAfterSendingStartWorkerMessage) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStopWorker();
  StartWorkerUntilStartSent(worker.get(), CreateStartParams(pair.second));
  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Detach();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[0].status.value());
}

TEST_F(EmbeddedWorkerInstanceTest, StopAfterSendingStartWorkerMessage) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  client->UnblockStopWorker();

  StartWorkerUntilStartSent(worker.get(), CreateStartParams(pair.second));
  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(STOPPED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, events_[0].status.value());
  events_.clear();

  // Restart the worker.
  StartWorker(worker.get(), CreateStartParams(pair.second));

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

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  // Start the worker.
  StartWorker(worker.get(), CreateStartParams(pair.second));

  // Detach.
  worker->Detach();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

// Test for when sending the start IPC failed.
TEST_F(EmbeddedWorkerInstanceTest, FailToSendStartIPC) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Let StartWorker fail; mojo IPC fails to connect to a remote interface.
  helper_->AddPendingInstanceClient(nullptr);

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Attempt to start the worker. From the browser process's point of view, the
  // start IPC was sent.
  absl::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop loop;
  worker->Start(CreateStartParams(pair.second),
                ReceiveStatus(&status, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // But the renderer should not receive the message and the binding is broken.
  // Worker should handle the failure of binding on the remote side as detach.
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[2].status.value());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

TEST_F(EmbeddedWorkerInstanceTest, RemoveRemoteInterface) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Attempt to start the worker.
  absl::optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop loop;
  auto* client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());
  worker->Start(CreateStartParams(pair.second),
                ReceiveStatus(&status, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Disconnect the Mojo connection. Worker should handle the sudden shutdown as
  // detach.
  client->RunUntilBound();
  client->Disconnect();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[2].status.value());
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

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  // We should set COEP, or cache storage pipe won't be made.
  pair.second->set_cross_origin_embedder_policy(
      network::CrossOriginEmbedderPolicy());
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  // Start the worker.
  auto* client =
      helper_->AddNewPendingInstanceClient<RecordCacheStorageInstanceClient>(
          helper_.get());
  StartWorker(worker.get(), CreateStartParams(pair.second));

  // Cache storage should have been sent.
  EXPECT_TRUE(client->had_cache_storage());

  // Stop the worker.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
}

// Test that the worker is not given a CacheStoragePtr during startup when
// the feature is disabled.
TEST_F(EmbeddedWorkerInstanceTest, CacheStorageOptimizationIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kEagerCacheStorageSetupForServiceWorkers);

  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  auto* client =
      helper_->AddNewPendingInstanceClient<RecordCacheStorageInstanceClient>(
          helper_.get());

  // Start the worker.
  blink::mojom::EmbeddedWorkerStartParamsPtr params =
      CreateStartParams(pair.second);
  StartWorker(worker.get(), std::move(params));

  // Cache storage should not have been sent.
  EXPECT_FALSE(client->had_cache_storage());

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
                      helper()->GetNextThreadId(),
                      blink::mojom::EmbeddedWorkerStartTiming::New());
  }
};

// Tests that kAbruptCompletion is the OnStarted() status when the
// renderer reports abrupt completion.
TEST_F(EmbeddedWorkerInstanceTest, AbruptCompletion) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  worker->AddObserver(this);

  helper_->AddPendingInstanceClient(
      std::make_unique<AbruptCompletionInstanceClient>(helper_.get()));
  StartWorker(worker.get(), CreateStartParams(pair.second));

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
                      fetch_handler_type_, helper()->GetNextThreadId(),
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
  RegistrationAndVersionPair pair1 = PrepareRegistrationAndVersion(scope, url);
  auto worker1 = std::make_unique<EmbeddedWorkerInstance>(pair1.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker1->status());
  worker1->AddObserver(this);

  auto* fetch_handler_worker =
      helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
          helper_.get());
  fetch_handler_worker->set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable);
  StartWorker(worker1.get(), CreateStartParams(pair1.second));

  EXPECT_NE(blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
            fetch_handler_type_);
  worker1->Stop();

  RegistrationAndVersionPair pair2 = PrepareRegistrationAndVersion(scope, url);
  auto worker2 = std::make_unique<EmbeddedWorkerInstance>(pair2.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker2->status());
  worker2->AddObserver(this);

  auto* no_fetch_handler_worker =
      helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
          helper_.get());
  no_fetch_handler_worker->set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler);
  StartWorker(worker2.get(), CreateStartParams(pair2.second));

  EXPECT_EQ(blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
            fetch_handler_type_);
  worker2->Stop();
}

}  // namespace content

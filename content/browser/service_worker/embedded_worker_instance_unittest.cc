// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace {

EmbeddedWorkerInstance::StatusCallback ReceiveStatus(
    base::Optional<blink::ServiceWorkerStatusCode>* out_status,
    base::OnceClosure quit) {
  return base::BindOnce(
      [](base::Optional<blink::ServiceWorkerStatusCode>* out_status,
         base::OnceClosure quit, blink::ServiceWorkerStatusCode status) {
        *out_status = status;
        std::move(quit).Run();
      },
      out_status, std::move(quit));
}

const char kHistogramServiceWorkerRuntime[] = "ServiceWorker.Runtime";

}  // namespace

class EmbeddedWorkerInstanceTest : public testing::Test,
                                   public EmbeddedWorkerInstance::Listener {
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
    base::Optional<EmbeddedWorkerStatus> status;
    base::Optional<blink::mojom::ServiceWorkerStartStatus> start_status;
  };

  void RecordEvent(EventType type,
                   base::Optional<EmbeddedWorkerStatus> status = base::nullopt,
                   base::Optional<blink::mojom::ServiceWorkerStartStatus>
                       start_status = base::nullopt) {
    EventLog log = {type, status, start_status};
    events_.push_back(log);
  }

  void OnProcessAllocated() override { RecordEvent(PROCESS_ALLOCATED); }
  void OnStartWorkerMessageSent() override {
    RecordEvent(START_WORKER_MESSAGE_SENT);
  }
  void OnStarted(blink::mojom::ServiceWorkerStartStatus status) override {
    RecordEvent(STARTED, base::nullopt, status);
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
    context()->storage()->LazyInitializeForTest();

    RegistrationAndVersionPair pair;
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    pair.first = base::MakeRefCounted<ServiceWorkerRegistration>(
        options, context()->storage()->NewRegistrationId(),
        context()->AsWeakPtr());
    pair.second = base::MakeRefCounted<ServiceWorkerVersion>(
        pair.first.get(), script_url, blink::mojom::ScriptType::kClassic,
        context()->storage()->NewVersionId(), context()->AsWeakPtr());
    return pair;
  }

  // Calls worker->Start() and runs until the start IPC is sent.
  //
  // Expects success. For failure cases, call Start() manually.
  void StartWorkerUntilStartSent(
      EmbeddedWorkerInstance* worker,
      blink::mojom::EmbeddedWorkerStartParamsPtr params) {
    base::Optional<blink::ServiceWorkerStatusCode> status;
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
    params->pause_after_download = false;
    params->is_installed = false;

    params->service_worker_receiver = CreateServiceWorker();
    params->controller_receiver = CreateController();
    params->installed_scripts_info = GetInstalledScriptsInfoPtr();
    params->provider_info = CreateProviderInfo(std::move(version));
    return params;
  }

  blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo(
      scoped_refptr<ServiceWorkerVersion> version) {
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
    version->provider_host_ = ServiceWorkerProviderHost::CreateForServiceWorker(
        context()->AsWeakPtr(), version, &provider_info);
    return provider_info;
  }

  mojo::PendingReceiver<blink::mojom::ServiceWorker> CreateServiceWorker() {
    service_workers_.emplace_back();
    return service_workers_.back().BindNewPipeAndPassReceiver();
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
  std::vector<mojo::Remote<blink::mojom::ServiceWorker>> service_workers_;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
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

TEST_F(EmbeddedWorkerInstanceTest, DetachDuringProcessAllocation) {
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    // This test calls Start() then Detach() to test detaching during process
    // allocation. But when ServiceWorkerOnUI is enabled, Start() synchronously
    // reaches the SetupOnUIThread() step, so process allocation occurs before
    // Detach() is called, so this test doesn't make sense.
    return;
  }

  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Run the start worker sequence and detach during process allocation.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  blink::mojom::EmbeddedWorkerStartParamsPtr params =
      CreateStartParams(pair.second);
  worker->Start(std::move(params), ReceiveStatus(&status, base::DoNothing()));
  worker->Detach();
  base::RunLoop().RunUntilIdle();
  // The start callback should not be aborted by detach (see a comment on the
  // dtor of EmbeddedWorkerInstance::StartTask).
  EXPECT_FALSE(status);

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // "PROCESS_ALLOCATED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[0].status.value());
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

TEST_F(EmbeddedWorkerInstanceTest, StopDuringProcessAllocation) {
  if (ServiceWorkerContext::IsServiceWorkerOnUIEnabled()) {
    // This test calls Start() then Stop() to test stopping during process
    // allocation. But when ServiceWorkerOnUI is enabled, Start() synchronously
    // reaches the SetupOnUIThread() step, so process allocation occurs before
    // Stop() is called, so this test doesn't make sense.
    return;
  }

  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Stop the start worker sequence before a process is allocated.
  base::Optional<blink::ServiceWorkerStatusCode> status;

  worker->Start(CreateStartParams(pair.second),
                ReceiveStatus(&status, base::DoNothing()));
  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by stop (see a comment on the dtor
  // of EmbeddedWorkerInstance::StartTask).
  EXPECT_FALSE(status);

  // "PROCESS_ALLOCATED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(STOPPED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[0].status.value());
  events_.clear();

  // Restart the worker.
  StartWorker(worker.get(), CreateStartParams(pair.second));

  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

class DontReceiveResumeAfterDownloadInstanceClient
    : public FakeEmbeddedWorkerInstanceClient {
 public:
  DontReceiveResumeAfterDownloadInstanceClient(
      EmbeddedWorkerTestHelper* helper,
      bool* was_resume_after_download_called)
      : FakeEmbeddedWorkerInstanceClient(helper),
        was_resume_after_download_called_(was_resume_after_download_called) {}

 private:
  void ResumeAfterDownload() override {
    *was_resume_after_download_called_ = true;
  }

  bool* const was_resume_after_download_called_;
};

TEST_F(EmbeddedWorkerInstanceTest, StopDuringPausedAfterDownload) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  bool was_resume_after_download_called = false;
  helper_->AddPendingInstanceClient(
      std::make_unique<DontReceiveResumeAfterDownloadInstanceClient>(
          helper_.get(), &was_resume_after_download_called));

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());
  worker->AddObserver(this);

  // Run the start worker sequence until pause after download.
  blink::mojom::EmbeddedWorkerStartParamsPtr params =
      CreateStartParams(pair.second);
  params->pause_after_download = true;
  base::Optional<blink::ServiceWorkerStatusCode> status;
  worker->Start(std::move(params), ReceiveStatus(&status, base::DoNothing()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Make the worker stopping and attempt to send a resume after download
  // message.
  worker->Stop();
  worker->ResumeAfterDownload();
  base::RunLoop().RunUntilIdle();

  // The resume after download message should not have been sent.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_FALSE(was_resume_after_download_called);
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
  base::Optional<blink::ServiceWorkerStatusCode> status;
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
  base::Optional<blink::ServiceWorkerStatusCode> status;
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
    had_cache_storage_ = !!start_params->provider_info->cache_storage;
    FakeEmbeddedWorkerInstanceClient::StartWorker(std::move(start_params));
  }

  bool had_cache_storage() const { return had_cache_storage_; }

 private:
  bool had_cache_storage_ = false;
};

// Test that the worker is given a CacheStoragePtr during startup, when
// |pause_after_download| is false.
TEST_F(EmbeddedWorkerInstanceTest, CacheStorageOptimization) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  // First, test a worker without pause after download.
  {
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

  // Second, test a worker with pause after download.
  {
    auto* client =
        helper_->AddNewPendingInstanceClient<RecordCacheStorageInstanceClient>(
            helper_.get());

    // Start the worker until paused.
    blink::mojom::EmbeddedWorkerStartParamsPtr params =
        CreateStartParams(pair.second);
    params->pause_after_download = true;
    worker->Start(std::move(params), base::DoNothing());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

    // Finish starting.
    worker->ResumeAfterDownload();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

    // Cache storage should not have been sent.
    EXPECT_FALSE(client->had_cache_storage());

    // Stop the worker.
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
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

  // First, test a worker without pause after download.
  {
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

  // Second, test a worker with pause after download.
  {
    auto* client =
        helper_->AddNewPendingInstanceClient<RecordCacheStorageInstanceClient>(
            helper_.get());

    // Start the worker until paused.
    blink::mojom::EmbeddedWorkerStartParamsPtr params =
        CreateStartParams(pair.second);
    params->pause_after_download = true;
    worker->Start(std::move(params), base::DoNothing());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

    // Finish starting.
    worker->ResumeAfterDownload();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

    // Cache storage should not have been sent.
    EXPECT_FALSE(client->had_cache_storage());

    // Stop the worker.
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
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

// Tests recording the lifetime UMA.
TEST_F(EmbeddedWorkerInstanceTest, Lifetime) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  base::HistogramTester metrics;

  // Start the worker.
  StartWorker(worker.get(), CreateStartParams(pair.second));
  metrics.ExpectTotalCount(kHistogramServiceWorkerRuntime, 0);

  // Stop the worker.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // The runtime metric should have been recorded.
  metrics.ExpectTotalCount(kHistogramServiceWorkerRuntime, 1);
}

// Tests that the lifetime UMA isn't recorded if DevTools was attached
// while the worker was running.
TEST_F(EmbeddedWorkerInstanceTest, Lifetime_DevToolsAttachedAfterStart) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  base::HistogramTester metrics;

  // Start the worker.
  StartWorker(worker.get(), CreateStartParams(pair.second));

  // Attach DevTools.
  worker->SetDevToolsAttached(true);

  // To make things tricky, detach DevTools.
  worker->SetDevToolsAttached(false);

  // Stop the worker.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // The runtime metric should not have ben recorded since DevTools
  // was attached at some point during the worker's life.
  metrics.ExpectTotalCount(kHistogramServiceWorkerRuntime, 0);
}

// Tests that the lifetime UMA isn't recorded if DevTools was attached
// before the worker finished starting.
TEST_F(EmbeddedWorkerInstanceTest, Lifetime_DevToolsAttachedDuringStart) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  auto worker = std::make_unique<EmbeddedWorkerInstance>(pair.second.get());

  base::HistogramTester metrics;

  // Attach DevTools while the worker is starting.
  worker->Start(CreateStartParams(pair.second), base::DoNothing());
  worker->SetDevToolsAttached(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

  // To make things tricky, detach DevTools.
  worker->SetDevToolsAttached(false);

  // Stop the worker.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // The runtime metric should not have ben recorded since DevTools
  // was attached at some point during the worker's life.
  metrics.ExpectTotalCount(kHistogramServiceWorkerRuntime, 0);
}

}  // namespace content

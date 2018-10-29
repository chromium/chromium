// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
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

}  // namespace

class EmbeddedWorkerInstanceTest : public testing::TestWithParam<bool>,
                                   public EmbeddedWorkerInstance::Listener {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

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
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          blink::features::kServiceWorkerServicification);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          blink::features::kServiceWorkerServicification);
    }
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  using RegistrationAndVersionPair =
      std::pair<scoped_refptr<ServiceWorkerRegistration>,
                scoped_refptr<ServiceWorkerVersion>>;

  RegistrationAndVersionPair PrepareRegistrationAndVersion(
      const GURL& scope,
      const GURL& script_url) {
    base::RunLoop loop;
    if (!context()->storage()->LazyInitializeForTest(loop.QuitClosure())) {
      loop.Run();
    }
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
  void StartWorkerUntilStartSent(EmbeddedWorkerInstance* worker,
                                 mojom::EmbeddedWorkerStartParamsPtr params) {
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
                   mojom::EmbeddedWorkerStartParamsPtr params) {
    StartWorkerUntilStartSent(worker, std::move(params));
    // TODO(falken): Listen for OnStarted() instead of this.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
  }

  mojom::EmbeddedWorkerStartParamsPtr CreateStartParams(
      scoped_refptr<ServiceWorkerVersion> version) {
    auto params = mojom::EmbeddedWorkerStartParams::New();
    params->service_worker_version_id = version->version_id();
    params->scope = version->scope();
    params->script_url = version->script_url();
    params->pause_after_download = false;
    params->is_installed = false;

    params->service_worker_request = CreateServiceWorker();
    params->controller_request = CreateController();
    params->installed_scripts_info = GetInstalledScriptsInfoPtr();
    params->provider_info = CreateProviderInfo(std::move(version));
    return params;
  }

  mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo(
      scoped_refptr<ServiceWorkerVersion> version) {
    auto provider_info = mojom::ServiceWorkerProviderInfoForStartWorker::New();
    version->provider_host_ = ServiceWorkerProviderHost::PreCreateForController(
        context()->AsWeakPtr(), version, &provider_info);
    return provider_info;
  }

  mojom::ServiceWorkerRequest CreateServiceWorker() {
    service_workers_.emplace_back();
    return mojo::MakeRequest(&service_workers_.back());
  }

  mojom::ControllerServiceWorkerRequest CreateController() {
    controllers_.emplace_back();
    return mojo::MakeRequest(&controllers_.back());
  }

  void SetWorkerStatus(EmbeddedWorkerInstance* worker,
                       EmbeddedWorkerStatus status) {
    worker->status_ = status;
  }

  blink::mojom::ServiceWorkerInstalledScriptsInfoPtr
  GetInstalledScriptsInfoPtr() {
    installed_scripts_managers_.emplace_back();
    auto info = blink::mojom::ServiceWorkerInstalledScriptsInfo::New();
    info->manager_request =
        mojo::MakeRequest(&installed_scripts_managers_.back());
    installed_scripts_manager_host_requests_.push_back(
        mojo::MakeRequest(&info->manager_host_ptr));
    return info;
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    DCHECK(context());
    return context()->embedded_worker_registry();
  }

  std::vector<std::unique_ptr<
      EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient>>*
  mock_instance_clients() {
    return helper_->mock_instance_clients();
  }

  // Mojo endpoints.
  std::vector<mojom::ServiceWorkerPtr> service_workers_;
  std::vector<mojom::ControllerServiceWorkerPtr> controllers_;
  std::vector<blink::mojom::ServiceWorkerInstalledScriptsManagerPtr>
      installed_scripts_managers_;
  std::vector<blink::mojom::ServiceWorkerInstalledScriptsManagerHostRequest>
      installed_scripts_manager_host_requests_;

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<EventLog> events_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

// A helper to simulate the start worker sequence is stalled in a worker
// process.
class StalledInStartWorkerHelper : public EmbeddedWorkerTestHelper {
 public:
  StalledInStartWorkerHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~StalledInStartWorkerHelper() override {}

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerRequest service_worker_request,
      mojom::ControllerServiceWorkerRequest controller_request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info)
      override {
    if (force_stall_in_start_) {
      // Prepare for OnStopWorker().
      instance_host_ptr_map_[embedded_worker_id].Bind(std::move(instance_host));
      // Do nothing to simulate a stall in the worker process.
      return;
    }
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id, service_worker_version_id, scope, script_url,
        pause_after_download, std::move(service_worker_request),
        std::move(controller_request), std::move(instance_host),
        std::move(provider_info), std::move(installed_scripts_info));
  }

  void OnStopWorker(int embedded_worker_id) override {
    if (instance_host_ptr_map_[embedded_worker_id]) {
      instance_host_ptr_map_[embedded_worker_id]->OnStopped();
      base::RunLoop().RunUntilIdle();
      return;
    }
    EmbeddedWorkerTestHelper::OnStopWorker(embedded_worker_id);
  }

  void set_force_stall_in_start(bool force_stall_in_start) {
    force_stall_in_start_ = force_stall_in_start;
  }

 private:
  bool force_stall_in_start_ = true;

  std::map<
      int /* embedded_worker_id */,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtr /* instance_host_ptr */>
      instance_host_ptr_map_;
};

TEST_P(EmbeddedWorkerInstanceTest, StartAndStop) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
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
TEST_P(EmbeddedWorkerInstanceTest, ForceNewProcess) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  const int64_t service_worker_version_id = pair.second->version_id();
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
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

TEST_P(EmbeddedWorkerInstanceTest, StopWhenDevToolsAttached) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
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

// Test that the removal of a worker from the registry doesn't remove
// other workers in the same process.
TEST_P(EmbeddedWorkerInstanceTest, RemoveWorkerInSharedProcess) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair1 = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker1 =
      embedded_worker_registry()->CreateWorker(pair1.second.get());
  RegistrationAndVersionPair pair2 = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker2 =
      embedded_worker_registry()->CreateWorker(pair2.second.get());

  int process_id = helper_->mock_render_process_id();

  // Start workers.
  StartWorker(worker1.get(), CreateStartParams(pair1.second));
  StartWorker(worker2.get(), CreateStartParams(pair2.second));

  // The two workers share the same process.
  EXPECT_EQ(worker1->process_id(), worker2->process_id());

  // Destroy worker1. It removes itself from the registry.
  int worker1_id = worker1->embedded_worker_id();
  worker1->Stop();
  worker1.reset();

  // Only worker1 should be removed from the registry's process_map.
  EmbeddedWorkerRegistry* registry =
      helper_->context()->embedded_worker_registry();
  EXPECT_EQ(0UL, registry->worker_process_map_[process_id].count(worker1_id));
  EXPECT_EQ(1UL, registry->worker_process_map_[process_id].count(
                     worker2->embedded_worker_id()));

  worker2->Stop();
}

TEST_P(EmbeddedWorkerInstanceTest, DetachDuringProcessAllocation) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

  // Run the start worker sequence and detach during process allocation.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  mojom::EmbeddedWorkerStartParamsPtr params = CreateStartParams(pair.second);
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

TEST_P(EmbeddedWorkerInstanceTest, DetachAfterSendingStartWorkerMessage) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_.reset(new StalledInStartWorkerHelper());
  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

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

TEST_P(EmbeddedWorkerInstanceTest, StopDuringProcessAllocation) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
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
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit DontReceiveResumeAfterDownloadInstanceClient(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper,
      bool* was_resume_after_download_called)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper),
        was_resume_after_download_called_(was_resume_after_download_called) {}

 private:
  void ResumeAfterDownload() override {
    *was_resume_after_download_called_ = true;
  }

  bool* const was_resume_after_download_called_;
};

TEST_P(EmbeddedWorkerInstanceTest, StopDuringPausedAfterDownload) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  bool was_resume_after_download_called = false;
  helper_->RegisterMockInstanceClient(
      std::make_unique<DontReceiveResumeAfterDownloadInstanceClient>(
          helper_->AsWeakPtr(), &was_resume_after_download_called));

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

  // Run the start worker sequence until pause after download.
  mojom::EmbeddedWorkerStartParamsPtr params = CreateStartParams(pair.second);
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

TEST_P(EmbeddedWorkerInstanceTest, StopAfterSendingStartWorkerMessage) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_.reset(new StalledInStartWorkerHelper);
  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

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
  static_cast<StalledInStartWorkerHelper*>(helper_.get())
      ->set_force_stall_in_start(false);
  StartWorker(worker.get(), CreateStartParams(pair.second));

  // The worker should be started.
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

TEST_P(EmbeddedWorkerInstanceTest, Detach) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

  // Start the worker.
  base::RunLoop run_loop;
  StartWorker(worker.get(), CreateStartParams(pair.second));

  // Detach.
  int process_id = worker->process_id();
  worker->Detach();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // Send the registry a message from the detached worker. Nothing should
  // happen.
  embedded_worker_registry()->OnWorkerStarted(process_id,
                                              worker->embedded_worker_id());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

// Test for when sending the start IPC failed.
TEST_P(EmbeddedWorkerInstanceTest, FailToSendStartIPC) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Let StartWorker fail; mojo IPC fails to connect to a remote interface.
  helper_->RegisterMockInstanceClient(nullptr);

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
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

class FailEmbeddedWorkerInstanceClientImpl
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit FailEmbeddedWorkerInstanceClientImpl(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper) {}

 private:
  void StartWorker(mojom::EmbeddedWorkerStartParamsPtr) override {
    helper_->mock_instance_clients()->clear();
  }
};

TEST_P(EmbeddedWorkerInstanceTest, RemoveRemoteInterface) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Let StartWorker fail; binding is discarded in the middle of IPC
  helper_->RegisterMockInstanceClient(
      std::make_unique<FailEmbeddedWorkerInstanceClientImpl>(
          helper_->AsWeakPtr()));
  ASSERT_EQ(mock_instance_clients()->size(), 1UL);

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

  // Attempt to start the worker.
  base::Optional<blink::ServiceWorkerStatusCode> status;
  base::RunLoop loop;
  worker->Start(CreateStartParams(pair.second),
                ReceiveStatus(&status, loop.QuitClosure()));
  loop.Run();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

  // Worker should handle the sudden shutdown as detach.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[2].status.value());
}

class StoreMessageInstanceClient
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit StoreMessageInstanceClient(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper) {}

  const std::vector<std::pair<blink::WebConsoleMessage::Level, std::string>>&
  message() {
    return messages_;
  }

 private:
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message) override {
    messages_.push_back(std::make_pair(level, message));
  }

  std::vector<std::pair<blink::WebConsoleMessage::Level, std::string>>
      messages_;
};

TEST_P(EmbeddedWorkerInstanceTest, AddMessageToConsole) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  std::unique_ptr<StoreMessageInstanceClient> instance_client =
      std::make_unique<StoreMessageInstanceClient>(helper_->AsWeakPtr());
  StoreMessageInstanceClient* instance_client_rawptr = instance_client.get();
  helper_->RegisterMockInstanceClient(std::move(instance_client));
  ASSERT_EQ(mock_instance_clients()->size(), 1UL);

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  worker->AddObserver(this);

  // Attempt to start the worker and immediate AddMessageToConsole should not
  // cause a crash.
  std::pair<blink::WebConsoleMessage::Level, std::string> test_message =
      std::make_pair(blink::WebConsoleMessage::kLevelVerbose, "");
  base::Optional<blink::ServiceWorkerStatusCode> status;
  worker->Start(CreateStartParams(pair.second),
                ReceiveStatus(&status, base::DoNothing()));
  worker->AddMessageToConsole(test_message.first, test_message.second);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

  // Messages sent before sending StartWorker message won't be dispatched.
  ASSERT_EQ(0UL, instance_client_rawptr->message().size());
  ASSERT_EQ(3UL, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  worker->AddMessageToConsole(test_message.first, test_message.second);
  base::RunLoop().RunUntilIdle();

  // Messages sent after sending StartWorker message should be reached to
  // the renderer.
  ASSERT_EQ(1UL, instance_client_rawptr->message().size());
  EXPECT_EQ(test_message, instance_client_rawptr->message()[0]);

  // Ensure the worker is stopped.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

// Records whether a CacheStoragePtr was sent as part of StartWorker.
class RecordCacheStorageHelper : public EmbeddedWorkerTestHelper {
 public:
  RecordCacheStorageHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~RecordCacheStorageHelper() override {}

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerRequest service_worker_request,
      mojom::ControllerServiceWorkerRequest controller_request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      blink::mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info)
      override {
    had_cache_storage_ = !!provider_info->cache_storage;
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id, service_worker_version_id, scope, script_url,
        pause_after_download, std::move(service_worker_request),
        std::move(controller_request), std::move(instance_host),
        std::move(provider_info), std::move(installed_scripts_info));
  }

  bool had_cache_storage() const { return had_cache_storage_; }

 private:
  bool had_cache_storage_ = false;
};

// Test that the worker is given a CacheStoragePtr during startup, when
// |pause_after_download| is false.
TEST_P(EmbeddedWorkerInstanceTest, CacheStorageOptimization) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  auto helper = std::make_unique<RecordCacheStorageHelper>();
  auto* helper_rawptr = helper.get();
  helper_ = std::move(helper);

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());

  // First, test a worker without pause after download.
  {
    // Start the worker.
    StartWorker(worker.get(), CreateStartParams(pair.second));

    // Cache storage should have been sent.
    EXPECT_TRUE(helper_rawptr->had_cache_storage());

    // Stop the worker.
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }

  // Second, test a worker with pause after download.
  {
    // Start the worker until paused.
    mojom::EmbeddedWorkerStartParamsPtr params = CreateStartParams(pair.second);
    params->pause_after_download = true;
    worker->Start(std::move(params), base::DoNothing());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

    // Finish starting.
    worker->ResumeAfterDownload();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

    // Cache storage should not have been sent.
    EXPECT_FALSE(helper_rawptr->had_cache_storage());

    // Stop the worker.
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
}

// Test that the worker is not given a CacheStoragePtr during startup when
// the feature is disabled.
TEST_P(EmbeddedWorkerInstanceTest, CacheStorageOptimizationIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      blink::features::kEagerCacheStorageSetupForServiceWorkers);

  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  auto helper = std::make_unique<RecordCacheStorageHelper>();
  auto* helper_rawptr = helper.get();
  helper_ = std::move(helper);

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());

  // First, test a worker without pause after download.
  {
    // Start the worker.
    mojom::EmbeddedWorkerStartParamsPtr params = CreateStartParams(pair.second);
    StartWorker(worker.get(), std::move(params));

    // Cache storage should not have been sent.
    EXPECT_FALSE(helper_rawptr->had_cache_storage());

    // Stop the worker.
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }

  // Second, test a worker with pause after download.
  {
    // Start the worker until paused.
    mojom::EmbeddedWorkerStartParamsPtr params = CreateStartParams(pair.second);
    params->pause_after_download = true;
    worker->Start(std::move(params), base::DoNothing());
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

    // Finish starting.
    worker->ResumeAfterDownload();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

    // Cache storage should not have been sent.
    EXPECT_FALSE(helper_rawptr->had_cache_storage());

    // Stop the worker.
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
}

// Starts the worker with kAbruptCompletion status.
class AbruptCompletionHelper : public EmbeddedWorkerTestHelper {
 public:
  AbruptCompletionHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~AbruptCompletionHelper() override = default;

  void OnResumeAfterDownload(int embedded_worker_id) override {
    SimulateScriptEvaluationStart(embedded_worker_id);
    SimulateWorkerStarted(
        embedded_worker_id,
        blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion,
        GetNextThreadId());
  }
};

// Tests that kAbruptCompletion is the OnStarted() status when the
// renderer reports abrupt completion.
TEST_P(EmbeddedWorkerInstanceTest, AbruptCompletion) {
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");
  helper_ = std::make_unique<AbruptCompletionHelper>();

  RegistrationAndVersionPair pair = PrepareRegistrationAndVersion(scope, url);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker(pair.second.get());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  worker->AddObserver(this);

  StartWorker(worker.get(), CreateStartParams(pair.second));

  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);
  EXPECT_EQ(blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion,
            events_[2].start_status.value());
  worker->Stop();
}

INSTANTIATE_TEST_CASE_P(IsServiceWorkerServicificationEnabled,
                        EmbeddedWorkerInstanceTest,
                        ::testing::Bool(););

}  // namespace content

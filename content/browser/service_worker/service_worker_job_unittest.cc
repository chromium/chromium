// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <optional>
#include <tuple>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "components/services/storage/service_worker/service_worker_database.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/test_service_worker_observer.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/test/fake_network.h"
#include "content/test/storage_partition_test_helpers.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

// Unit tests for testing all job registration tasks.
namespace content {

namespace {

using net::IOBuffer;
using net::TestCompletionCallback;
using net::WrappedIOBuffer;

using ::testing::Eq;
using ::testing::Pointee;

void SaveRegistrationCallback(
    blink::ServiceWorkerStatusCode expected_status,
    scoped_refptr<ServiceWorkerRegistration>* registration_out,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  EXPECT_EQ(expected_status, status);
  *registration_out = registration;
  std::move(quit_closure).Run();
}

void SaveFoundRegistrationCallback(
    blink::ServiceWorkerStatusCode expected_status,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    base::OnceClosure quit_closure,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> result) {
  EXPECT_EQ(expected_status, status);
  *registration = std::move(result);
  std::move(quit_closure).Run();
}

// Creates a callback which keeps track of the resulting registration.
// When the callback is fired, it ensures that the resulting status
// matches the expectation.
ServiceWorkerRegisterJob::RegistrationCallback SaveRegistration(
    blink::ServiceWorkerStatusCode expected_status,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    base::OnceClosure quit_closure) {
  return base::BindOnce(&SaveRegistrationCallback, expected_status,
                        registration, std::move(quit_closure));
}

ServiceWorkerRegistry::FindRegistrationCallback SaveFoundRegistration(
    blink::ServiceWorkerStatusCode expected_status,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    base::OnceClosure quit_closure) {
  return base::BindOnce(&SaveFoundRegistrationCallback, expected_status,
                        registration, std::move(quit_closure));
}

void SaveUnregistrationCallback(blink::ServiceWorkerStatusCode expected_status,
                                base::OnceClosure quit_closure,
                                int64_t registration_id,
                                blink::ServiceWorkerStatusCode status) {
  EXPECT_EQ(expected_status, status);
  std::move(quit_closure).Run();
}

ServiceWorkerUnregisterJob::UnregistrationCallback SaveUnregistration(
    blink::ServiceWorkerStatusCode expected_status,
    base::OnceClosure quit_closure) {
  return base::BindOnce(&SaveUnregistrationCallback, expected_status,
                        std::move(quit_closure));
}

void RequestTermination(
    mojo::AssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>* host) {
  // We can't wait for the callback since StopWorker() arrives before it which
  // severs the Mojo connection.
  (*host)->RequestTermination(base::DoNothing());
}

class EmbeddedWorkerStatusObserver : public ServiceWorkerVersion::Observer {
 public:
  EmbeddedWorkerStatusObserver(base::OnceClosure quit_closure,
                               blink::EmbeddedWorkerStatus running_status)
      : quit_closure_(std::move(quit_closure)),
        expected_running_status_(running_status) {}

  void OnRunningStateChanged(ServiceWorkerVersion* version) override {
    if (!quit_closure_)
      return;

    if (version->running_status() == expected_running_status_)
      std::move(quit_closure_).Run();
  }

 private:
  EmbeddedWorkerStatusObserver(const EmbeddedWorkerStatusObserver&) = delete;
  EmbeddedWorkerStatusObserver& operator=(const EmbeddedWorkerStatusObserver&) =
      delete;

  base::OnceClosure quit_closure_;

  blink::EmbeddedWorkerStatus expected_running_status_;
};

network::CrossOriginEmbedderPolicy CrossOriginEmbedderPolicyNone() {
  return network::CrossOriginEmbedderPolicy();
}

network::CrossOriginEmbedderPolicy CrossOriginEmbedderPolicyRequireCorp() {
  network::CrossOriginEmbedderPolicy out;
  out.value = network::mojom::CrossOriginEmbedderPolicyValue::kRequireCorp;
  return out;
}

}  // namespace

enum class StorageKeyTestCase {
  kFirstParty,
  kThirdParty,
};

class ServiceWorkerJobTest
    : public testing::Test,
      public testing::WithParamInterface<StorageKeyTestCase> {
 public:
  ServiceWorkerJobTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME,
                          BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

  ServiceWorkerJobCoordinator* job_coordinator() const {
    return context()->job_coordinator();
  }
  ServiceWorkerRegistry* registry() const { return context()->registry(); }

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

 protected:
  scoped_refptr<ServiceWorkerRegistration> RunRegisterJob(
      const GURL& script_url,
      const blink::StorageKey& key,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      blink::ServiceWorkerStatusCode expected_status =
          blink::ServiceWorkerStatusCode::kOk);
  void RunUnregisterJob(const GURL& scope,
                        const blink::StorageKey& key,
                        blink::ServiceWorkerStatusCode expected_status =
                            blink::ServiceWorkerStatusCode::kOk);
  void WaitForVersionRunningStatus(scoped_refptr<ServiceWorkerVersion> version,
                                   blink::EmbeddedWorkerStatus running_status);
  scoped_refptr<ServiceWorkerRegistration> FindRegistrationForScope(
      const GURL& scope,
      const blink::StorageKey& key,
      blink::ServiceWorkerStatusCode expected_status =
          blink::ServiceWorkerStatusCode::kOk);
  ServiceWorkerClient* CreateControllee();
  scoped_refptr<ServiceWorkerRegistration> CreateRegistrationWithControllee(
      const GURL& script_url,
      const GURL& scope_url);
  std::vector<int64_t> GetPurgingResourceIdsForLiveVersion(int64_t version_id) {
    base::test::TestFuture<storage::ServiceWorkerDatabase::Status,
                           std::vector<int64_t>>
        future;
    context()->GetStorageControl()->GetPurgingResourceIdsForLiveVersionForTest(
        version_id, future.GetCallback<storage::ServiceWorkerDatabase::Status,
                                       const std::vector<int64_t>&>());
    return future.Get<1>();
  }

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<ScopedServiceWorkerClient> service_worker_clients_keep_alive_;
};

scoped_refptr<ServiceWorkerRegistration> ServiceWorkerJobTest::RunRegisterJob(
    const GURL& script_url,
    const blink::StorageKey& key,
    const blink::mojom::ServiceWorkerRegistrationOptions& options,
    blink::ServiceWorkerStatusCode expected_status) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  base::RunLoop run_loop;
  auto outside_fetch_client_settings_object =
      blink::mojom::FetchClientSettingsObject::New();
  outside_fetch_client_settings_object->outgoing_referrer = script_url;
  job_coordinator()->Register(
      script_url, options, key, std::move(outside_fetch_client_settings_object),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(expected_status, &registration, run_loop.QuitClosure()),
      PolicyContainerPolicies());
  run_loop.Run();
  return registration;
}

void ServiceWorkerJobTest::RunUnregisterJob(
    const GURL& scope,
    const blink::StorageKey& key,
    blink::ServiceWorkerStatusCode expected_status) {
  base::RunLoop run_loop;
  job_coordinator()->Unregister(
      scope, key, /*is_immediate=*/false,
      SaveUnregistration(expected_status, run_loop.QuitClosure()));
  run_loop.Run();
}

void ServiceWorkerJobTest::WaitForVersionRunningStatus(
    scoped_refptr<ServiceWorkerVersion> version,
    blink::EmbeddedWorkerStatus running_status) {
  if (version->running_status() == running_status)
    return;

  base::RunLoop run_loop;
  EmbeddedWorkerStatusObserver observer(run_loop.QuitClosure(), running_status);
  version->AddObserver(&observer);
  run_loop.Run();
  version->RemoveObserver(&observer);
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerJobTest::FindRegistrationForScope(
    const GURL& scope,
    const blink::StorageKey& key,
    blink::ServiceWorkerStatusCode expected_status) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  base::RunLoop run_loop;
  registry()->FindRegistrationForScope(
      scope, key,
      SaveFoundRegistration(expected_status, &registration,
                            run_loop.QuitClosure()));
  run_loop.Run();
  return registration;
}

ServiceWorkerClient* ServiceWorkerJobTest::CreateControllee() {
  ScopedServiceWorkerClient service_worker_client =
      CreateServiceWorkerClient(helper_->context());
  ServiceWorkerClient* service_worker_client_ptr = service_worker_client.get();
  service_worker_clients_keep_alive_.push_back(
      std::move(service_worker_client));
  return service_worker_client_ptr;
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerJobTest::CreateRegistrationWithControllee(const GURL& script_url,
                                                       const GURL& scope_url) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope_url;
  blink::StorageKey storage_key = GetTestStorageKey(scope_url);

  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script_url, storage_key, options);

  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(registration->installing_version(), runner);

  ServiceWorkerClient* service_worker_client = CreateControllee();
  service_worker_client->UpdateUrls(scope_url, url::Origin::Create(scope_url),
                                    storage_key);
  service_worker_client->SetControllerRegistration(
      registration,
      /*notify_controllerchange=*/false);
  return registration;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerJobTest,
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

TEST_P(ServiceWorkerJobTest, SameDocumentSameRegistration) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  GURL url("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(url);

  options.scope = url;
  scoped_refptr<ServiceWorkerRegistration> original_registration =
      RunRegisterJob(GURL("https://www.example.com/service_worker.js"), key,
                     options);
  scoped_refptr<ServiceWorkerRegistration> registration1;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, url, key,
      SaveFoundRegistration(blink::ServiceWorkerStatusCode::kOk, &registration1,
                            barrier_closure));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, url, key,
      SaveFoundRegistration(blink::ServiceWorkerStatusCode::kOk, &registration2,
                            barrier_closure));
  run_loop.Run();
  ASSERT_TRUE(registration1.get());
  ASSERT_EQ(registration1, original_registration);
  ASSERT_EQ(registration1, registration2);
}

TEST_P(ServiceWorkerJobTest, SameMatchSameRegistration) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  GURL url("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(url);

  options.scope = url;
  scoped_refptr<ServiceWorkerRegistration> original_registration =
      RunRegisterJob(GURL("https://www.example.com/service_worker.js"), key,
                     options);
  ASSERT_NE(static_cast<ServiceWorkerRegistration*>(nullptr),
            original_registration.get());

  scoped_refptr<ServiceWorkerRegistration> registration1;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation,
      GURL("https://www.example.com/one"), key,
      SaveFoundRegistration(blink::ServiceWorkerStatusCode::kOk, &registration1,
                            barrier_closure));

  scoped_refptr<ServiceWorkerRegistration> registration2;
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation,
      GURL("https://www.example.com/two"), key,
      SaveFoundRegistration(blink::ServiceWorkerStatusCode::kOk, &registration2,
                            barrier_closure));
  run_loop.Run();
  ASSERT_EQ(registration1, original_registration);
  ASSERT_EQ(registration1, registration2);
}

TEST_P(ServiceWorkerJobTest, DifferentMatchDifferentRegistration) {
  const GURL scope1("https://www.example.com/one");
  const GURL scope2("https://www.example.com/two");
  const GURL script_url("https://www.example.com/service_worker.js");
  const blink::StorageKey key = GetTestStorageKey(script_url);
  blink::mojom::ServiceWorkerRegistrationOptions options1;
  options1.scope = scope1;
  blink::mojom::ServiceWorkerRegistrationOptions options2;
  options2.scope = scope2;

  RunRegisterJob(script_url, key, options1);
  RunRegisterJob(script_url, key, options2);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, scope1, key,
      SaveFoundRegistration(blink::ServiceWorkerStatusCode::kOk, &registration1,
                            barrier_closure));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  registry()->FindRegistrationForClientUrl(
      ServiceWorkerRegistry::Purpose::kNotForNavigation, scope2, key,
      SaveFoundRegistration(blink::ServiceWorkerStatusCode::kOk, &registration2,
                            barrier_closure));

  run_loop.Run();
  ASSERT_NE(registration1, registration2);
}

class RecordInstallActivateWorker : public FakeServiceWorker {
 public:
  RecordInstallActivateWorker(EmbeddedWorkerTestHelper* helper)
      : FakeServiceWorker(helper) {}
  ~RecordInstallActivateWorker() override = default;

  const std::vector<ServiceWorkerMetrics::EventType>& events() const {
    return events_;
  }

  void DispatchInstallEvent(
      blink::mojom::ServiceWorker::DispatchInstallEventCallback callback)
      override {
    events_.emplace_back(ServiceWorkerMetrics::EventType::INSTALL);
    FakeServiceWorker::DispatchInstallEvent(std::move(callback));
  }

  void DispatchActivateEvent(
      blink::mojom::ServiceWorker::DispatchActivateEventCallback callback)
      override {
    events_.emplace_back(ServiceWorkerMetrics::EventType::ACTIVATE);
    FakeServiceWorker::DispatchActivateEvent(std::move(callback));
  }

 private:
  std::vector<ServiceWorkerMetrics::EventType> events_;
};

// Make sure basic registration is working.
TEST_P(ServiceWorkerJobTest, Register) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  auto* worker =
      helper_->AddNewPendingServiceWorker<RecordInstallActivateWorker>(
          helper_.get());
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(GURL("https://www.example.com/service_worker.js"),
                     GetTestStorageKey(options.scope), options);
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(registration->installing_version(), runner);
  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();

  EXPECT_TRUE(registration);
  ASSERT_EQ(2u, worker->events().size());
  EXPECT_EQ(ServiceWorkerMetrics::EventType::INSTALL, worker->events()[0]);
  EXPECT_EQ(ServiceWorkerMetrics::EventType::ACTIVATE, worker->events()[1]);
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_P(ServiceWorkerJobTest, Unregister) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> registration = RunRegisterJob(
      GURL("https://www.example.com/service_worker.js"), key, options);
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(registration->installing_version(), runner);
  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();

  ServiceWorkerHost* worker_host =
      registration->active_version()->worker_host();
  ASSERT_NE(nullptr, worker_host);
  ServiceWorkerContainerHost* container_host = worker_host->container_host();
  // One ServiceWorkerRegistrationObjectHost should have been created for the
  // new registration.
  EXPECT_EQ(1UL, container_host->registration_object_manager()
                     .registration_object_hosts_.size());
  // One ServiceWorkerObjectHost should have been created for the new service
  // worker.
  EXPECT_EQ(1UL, container_host->version_object_manager()
                     .service_worker_object_hosts_.size());

  RunUnregisterJob(options.scope, key);

  WaitForVersionRunningStatus(version, blink::EmbeddedWorkerStatus::kStopped);
  registry()->GetRemoteStorageControl().FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // The service worker registration object host and service worker object host
  // have been destroyed together with |worker_host| by the above
  // unregistration. Then |registration| and |version| should be the last one
  // reference to the corresponding instance.
  EXPECT_TRUE(registration->HasOneRef());
  EXPECT_TRUE(version->HasOneRef());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());

  registration = FindRegistrationForScope(
      options.scope, key, blink::ServiceWorkerStatusCode::kErrorNotFound);

  EXPECT_FALSE(registration);
}

TEST_P(ServiceWorkerJobTest, Unregister_NothingRegistered) {
  GURL scope("https://www.example.com/");

  RunUnregisterJob(scope, GetTestStorageKey(scope),
                   blink::ServiceWorkerStatusCode::kErrorNotFound);
}

TEST_P(ServiceWorkerJobTest, UnregisterImmediate) {
  const GURL scope("https://www.example.com/one/");

  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateRegistrationWithControllee(
          GURL("https://www.example.com/service_worker.js"), scope);

  EXPECT_NE(nullptr, registration->active_version());

  base::RunLoop run_loop;
  job_coordinator()->Unregister(
      scope, GetTestStorageKey(scope),
      /*is_immediate=*/true,
      SaveUnregistration(blink::ServiceWorkerStatusCode::kOk,
                         run_loop.QuitClosure()));

  run_loop.Run();

  EXPECT_TRUE(registration->is_uninstalled());
  EXPECT_EQ(nullptr, registration->active_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(nullptr, registration->installing_version());
}

// Make sure registering a new script creates a new version and shares an
// existing registration.
TEST_P(ServiceWorkerJobTest, RegisterNewScript) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> old_registration = RunRegisterJob(
      GURL("https://www.example.com/service_worker.js"), key, options);
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  old_registration->SetTaskRunnerForTest(runner);
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(old_registration->installing_version(), runner);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_scope =
      FindRegistrationForScope(options.scope, key);

  ASSERT_EQ(old_registration, old_registration_by_scope);
  old_registration_by_scope = nullptr;

  scoped_refptr<ServiceWorkerRegistration> new_registration = RunRegisterJob(
      GURL("https://www.example.com/service_worker_new.js"), key, options);

  ASSERT_EQ(old_registration, new_registration);

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_scope =
      FindRegistrationForScope(options.scope, key);

  ASSERT_EQ(new_registration, new_registration_by_scope);
}

// Make sure that when registering a duplicate scope+script_url
// combination, that the same registration is used.
TEST_P(ServiceWorkerJobTest, RegisterDuplicateScript) {
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  scoped_refptr<ServiceWorkerRegistration> old_registration =
      RunRegisterJob(script_url, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();

  // During the above registration, a service worker registration object host
  // for ServiceWorkerGlobalScope#registration has been created/added into
  // |worker_host|.
  ServiceWorkerHost* worker_host =
      old_registration->active_version()->worker_host();
  ASSERT_NE(nullptr, worker_host);
  ServiceWorkerContainerHost* container_host = worker_host->container_host();

  // Clear all service worker object hosts.
  container_host->version_object_manager().service_worker_object_hosts_.clear();
  // Ensure that the registration's object host doesn't have the reference.
  EXPECT_EQ(1UL, container_host->registration_object_manager()
                     .registration_object_hosts_.size());
  container_host->registration_object_manager()
      .registration_object_hosts_.clear();
  EXPECT_EQ(0UL, container_host->registration_object_manager()
                     .registration_object_hosts_.size());
  ASSERT_TRUE(old_registration->HasOneRef());

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_scope =
      FindRegistrationForScope(options.scope, key);

  ASSERT_TRUE(old_registration_by_scope.get());

  scoped_refptr<ServiceWorkerRegistration> new_registration =
      RunRegisterJob(script_url, key, options);

  ASSERT_EQ(old_registration, new_registration);

  ASSERT_FALSE(old_registration->HasOneRef());

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_scope =
      FindRegistrationForScope(options.scope, key);

  EXPECT_EQ(new_registration_by_scope, old_registration);
}

// An instance client that breaks the Mojo connection upon receiving the
// Start() message.
class FailStartInstanceClient : public FakeEmbeddedWorkerInstanceClient {
 public:
  FailStartInstanceClient(EmbeddedWorkerTestHelper* helper)
      : FakeEmbeddedWorkerInstanceClient(helper) {}

  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override {
    // Call Disconnect to break the Mojo connection immediately.
    // This makes sure the error code becomes kErrorStartWorkerFailed. Otherwise
    // the error code might be kNetworkError when PlzServiceWorker is enabled
    // because the URLLoader bound to params->main_script_load_params can get
    // destroyed before the service worker stops.
    Disconnect();
  }
};

TEST_P(ServiceWorkerJobTest, Register_FailToStartWorker) {
  helper_->AddPendingInstanceClient(
      std::make_unique<FailStartInstanceClient>(helper_.get()));

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(GURL("https://www.example.com/service_worker.js"),
                     GetTestStorageKey(options.scope), options,
                     blink::ServiceWorkerStatusCode::kErrorNetwork);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(nullptr), registration);
}

// Register and then unregister the scope, in parallel. Job coordinator should
// process jobs until the last job.
TEST_P(ServiceWorkerJobTest, ParallelRegUnreg) {
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script_url, key, options);

  RunUnregisterJob(options.scope, key);

  registration = FindRegistrationForScope(
      options.scope, key, blink::ServiceWorkerStatusCode::kErrorNotFound);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

// Register conflicting scripts for the same scope. The most recent
// registration should win, and the old registration should have been
// shutdown.
TEST_P(ServiceWorkerJobTest, ParallelRegNewScript) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(scope);

  GURL script_url1("https://www.example.com/service_worker1.js");
  scoped_refptr<ServiceWorkerRegistration> registration1 =
      RunRegisterJob(script_url1, key,
                     blink::mojom::ServiceWorkerRegistrationOptions(
                         scope, blink::mojom::ScriptType::kClassic,
                         blink::mojom::ServiceWorkerUpdateViaCache::kNone));

  GURL script_url2("https://www.example.com/service_worker2.js");
  scoped_refptr<ServiceWorkerRegistration> registration2 =
      RunRegisterJob(script_url2, key,
                     blink::mojom::ServiceWorkerRegistrationOptions(
                         scope, blink::mojom::ScriptType::kClassic,
                         blink::mojom::ServiceWorkerUpdateViaCache::kAll));

  scoped_refptr<ServiceWorkerRegistration> registration =
      FindRegistrationForScope(scope, key);

  ASSERT_EQ(registration2, registration);
}

// Register the exact same scope + script. Requests should be
// coalesced such that both callers get the exact same registration
// object.
TEST_P(ServiceWorkerJobTest, ParallelRegSameScript) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  GURL script_url("https://www.example.com/service_worker1.js");
  scoped_refptr<ServiceWorkerRegistration> registration1 =
      RunRegisterJob(script_url, key, options);

  scoped_refptr<ServiceWorkerRegistration> registration2 =
      RunRegisterJob(script_url, key, options);

  ASSERT_EQ(registration1, registration2);

  scoped_refptr<ServiceWorkerRegistration> registration =
      FindRegistrationForScope(options.scope, key);

  ASSERT_EQ(registration, registration1);
}

// Call simulataneous unregister calls.
TEST_P(ServiceWorkerJobTest, ParallelUnreg) {
  GURL scope("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(scope);

  GURL script_url("https://www.example.com/service_worker.js");
  RunUnregisterJob(scope, key, blink::ServiceWorkerStatusCode::kErrorNotFound);

  RunUnregisterJob(scope, key, blink::ServiceWorkerStatusCode::kErrorNotFound);

  // There isn't really a way to test that they are being coalesced,
  // but we can make sure they can exist simultaneously without
  // crashing.
  scoped_refptr<ServiceWorkerRegistration> registration =
      FindRegistrationForScope(scope, key,
                               blink::ServiceWorkerStatusCode::kErrorNotFound);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

TEST_P(ServiceWorkerJobTest, AbortAll_Register) {
  GURL script_url1("https://www1.example.com/service_worker.js");
  GURL script_url2("https://www2.example.com/service_worker.js");

  blink::mojom::ServiceWorkerRegistrationOptions options1;
  options1.scope = GURL("https://www1.example.com/");
  const blink::StorageKey key1 = GetTestStorageKey(options1.scope);
  blink::mojom::ServiceWorkerRegistrationOptions options2;
  options2.scope = GURL("https://www2.example.com/");
  const blink::StorageKey key2 = GetTestStorageKey(options2.scope);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  job_coordinator()->Register(
      script_url1, options1, key1,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                       &registration1, barrier_closure),
      PolicyContainerPolicies());

  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      script_url2, options2, key2,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                       &registration2, barrier_closure),
      PolicyContainerPolicies());

  job_coordinator()->AbortAll();

  run_loop.Run();

  registration1 = FindRegistrationForScope(
      options1.scope, key1, blink::ServiceWorkerStatusCode::kErrorNotFound);

  registration2 = FindRegistrationForScope(
      options2.scope, key2, blink::ServiceWorkerStatusCode::kErrorNotFound);

  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration1);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration2);
}

TEST_P(ServiceWorkerJobTest, AbortAll_Unregister) {
  GURL scope1("https://www1.example.com/");
  GURL scope2("https://www2.example.com/");

  scoped_refptr<ServiceWorkerRegistration> registration1;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  job_coordinator()->Unregister(
      scope1, GetTestStorageKey(scope1),
      /*is_immediate=*/false,
      SaveUnregistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                         barrier_closure));

  job_coordinator()->Unregister(
      scope2, GetTestStorageKey(scope2),
      /*is_immediate=*/false,
      SaveUnregistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                         barrier_closure));

  job_coordinator()->AbortAll();

  run_loop.Run();
}

TEST_P(ServiceWorkerJobTest, AbortAll_RegUnreg) {
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  scoped_refptr<ServiceWorkerRegistration> registration;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  job_coordinator()->Register(
      script_url, options, key, blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                       &registration, barrier_closure),
      PolicyContainerPolicies());

  job_coordinator()->Unregister(
      options.scope, key, /*is_immediate=*/false,
      SaveUnregistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                         barrier_closure));

  job_coordinator()->AbortAll();

  run_loop.Run();

  registration = FindRegistrationForScope(
      options.scope, key, blink::ServiceWorkerStatusCode::kErrorNotFound);

  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

TEST_P(ServiceWorkerJobTest, AbortScope) {
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options1;
  options1.scope = GURL("https://www.example.com/1");
  const blink::StorageKey key1 = GetTestStorageKey(options1.scope);
  blink::mojom::ServiceWorkerRegistrationOptions options2;
  options2.scope = GURL("https://www.example.com/2");
  const blink::StorageKey key2 = GetTestStorageKey(options2.scope);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());
  job_coordinator()->Register(
      script_url, options1, key1,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kErrorAbort,
                       &registration1, barrier_closure),
      PolicyContainerPolicies());

  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      script_url, options2, key2,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kOk, &registration2,
                       barrier_closure),
      PolicyContainerPolicies());

  job_coordinator()->Abort(options1.scope, key1);

  run_loop.Run();

  registration1 = FindRegistrationForScope(
      options1.scope, key1, blink::ServiceWorkerStatusCode::kErrorNotFound);
  EXPECT_EQ(nullptr, registration1);

  registration2 = FindRegistrationForScope(options2.scope, key2,
                                           blink::ServiceWorkerStatusCode::kOk);
  EXPECT_NE(nullptr, registration2);
}

// Tests that the waiting worker enters the 'redundant' state upon
// unregistration.
TEST_P(ServiceWorkerJobTest, UnregisterWaitingSetsRedundant) {
  GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script_url, key, options);
  ASSERT_TRUE(registration.get());

  // Manually create the waiting worker since there is no way to become a
  // waiting worker until Update is implemented.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration.get(), script_url, blink::mojom::ScriptType::kClassic, 1L,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  version->set_policy_container_host(
      base::MakeRefCounted<PolicyContainerHost>(PolicyContainerPolicies()));
  ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
            StartServiceWorker(version.get()));

  EXPECT_EQ(version->fetch_handler_existence(),
            ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration->SetWaitingVersion(version);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, version->status());

  RunUnregisterJob(GURL("https://www.example.com/"), key);
  WaitForVersionRunningStatus(version, blink::EmbeddedWorkerStatus::kStopped);

  // The version should be stopped since there is no controllee after
  // unregistration.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
}

// Tests that the active worker enters the 'redundant' state upon
// unregistration.
TEST_P(ServiceWorkerJobTest, UnregisterActiveSetsRedundant) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> registration = RunRegisterJob(
      GURL("https://www.example.com/service_worker.js"), key, options);
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(registration->installing_version(), runner);
  ASSERT_TRUE(registration.get());

  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();
  observer.RunUntilStatusChange(version.get(), ServiceWorkerVersion::ACTIVATED);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  RunUnregisterJob(GURL("https://www.example.com/"), key);

  WaitForVersionRunningStatus(version, blink::EmbeddedWorkerStatus::kStopped);

  // The version should be stopped since there is no controllee after
  // unregistration.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
}

// Tests that the active worker enters the 'redundant' state upon
// unregistration.
TEST_P(ServiceWorkerJobTest,
       UnregisterActiveSetsRedundant_WaitForNoControllee) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> registration = RunRegisterJob(
      GURL("https://www.example.com/service_worker.js"), key, options);
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(registration->installing_version(), runner);
  ASSERT_TRUE(registration.get());

  ServiceWorkerClient* service_worker_client = CreateControllee();
  registration->active_version()->AddControllee(service_worker_client);

  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  RunUnregisterJob(GURL("https://www.example.com/"), key);

  // The version should be running since there is still a controllee.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  registration->active_version()->RemoveControllee(
      service_worker_client->client_uuid());
  WaitForVersionRunningStatus(version, blink::EmbeddedWorkerStatus::kStopped);

  // The version should be stopped since there is no controllee.
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
}

TEST_P(ServiceWorkerJobTest, RegisterSameWhileUninstalling) {
  GURL script("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Add a controllee and queue an unregister to force the uninstalling state.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();
  version->AddControllee(service_worker_client);
  RunUnregisterJob(options.scope, key);
  // Make sure the registration is deleted and purgable resources
  // set for purging once the version goes dead.
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(0u, GetPurgingResourceIdsForLiveVersion(
                    registration->active_version()->version_id())
                    .size());

  // Register same script again.
  EXPECT_EQ(registration, RunRegisterJob(script, key, options));
  // Wait until the worker becomes installed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetPurgingResourceIdsForLiveVersion(
                    registration->active_version()->version_id())
                    .size());

  EXPECT_EQ(version, registration->active_version());
  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_FALSE(registration->is_uninstalled());
  EXPECT_EQ(nullptr, registration->installing_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());
}

TEST_P(ServiceWorkerJobTest, RegisterSameWhileUninstallingAndUnregister) {
  GURL script("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Add a controllee and queue an unregister to force the uninstalling state.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();
  version->AddControllee(service_worker_client);
  RunUnregisterJob(options.scope, key);
  // Make sure the registration is deleted and purgable resources
  // set for purging once the version goes dead.
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(0u, GetPurgingResourceIdsForLiveVersion(
                    registration->active_version()->version_id())
                    .size());

  // Register same script again.
  EXPECT_EQ(registration, RunRegisterJob(script, key, options));
  // Wait until the worker becomes installed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, GetPurgingResourceIdsForLiveVersion(
                    registration->active_version()->version_id())
                    .size());

  EXPECT_EQ(version, registration->active_version());
  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_FALSE(registration->is_uninstalled());
  EXPECT_EQ(nullptr, registration->installing_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  // Unregister again and make sure its resources will be purged despite
  // purge being cancelled previously.
  RunUnregisterJob(options.scope, key);
  // Make sure the registration is deleted and purgable resources
  // set for purging once the version goes dead.
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(0u, GetPurgingResourceIdsForLiveVersion(
                    registration->active_version()->version_id())
                    .size());
}

TEST_P(ServiceWorkerJobTest, RegisterWhileUninstalling) {
  GURL script1("https://www.example.com/service_worker.js");
  GURL script2("https://www.example.com/service_worker.js?new");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  auto* initial_client =
      helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
          helper_.get());
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script1, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Add a controllee and queue an unregister to force the uninstalling state.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> old_version =
      registration->active_version();
  old_version->AddControllee(service_worker_client);
  RunUnregisterJob(options.scope, key);

  // Register another script.
  EXPECT_EQ(registration, RunRegisterJob(script2, key, options));
  // Wait until the worker becomes installed.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_EQ(old_version, registration->active_version());
  scoped_refptr<ServiceWorkerVersion> new_version =
      registration->waiting_version();

  // Verify the new version is installed but not activated yet.
  EXPECT_EQ(nullptr, registration->installing_version());
  EXPECT_TRUE(new_version);
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
            new_version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, new_version->status());

  // Make the old version eligible for eviction.
  old_version->RemoveControllee(service_worker_client->client_uuid());
  RequestTermination(&initial_client->host());

  // Wait for activated.
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(new_version.get(), runner);

  // Verify state after the new version is activated.
  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_FALSE(registration->is_uninstalled());
  EXPECT_EQ(nullptr, registration->installing_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(new_version, registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, new_version->status());
}

TEST_P(ServiceWorkerJobTest, RegisterAndUnregisterWhileUninstalling) {
  GURL script1("https://www.example.com/service_worker.js");
  GURL script2("https://www.example.com/service_worker.js?new");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script1, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();

  // Add a controllee and queue an unregister to force the uninstalling state.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> old_version =
      registration->active_version();
  old_version->AddControllee(service_worker_client);
  RunUnregisterJob(options.scope, key);

  EXPECT_EQ(registration, RunRegisterJob(script2, key, options));
  // Wait until the worker becomes installed.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(registration, FindRegistrationForScope(options.scope, key));
  scoped_refptr<ServiceWorkerVersion> new_version =
      registration->waiting_version();
  ASSERT_TRUE(new_version);

  // Unregister the registration (but it's still live).
  RunUnregisterJob(options.scope, key);
  // Now it's not found in the storage.
  RunUnregisterJob(options.scope, key,
                   blink::ServiceWorkerStatusCode::kErrorNotFound);

  FindRegistrationForScope(options.scope, key,
                           blink::ServiceWorkerStatusCode::kErrorNotFound);
  EXPECT_TRUE(registration->is_uninstalling());
  EXPECT_EQ(old_version, registration->active_version());

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
            old_version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, old_version->status());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning,
            new_version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, new_version->status());

  old_version->RemoveControllee(service_worker_client->client_uuid());

  WaitForVersionRunningStatus(old_version,
                              blink::EmbeddedWorkerStatus::kStopped);
  WaitForVersionRunningStatus(new_version,
                              blink::EmbeddedWorkerStatus::kStopped);

  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_TRUE(registration->is_uninstalled());

  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped,
            old_version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, old_version->status());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped,
            new_version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, new_version->status());
}

TEST_P(ServiceWorkerJobTest, RegisterSameScriptMultipleTimesWhileUninstalling) {
  GURL script1("https://www.example.com/service_worker.js");
  GURL script2("https://www.example.com/service_worker.js?new");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  auto* initial_client =
      helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
          helper_.get());
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script1, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Add a controllee and queue an unregister to force the uninstalling state.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> old_version =
      registration->active_version();
  old_version->AddControllee(service_worker_client);
  RunUnregisterJob(options.scope, key);

  EXPECT_EQ(registration, RunRegisterJob(script2, key, options));
  // Wait until the worker becomes installed.
  base::RunLoop().RunUntilIdle();

  scoped_refptr<ServiceWorkerVersion> new_version =
      registration->waiting_version();
  ASSERT_TRUE(new_version);

  RunUnregisterJob(options.scope, key);

  EXPECT_TRUE(registration->is_uninstalling());
  // Make sure it's deleted.
  base::RunLoop().RunUntilIdle();
  EXPECT_NE(
      0u,
      GetPurgingResourceIdsForLiveVersion(new_version->version_id()).size());

  EXPECT_EQ(registration, RunRegisterJob(script2, key, options));

  // Make sure it's installed and nothing will get purged.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      0u,
      GetPurgingResourceIdsForLiveVersion(new_version->version_id()).size());

  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_EQ(new_version, registration->waiting_version());

  old_version->RemoveControllee(service_worker_client->client_uuid());
  RequestTermination(&initial_client->host());

  // Wait for activated.
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(new_version.get(), runner);

  // Verify state after the new version is activated.
  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_FALSE(registration->is_uninstalled());
  EXPECT_EQ(nullptr, registration->installing_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(new_version, registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, new_version->status());
}

// Make sure that the new version is cleared up after trying to register a
// script with bad origin. (see https://crbug.com/1312995)
TEST_P(ServiceWorkerJobTest, RegisterBadOrigin) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  // http fails the trustworthiness check.
  options.scope = GURL("http://www.example.com/");
  GURL script_url("http://www.example.com/service_worker.js");

  base::RunLoop run_loop;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      script_url, options, GetTestStorageKey(options.scope),
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kErrorDisallowed,
                       &registration, run_loop.QuitClosure()),
      PolicyContainerPolicies());

  // Get a reference for the new version before it aborts the registration.
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilLiveVersion();
  scoped_refptr<ServiceWorkerVersion> version =
      helper_->context_wrapper()->GetLiveVersion(
          helper_->context_wrapper()->GetAllLiveVersionInfo()[0].version_id);
  ASSERT_TRUE(version);

  run_loop.Run();

  // Let everything release.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(registration);
  EXPECT_TRUE(version->HasOneRef());
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kStopped, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
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

TEST_P(ServiceWorkerJobTest, FetchHandlerType) {
  GURL script("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  scoped_refptr<ServiceWorkerRegistration> registration;

  auto* fetch_handler_worker =
      helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
          helper_.get());
  fetch_handler_worker->set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType::kNotSkippable);
  RunRegisterJob(script, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  registration = FindRegistrationForScope(options.scope, key);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::EXISTS,
            registration->active_version()->fetch_handler_existence());
  RunUnregisterJob(options.scope, key);

  auto* no_fetch_handler_worker =
      helper_->AddNewPendingInstanceClient<FetchHandlerInstanceClient>(
          helper_.get());
  no_fetch_handler_worker->set_fetch_handler_type(
      blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler);
  RunRegisterJob(script, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  registration = FindRegistrationForScope(options.scope, key);
  EXPECT_EQ(ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST,
            registration->active_version()->fetch_handler_existence());
  RunUnregisterJob(options.scope, key);
}

// Test that clients are alerted of new registrations if they are
// in-scope, so that Clients.claim() or ServiceWorkerContainer.ready work
// correctly.
TEST_P(ServiceWorkerJobTest, AddRegistrationToMatchingerHosts) {
  GURL scope("https://www.example.com/scope/");
  GURL in_scope("https://www.example.com/scope/page");
  GURL out_scope("https://www.example.com/page");

  // Make an in-scope client.
  ServiceWorkerClient* client = CreateControllee();
  client->UpdateUrls(in_scope, url::Origin::Create(in_scope),
                     GetTestStorageKey(in_scope));

  // Make an in-scope reserved client.
  ScopedServiceWorkerClient reserved_client =
      CreateServiceWorkerClient(helper_->context());
  reserved_client->UpdateUrls(in_scope, url::Origin::Create(in_scope),
                              GetTestStorageKey(in_scope));

  // Make an out-scope client.
  ServiceWorkerClient* out_scope_client = CreateControllee();
  out_scope_client->UpdateUrls(out_scope, url::Origin::Create(out_scope),
                               GetTestStorageKey(out_scope));

  // Make a new registration.
  GURL script("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script, GetTestStorageKey(options.scope), options);

  EXPECT_EQ(registration.get(), client->MatchRegistration());
  EXPECT_EQ(registration.get(), reserved_client->MatchRegistration());
  EXPECT_NE(registration.get(), out_scope_client->MatchRegistration());
}

namespace {  // Helpers for the update job tests.

const char kNoChangeOrigin[] = "https://nochange/";
const char kScope[] = "scope/";
const char kScript[] = "script.js";

const char kHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/javascript\n\n";
const char kBody[] = "/* old body */";
const char kNewBody[] = "/* new body */";

void WriteResponse(
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter>& writer,
    const std::string& headers,
    mojo_base::BigBuffer body) {
  int length = body.size();
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_time = base::Time::Now();
  response_head->response_time = base::Time::Now();
  response_head->headers = new net::HttpResponseHeaders(headers);
  response_head->content_length = length;

  int rv = -1234;
  {
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    writer->WriteResponseHead(std::move(response_head),
                              base::BindLambdaForTesting([&](int result) {
                                rv = result;
                                loop.Quit();
                              }));
    loop.Run();
    EXPECT_LT(0, rv);
  }

  rv = -1234;
  {
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    writer->WriteData(std::move(body),
                      base::BindLambdaForTesting([&](int result) {
                        rv = result;
                        loop.Quit();
                      }));
    loop.Run();
    EXPECT_EQ(length, rv);
  }
}

void WriteStringResponse(
    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter>& writer,
    const std::string& body) {
  mojo_base::BigBuffer body_buffer(base::as_bytes(base::make_span(body)));
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0\0";
  std::string headers(kHttpHeaders, std::size(kHttpHeaders));
  WriteResponse(writer, headers, std::move(body_buffer));
}

class UpdateJobTestHelper : public EmbeddedWorkerTestHelper,
                            public ServiceWorkerRegistration::Listener,
                            public ServiceWorkerContextCoreObserver {
 public:
  struct AttributeChangeLogEntry {
    int64_t registration_id;
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr mask;
    ServiceWorkerRegistrationInfo info;
  };

  struct StateChangeLogEntry {
    int64_t version_id;
    ServiceWorkerVersion::Status status;
  };

  UpdateJobTestHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {
    context_wrapper()->AddObserver(this);
    interceptor_ = std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
        &FakeNetwork::HandleRequest, base::Unretained(&fake_network_)));
    fake_network_.SetDefaultResponse(kHeaders, kBody,
                                     /*network_accessed=*/true, net::OK);
  }
  ~UpdateJobTestHelper() override {
    context_wrapper()->RemoveObserver(this);
    if (observed_registration_.get())
      observed_registration_->RemoveListener(this);
  }

  class ScriptFailureEmbeddedWorkerInstanceClient
      : public FakeEmbeddedWorkerInstanceClient {
   public:
    explicit ScriptFailureEmbeddedWorkerInstanceClient(
        UpdateJobTestHelper* helper)
        : FakeEmbeddedWorkerInstanceClient(helper) {}
    ~ScriptFailureEmbeddedWorkerInstanceClient() override = default;

    void StartWorker(
        blink::mojom::EmbeddedWorkerStartParamsPtr params) override {
      host().Bind(std::move(params->instance_host));
      start_params_ = std::move(params);

      helper()->OnServiceWorkerReceiver(
          std::move(start_params_->service_worker_receiver));
    }

    void SimulateFailureOfScriptEvaluation() {
      host()->OnScriptEvaluationStart();
      host()->OnStarted(
          blink::mojom::ServiceWorkerStartStatus::kAbruptCompletion,
          blink::mojom::ServiceWorkerFetchHandlerType::kNoHandler,
          /*has_hid_event_handlers=*/false, /*has_usb_event_handlers=*/false,
          helper()->GetNextThreadId(),
          blink::mojom::EmbeddedWorkerStartTiming::New());
    }

   private:
    blink::mojom::EmbeddedWorkerStartParamsPtr start_params_;
  };

  class ScriptFailureServiceWorker : public FakeServiceWorker {
   public:
    ScriptFailureServiceWorker(
        EmbeddedWorkerTestHelper* helper,
        ScriptFailureEmbeddedWorkerInstanceClient* client)
        : FakeServiceWorker(helper), client_(client) {}

    void InitializeGlobalScope(
        mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerHost>,
        mojo::PendingAssociatedRemote<
            blink::mojom::AssociatedInterfaceProvider>,
        mojo::PendingAssociatedReceiver<
            blink::mojom::AssociatedInterfaceProvider>,
        blink::mojom::ServiceWorkerRegistrationObjectInfoPtr,
        blink::mojom::ServiceWorkerObjectInfoPtr,
        blink::mojom::FetchHandlerExistence,
        mojo::PendingReceiver<blink::mojom::ReportingObserver>,
        blink::mojom::AncestorFrameType,
        const blink::StorageKey& storage_key) override {
      client_->SimulateFailureOfScriptEvaluation();
      // Set `client_` to nullptr to prevent it from dangling, since
      // `SimulateFailureOfScriptEvaluation()` will ensure failure and destroy
      // Client first then Worker which makes `client_` dangling.
      client_ = nullptr;
    }

   private:
    raw_ptr<ScriptFailureEmbeddedWorkerInstanceClient> client_;
  };

  ServiceWorkerJobCoordinator* job_coordinator() {
    return context()->job_coordinator();
  }

  scoped_refptr<ServiceWorkerRegistration> SetupInitialRegistration(
      const GURL& test_origin,
      const blink::StorageKey& test_storage_key,
      bool store_worker_instance = false) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = test_origin.Resolve(kScope);
    scoped_refptr<ServiceWorkerRegistration> registration;

    auto client = std::make_unique<FakeEmbeddedWorkerInstanceClient>(this);
    // Only store the worker instance for specific test cases, such as
    // `Update_NewVersion` Otherwise, it will crash other tests such as
    // `Update_EvictedIncumbent` since
    // `initial_embedded_worker_instance_client_` becomes dangling for those
    // cases.
    if (store_worker_instance) {
      initial_embedded_worker_instance_client_ = client.get();
    }

    AddPendingInstanceClient(std::move(client));
    base::RunLoop run_loop;
    job_coordinator()->Register(
        test_origin.Resolve(kScript), options, test_storage_key,
        blink::mojom::FetchClientSettingsObject::New(),
        /*requesting_frame_id=*/GlobalRenderFrameHostId(),
        /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
        SaveRegistration(blink::ServiceWorkerStatusCode::kOk, &registration,
                         run_loop.QuitClosure()),
        PolicyContainerPolicies());
    run_loop.Run();
    // Wait until the worker becomes active.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registration.get());
    EXPECT_TRUE(registration->active_version());
    EXPECT_FALSE(registration->installing_version());
    EXPECT_FALSE(registration->waiting_version());
    observed_registration_ = registration;
    return registration;
  }

  // EmbeddedWorkerTestHelper overrides:
  void PopulateScriptCacheMap(int64_t version_id,
                              base::OnceClosure callback) override {
    context()->GetStorageControl()->GetNewResourceId(base::BindOnce(
        &UpdateJobTestHelper::DidGetNewResourceIdForScriptCache,
        weak_factory_.GetWeakPtr(), version_id, std::move(callback)));
  }

  void DidGetNewResourceIdForScriptCache(int64_t version_id,
                                         base::OnceClosure callback,
                                         int64_t resource_id) {
    ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
    ASSERT_TRUE(version);
    scoped_refptr<ServiceWorkerRegistration> registration =
        context()->GetLiveRegistration(version->registration_id());
    ASSERT_TRUE(registration);
    GURL script = version->script_url();
    bool is_update = registration->active_version() &&
                     version != registration->active_version();

    mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
    context()->GetStorageControl()->CreateResourceWriter(
        resource_id, writer.BindNewPipeAndPassReceiver());
    version->script_cache_map()->NotifyStartedCaching(script, resource_id);
    if (!is_update) {
      // Spoof caching the script for the initial version.
      WriteStringResponse(writer, kBody);
      version->script_cache_map()->NotifyFinishedCaching(
          script, std::size(kBody), /*sha256_checksum=*/"", net::OK,
          std::string());
    } else {
      EXPECT_NE(GURL(kNoChangeOrigin), script.DeprecatedGetOriginAsURL());
      // The script must be changed.
      WriteStringResponse(writer, kNewBody);
      version->script_cache_map()->NotifyFinishedCaching(
          script, std::size(kNewBody), /*sha256_checksum=*/"", net::OK,
          std::string());
    }

    version->SetMainScriptResponse(CreateMainScriptResponse());
    std::move(callback).Run();
  }

  // ServiceWorkerContextCoreObserver overrides
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status status) override {
    StateChangeLogEntry entry;
    entry.version_id = version_id;
    entry.status = status;
    state_change_log_.push_back(std::move(entry));
  }

  // ServiceWorkerRegistration::Listener overrides
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) override {
    AttributeChangeLogEntry entry;
    entry.registration_id = registration->id();
    entry.mask = std::move(changed_mask);
    entry.info = registration->GetInfo();
    attribute_change_log_.push_back(std::move(entry));
  }

  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override {
    registration_failed_ = true;
  }

  void OnUpdateFound(ServiceWorkerRegistration* registration) override {
    update_found_ = true;
  }

  raw_ptr<FakeEmbeddedWorkerInstanceClient>
      initial_embedded_worker_instance_client_ = nullptr;
  scoped_refptr<ServiceWorkerRegistration> observed_registration_;
  std::vector<AttributeChangeLogEntry> attribute_change_log_;
  std::vector<StateChangeLogEntry> state_change_log_;
  bool update_found_ = false;
  bool registration_failed_ = false;
  bool force_start_worker_failure_ = false;
  std::optional<bool> will_be_terminated_;

  FakeNetwork fake_network_;
  std::unique_ptr<URLLoaderInterceptor> interceptor_;

  base::WeakPtrFactory<UpdateJobTestHelper> weak_factory_{this};
};

}  // namespace

// This class is for cases that can be impacted by different update check
// types.
class ServiceWorkerUpdateJobTest : public ServiceWorkerJobTest {
 public:
  void SetUp() override {
    update_helper_ = new UpdateJobTestHelper();
    helper_.reset(update_helper_);
    // Reset the mock loader because this test creates a storage partition and
    // it makes GetLoaderFactoryForUpdateCheck() work correctly.
    helper_->context_wrapper()->SetLoaderFactoryForUpdateCheckForTest(nullptr);

    // Create a StoragePartition with the testing browser context so that the
    // ServiceWorkerUpdateChecker can find the BrowserContext through it.
    storage_partition_impl_ = StoragePartitionImpl::Create(
        helper_->browser_context(),
        CreateStoragePartitionConfigForTesting(/*in_memory=*/true),
        base::FilePath() /* relative_partition_path */);
    storage_partition_impl_->Initialize();
    helper_->context_wrapper()->set_storage_partition(
        storage_partition_impl_.get());
  }

  void TearDown() override {
    // These need to be cleared before `helper_` to avoid dangling pointers.
    storage_partition_impl_->OnBrowserContextWillBeDestroyed();
    storage_partition_impl_.reset();
    update_helper_ = nullptr;

    ServiceWorkerJobTest::TearDown();
  }

 protected:
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
  raw_ptr<UpdateJobTestHelper> update_helper_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ServiceWorkerUpdateJobTest,
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

// Make sure that the same registration is used and the update_via_cache value
// is updated when registering a service worker with the same parameter except
// for updateViaCache.
TEST_P(ServiceWorkerUpdateJobTest, RegisterWithDifferentUpdateViaCache) {
  const GURL script_url("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  scoped_refptr<ServiceWorkerRegistration> old_registration =
      RunRegisterJob(script_url, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kImports,
            old_registration->update_via_cache());

  // During the above registration, a service worker registration object host
  // for ServiceWorkerGlobalScope#registration has been created/added into
  // |worker_host|.
  ServiceWorkerHost* worker_host =
      old_registration->active_version()->worker_host();
  ASSERT_TRUE(worker_host);
  ServiceWorkerContainerHost* container_host = worker_host->container_host();

  // Remove references to |old_registration| so that |old_registration| is the
  // only reference to the registration.
  container_host->version_object_manager().service_worker_object_hosts_.clear();
  EXPECT_EQ(1UL, container_host->registration_object_manager()
                     .registration_object_hosts_.size());
  container_host->registration_object_manager()
      .registration_object_hosts_.clear();
  EXPECT_EQ(0UL, container_host->registration_object_manager()
                     .registration_object_hosts_.size());
  EXPECT_TRUE(old_registration->HasOneRef());

  EXPECT_TRUE(FindRegistrationForScope(options.scope, key));

  base::HistogramTester histogram_tester;
  options.update_via_cache = blink::mojom::ServiceWorkerUpdateViaCache::kNone;
  scoped_refptr<ServiceWorkerRegistration> new_registration =
      RunRegisterJob(script_url, key, options);

  // Ensure that the registration object is not copied.
  EXPECT_EQ(old_registration, new_registration);
  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kNone,
            new_registration->update_via_cache());

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_scope =
      FindRegistrationForScope(options.scope, key);

  EXPECT_EQ(new_registration_by_scope, old_registration);
}

TEST_P(ServiceWorkerUpdateJobTest, Update_NoChange) {
  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper_->SetupInitialRegistration(
          GURL(kNoChangeOrigin), GetTestStorageKey(GURL(kNoChangeOrigin)));
  ASSERT_TRUE(registration.get());
  ASSERT_EQ(4u, update_helper_->state_change_log_.size());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING,
            update_helper_->state_change_log_[0].status);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            update_helper_->state_change_log_[1].status);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING,
            update_helper_->state_change_log_[2].status);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            update_helper_->state_change_log_[3].status);
  update_helper_->state_change_log_.clear();

  // Run the update job.
  base::HistogramTester histogram_tester;
  registration->AddListener(update_helper_);
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  first_version->StartUpdate();
  base::RunLoop().RunUntilIdle();

  // Verify results.
  ASSERT_TRUE(registration->active_version());
  EXPECT_EQ(first_version.get(), registration->active_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_FALSE(registration->waiting_version());
  EXPECT_TRUE(update_helper_->attribute_change_log_.empty());
  EXPECT_FALSE(update_helper_->update_found_);
}

TEST_P(ServiceWorkerUpdateJobTest, Update_BumpLastUpdateCheckTime) {
  const base::Time kToday = base::Time::Now();
  const base::Time kYesterday = kToday - base::Days(1) - base::Hours(1);
  const GURL kNewVersionOrigin("https://newversion/");

  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper_->SetupInitialRegistration(
          GURL(kNoChangeOrigin), GetTestStorageKey(GURL(kNoChangeOrigin)));
  ASSERT_TRUE(registration.get());

  registration->AddListener(update_helper_);

  // Run an update where the script did not change and the network was not
  // accessed. The check time should not be updated.
  // Set network not accessed.
  update_helper_->fake_network_.SetResponse(
      GURL(kNoChangeOrigin).Resolve(kScript), kHeaders, kBody,
      /*network_accessed=*/false, net::OK);

  {
    base::HistogramTester histogram_tester;
    registration->set_last_update_check(kToday);
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(kToday, registration->last_update_check());
    EXPECT_FALSE(update_helper_->update_found_);
  }

  // Run an update where the script did not change and the network was
  // accessed. The check time should be updated.
  // Set network accessed.
  update_helper_->fake_network_.SetResponse(
      GURL(kNoChangeOrigin).Resolve(kScript), kHeaders, kBody,
      /*network_accessed=*/true, net::OK);

  {
    base::HistogramTester histogram_tester;
    registration->set_last_update_check(kYesterday);
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(kYesterday, registration->last_update_check());
    EXPECT_FALSE(update_helper_->update_found_);
    registration->RemoveListener(update_helper_);
    registration = update_helper_->SetupInitialRegistration(
        kNewVersionOrigin, GetTestStorageKey(kNewVersionOrigin));
    ASSERT_TRUE(registration.get());
  }

  registration->AddListener(update_helper_);

  // Run an update where the script changed. The check time should be updated.
  // Change script body.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeaders, kNewBody,
                                            /*network_accessed=*/true, net::OK);
  {
    base::HistogramTester histogram_tester;
    registration->set_last_update_check(kYesterday);
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(kYesterday, registration->last_update_check());
  }

  // Run an update to a worker that loads successfully but fails to start up
  // (script evaluation failure). The check time should be updated.
  auto* embedded_worker_instance_client =
      update_helper_->AddNewPendingInstanceClient<
          UpdateJobTestHelper::ScriptFailureEmbeddedWorkerInstanceClient>(
          update_helper_);
  update_helper_->AddNewPendingServiceWorker<
      UpdateJobTestHelper::ScriptFailureServiceWorker>(
      update_helper_, embedded_worker_instance_client);
  registration->set_last_update_check(kYesterday);
  // Change script body.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeaders, kBody,
                                            /*network_accessed=*/true, net::OK);
  {
    base::HistogramTester histogram_tester;
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(kYesterday, registration->last_update_check());
  }
}

TEST_P(ServiceWorkerUpdateJobTest, Update_NewVersion) {
  const GURL kNewVersionOrigin("https://newversion/");

  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper_->SetupInitialRegistration(
          kNewVersionOrigin, GetTestStorageKey(kNewVersionOrigin),
          /*store_worker_instance=*/true);
  ASSERT_TRUE(registration.get());
  update_helper_->state_change_log_.clear();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Run the update job and an update is found.
  // Change script body.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeaders, kNewBody,
                                            /*network_accessed=*/true, net::OK);

  base::HistogramTester histogram_tester;
  registration->AddListener(update_helper_);
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  first_version->StartUpdate();
  base::RunLoop().RunUntilIdle();

  // The worker is updated after RequestTermination() is called from the
  // renderer. Until then, the active version stays active.
  EXPECT_EQ(first_version.get(), registration->active_version());
  // The new worker is installed but not yet to be activated.
  scoped_refptr<ServiceWorkerVersion> new_version =
      registration->waiting_version();
  EXPECT_EQ(2u, update_helper_->attribute_change_log_.size());
  RequestTermination(
      &(update_helper_->initial_embedded_worker_instance_client_->host()));
  // Setting `initial_embedded_worker_instance_client_` to null to prevent it
  // from dangling.
  update_helper_->initial_embedded_worker_instance_client_ = nullptr;

  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(new_version.get(), runner);

  // Pump the loop again. This ensures |update_helper_| observes all
  // the status changes, since RunUntilActivated() only ensured
  // ServiceWorkerJobTest did.
  base::RunLoop().RunUntilIdle();

  // Verify results.
  ASSERT_TRUE(registration->active_version());
  EXPECT_NE(first_version.get(), registration->active_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_FALSE(registration->waiting_version());
  ASSERT_EQ(3u, update_helper_->attribute_change_log_.size());

  {
    const UpdateJobTestHelper::AttributeChangeLogEntry& entry =
        update_helper_->attribute_change_log_[0];
    EXPECT_TRUE(entry.mask->installing);
    EXPECT_FALSE(entry.mask->waiting);
    EXPECT_FALSE(entry.mask->active);
    EXPECT_NE(entry.info.installing_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
    EXPECT_EQ(entry.info.waiting_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
    EXPECT_NE(entry.info.active_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
  }

  {
    const UpdateJobTestHelper::AttributeChangeLogEntry& entry =
        update_helper_->attribute_change_log_[1];
    EXPECT_TRUE(entry.mask->installing);
    EXPECT_TRUE(entry.mask->waiting);
    EXPECT_FALSE(entry.mask->active);
    EXPECT_EQ(entry.info.installing_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
    EXPECT_NE(entry.info.waiting_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
    EXPECT_NE(entry.info.active_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
  }

  {
    const UpdateJobTestHelper::AttributeChangeLogEntry& entry =
        update_helper_->attribute_change_log_[2];
    EXPECT_FALSE(entry.mask->installing);
    EXPECT_TRUE(entry.mask->waiting);
    EXPECT_TRUE(entry.mask->active);
    EXPECT_EQ(entry.info.installing_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
    EXPECT_EQ(entry.info.waiting_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
    EXPECT_NE(entry.info.active_version.version_id,
              blink::mojom::kInvalidServiceWorkerVersionId);
  }

  // expected version state transitions:
  // new.installing, new.installed,
  // old.redundant,
  // new.activating, new.activated
  ASSERT_EQ(5u, update_helper_->state_change_log_.size());

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper_->state_change_log_[0].version_id);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING,
            update_helper_->state_change_log_[0].status);

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper_->state_change_log_[1].version_id);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            update_helper_->state_change_log_[1].status);

  EXPECT_EQ(first_version->version_id(),
            update_helper_->state_change_log_[2].version_id);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT,
            update_helper_->state_change_log_[2].status);

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper_->state_change_log_[3].version_id);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING,
            update_helper_->state_change_log_[3].status);

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper_->state_change_log_[4].version_id);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            update_helper_->state_change_log_[4].status);

  EXPECT_TRUE(update_helper_->update_found_);
}

// Test that the update job uses the script URL of the newest worker when the
// job starts, rather than when it is scheduled.
TEST_P(ServiceWorkerUpdateJobTest, Update_ScriptUrlChanged) {
  const GURL old_script("https://www.example.com/service_worker.js");
  const GURL new_script("https://www.example.com/new_worker.js");

  // Create a registration with an active version.
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);
  auto* initial_client =
      helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
          helper_.get());
  // Setup the old script response.
  update_helper_->fake_network_.SetResponse(old_script, kHeaders, kBody,
                                            /*network_accessed=*/true, net::OK);
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(old_script, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Queue an Update. When this runs, it will use the waiting version's script.
  job_coordinator()->Update(registration.get(), false, false,
                            blink::mojom::FetchClientSettingsObject::New(),
                            base::NullCallback());

  // Add a waiting version with a new script.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration.get(), new_script, blink::mojom::ScriptType::kClassic,
      2L /* dummy version id */,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
      helper_->context()->AsWeakPtr());
  registration->SetWaitingVersion(version);

  // Setup the new script response.
  update_helper_->fake_network_.SetResponse(new_script, kHeaders, kNewBody,
                                            /*network_accessed=*/true, net::OK);

  // Make sure the storage has the data of the current waiting version.
  const int64_t resource_id = 2;
  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer;
  context()->GetStorageControl()->CreateResourceWriter(
      resource_id, writer.BindNewPipeAndPassReceiver());
  version->script_cache_map()->NotifyStartedCaching(new_script, resource_id);
  WriteStringResponse(writer, kBody);
  version->script_cache_map()->NotifyFinishedCaching(
      new_script, std::size(kBody), /*sha256_checksum=*/"", net::OK,
      std::string());

  // Run the update job.
  base::RunLoop().RunUntilIdle();

  // The worker is activated after RequestTermination() is called from the
  // renderer. Until then, the active version stays active.
  // Still waiting, but the waiting version isn't |version| since another
  // ServiceWorkerVersion is created during the update job and the job wipes
  // out the older waiting version.
  ServiceWorkerVersion* waiting_version = registration->waiting_version();
  EXPECT_TRUE(registration->active_version());
  EXPECT_TRUE(waiting_version);
  EXPECT_NE(version.get(), waiting_version);

  RequestTermination(&initial_client->host());
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(waiting_version, runner);

  // The update job should have created a new version with the new script,
  // and promoted it to the active version.
  EXPECT_EQ(new_script, registration->active_version()->script_url());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(nullptr, registration->installing_version());
}

// Test that update fails if the incumbent worker was evicted
// during the update job (this can happen on disk cache failure).
TEST_P(ServiceWorkerUpdateJobTest, Update_EvictedIncumbent) {
  const GURL kNewVersionOrigin("https://newversion/");

  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper_->SetupInitialRegistration(
          kNewVersionOrigin, GetTestStorageKey(kNewVersionOrigin));
  ASSERT_TRUE(registration.get());
  update_helper_->state_change_log_.clear();

  registration->AddListener(update_helper_);
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  auto* instance_client = helper_->AddNewPendingInstanceClient<
      DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get());

  // Start the update job and make it block on the worker starting.
  // Evict the incumbent during that time.
  // Change script body.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeaders, kNewBody,
                                            /*network_accessed=*/true, net::OK);

  first_version->StartUpdate();
  instance_client->RunUntilStartWorker();
  registration->ForceDelete();

  // Finish the update job.
  instance_client->UnblockStartWorker();
  base::RunLoop().RunUntilIdle();

  // Verify results.
  EXPECT_FALSE(registration->GetNewestVersion());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, first_version->status());
  EXPECT_TRUE(update_helper_->attribute_change_log_.empty());
  EXPECT_FALSE(update_helper_->update_found_);
  EXPECT_TRUE(update_helper_->registration_failed_);
  EXPECT_TRUE(registration->is_uninstalled());
}

TEST_P(ServiceWorkerUpdateJobTest, Update_UninstallingRegistration) {
  const GURL scope("https://www.example.com/one/");

  // Create a registration with a controllee and queue an unregister to force
  // the uninstalling state.
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateRegistrationWithControllee(
          GURL("https://www.example.com/service_worker.js"), scope);

  ServiceWorkerVersion* active_version = registration->active_version();

  base::RunLoop run_loop;
  job_coordinator()->Unregister(
      scope, GetTestStorageKey(scope),
      /*is_immediate=*/false,
      SaveUnregistration(blink::ServiceWorkerStatusCode::kOk,
                         run_loop.QuitClosure()));

  // Update should abort after it starts and sees uninstalling.
  job_coordinator()->Update(registration.get(), false, false,
                            blink::mojom::FetchClientSettingsObject::New(),
                            base::NullCallback());

  run_loop.Run();

  // Verify the registration was not modified by the Update.
  EXPECT_TRUE(registration->is_uninstalling());
  EXPECT_EQ(active_version, registration->active_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(nullptr, registration->installing_version());
}

TEST_P(ServiceWorkerUpdateJobTest, RegisterMultipleTimesWhileUninstalling) {
  GURL script1("https://www.example.com/service_worker.js?first");
  GURL script2("https://www.example.com/service_worker.js?second");
  GURL script3("https://www.example.com/service_worker.js?third");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/one/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  auto* initial_client =
      helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
          helper_.get());
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script1, key, options);
  // Wait until the worker becomes actvie.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Add a controllee and queue an unregister to force the uninstalling state.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  first_version->AddControllee(service_worker_client);
  RunUnregisterJob(options.scope, key);

  EXPECT_EQ(registration, RunRegisterJob(script2, key, options));
  // Wait until the worker becomes installed.
  base::RunLoop().RunUntilIdle();

  scoped_refptr<ServiceWorkerVersion> second_version =
      registration->waiting_version();
  ASSERT_TRUE(second_version);

  RunUnregisterJob(options.scope, key);

  EXPECT_TRUE(registration->is_uninstalling());

  EXPECT_EQ(registration, RunRegisterJob(script3, key, options));
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilStatusChange(second_version.get(),
                                ServiceWorkerVersion::REDUNDANT);
  scoped_refptr<ServiceWorkerVersion> third_version =
      registration->waiting_version();
  ASSERT_TRUE(third_version);

  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, second_version->status());

  first_version->RemoveControllee(service_worker_client->client_uuid());
  RequestTermination(&initial_client->host());

  // Wait for activated.
  observer.RunUntilActivated(third_version.get(), runner);

  // Verify the new version is activated.
  EXPECT_FALSE(registration->is_uninstalling());
  EXPECT_FALSE(registration->is_uninstalled());
  EXPECT_EQ(nullptr, registration->installing_version());
  EXPECT_EQ(nullptr, registration->waiting_version());
  EXPECT_EQ(third_version, registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, third_version->status());
}

// Test that activation doesn't complete if it's triggered by removing a
// controllee and starting the worker failed due to shutdown.
TEST_P(ServiceWorkerUpdateJobTest, ActivateCancelsOnShutdown) {
  GURL script("https://www.example.com/service_worker.js");
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  auto* initial_client =
      helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
          helper_.get());
  scoped_refptr<ServiceWorkerRegistration> registration =
      RunRegisterJob(script, key, options);
  // Wait until the worker becomes active.
  base::RunLoop().RunUntilIdle();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  registration->SetTaskRunnerForTest(runner);

  // Add a controllee.
  ServiceWorkerClient* service_worker_client = CreateControllee();
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  first_version->AddControllee(service_worker_client);

  // Update. The new version should be waiting.
  // Change script body.
  update_helper_->fake_network_.SetResponse(script, kHeaders, kNewBody,
                                            /*network_accessed=*/true, net::OK);

  registration->AddListener(update_helper_);
  first_version->StartUpdate();
  base::RunLoop().RunUntilIdle();
  scoped_refptr<ServiceWorkerVersion> new_version =
      registration->waiting_version();
  ASSERT_TRUE(new_version);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, new_version->status());

  // Stop the worker so that it must start again when activation is attempted.
  // (This is not strictly necessary to exercise the codepath, but it makes it
  // easy to cause a failure with set_force_start_worker_failure after
  // shutdown is simulated. Otherwise our test helper often fails on
  // DCHECK(context)).
  new_version->StopWorker(base::DoNothing());

  // Remove the controllee. The new version should be activating, and delayed
  // until the runner runs again.
  first_version->RemoveControllee(service_worker_client->client_uuid());
  base::RunLoop().RunUntilIdle();

  // Activating the new version won't happen until
  // RequestTermination() is called.
  EXPECT_EQ(first_version.get(), registration->active_version());
  RequestTermination(&initial_client->host());

  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilStatusChange(new_version.get(),
                                ServiceWorkerVersion::ACTIVATING);
  EXPECT_EQ(new_version.get(), registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, new_version->status());

  // Shutdown.
  update_helper_->context()->wrapper()->Shutdown();
  auto* embedded_worker_instance_client =
      update_helper_->AddNewPendingInstanceClient<
          UpdateJobTestHelper::ScriptFailureEmbeddedWorkerInstanceClient>(
          update_helper_);
  update_helper_->AddNewPendingServiceWorker<
      UpdateJobTestHelper::ScriptFailureServiceWorker>(
      update_helper_, embedded_worker_instance_client);

  // Allow the activation to continue. It will fail, and the worker
  // should not be promoted to ACTIVATED because failure occur
  // during shutdown.
  runner->RunPendingTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(new_version.get(), registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING, new_version->status());
  registration->RemoveListener(update_helper_);
  // Dispatch Mojo messages for those Mojo interfaces bound on |runner| to
  // avoid possible memory leak.
  runner->RunUntilIdle();
}

// Update job should handle the COEP header appropriately.
TEST_P(ServiceWorkerUpdateJobTest, Update_CrossOriginEmbedderPolicyValue) {
  const GURL kNewVersionOrigin("https://newversion/");
  const char kHeadersWithRequireCorp[] = R"(HTTP/1.1 200 OK
Content-Type: application/javascript
Cross-Origin-Embedder-Policy: require-corp

)";
  const char kHeadersWithNone[] = R"(HTTP/1.1 200 OK
Content-Type: application/javascript
Cross-Origin-Embedder-Policy: none

)";

  const base::Time kToday = base::Time::Now();
  const base::Time kYesterday = kToday - base::Days(1) - base::Hours(1);

  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper_->SetupInitialRegistration(
          kNewVersionOrigin, GetTestStorageKey(kNewVersionOrigin));
  ASSERT_TRUE(registration.get());
  // COEP is populated here because the worker's script is loaded as a part of
  // the start worker sequence before registration and the response header is
  // reflected to the version at that point
  EXPECT_THAT(registration->active_version()->cross_origin_embedder_policy(),
              Pointee(Eq(CrossOriginEmbedderPolicyNone())));

  registration->AddListener(update_helper_);

  // Run an update where the response header is updated but the script did not
  // change. No update is found but the last update check time is updated.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeadersWithRequireCorp, kBody,
                                            /*network_accessed=*/true, net::OK);

  {
    base::HistogramTester histogram_tester;
    registration->set_last_update_check(kYesterday);
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(kYesterday, registration->last_update_check());
    EXPECT_FALSE(update_helper_->update_found_);
  }

  // Run an update where the COEP value and the script changed.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeadersWithRequireCorp, kNewBody,
                                            /*network_accessed=*/true, net::OK);
  {
    base::HistogramTester histogram_tester;
    registration->set_last_update_check(kYesterday);
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(kYesterday, registration->last_update_check());
    EXPECT_TRUE(update_helper_->update_found_);
    ASSERT_NE(nullptr, registration->waiting_version());
    EXPECT_THAT(registration->waiting_version()->cross_origin_embedder_policy(),
                Pointee(Eq(CrossOriginEmbedderPolicyRequireCorp())));
  }

  // Run an update again where the COEP value and the body has been updated. The
  // COEP value should be updated appropriately.
  update_helper_->fake_network_.SetResponse(kNewVersionOrigin.Resolve(kScript),
                                            kHeadersWithNone, kBody,
                                            /*network_accessed=*/true, net::OK);
  {
    base::HistogramTester histogram_tester;
    registration->set_last_update_check(kYesterday);
    registration->active_version()->StartUpdate();
    base::RunLoop().RunUntilIdle();
    EXPECT_LT(kYesterday, registration->last_update_check());
    EXPECT_TRUE(update_helper_->update_found_);
    ASSERT_NE(nullptr, registration->waiting_version());
    EXPECT_THAT(registration->waiting_version()->cross_origin_embedder_policy(),
                Pointee(Eq(CrossOriginEmbedderPolicyNone())));
  }
}

class WaitForeverInstallWorker : public FakeServiceWorker {
 public:
  WaitForeverInstallWorker(EmbeddedWorkerTestHelper* helper)
      : FakeServiceWorker(helper) {}
  ~WaitForeverInstallWorker() override = default;

  void DispatchInstallEvent(
      blink::mojom::ServiceWorker::DispatchInstallEventCallback callback)
      override {
    callback_ = std::move(callback);
  }

 private:
  blink::mojom::ServiceWorker::DispatchInstallEventCallback callback_;
};

// Test that the job queue doesn't get stuck by bad workers.
TEST_P(ServiceWorkerJobTest, TimeoutBadJobs) {
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = GURL("https://www.example.com/");
  const blink::StorageKey key = GetTestStorageKey(options.scope);

  // Make a job that gets stuck due to a worker stalled in starting.
  base::RunLoop loop1;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  helper_->AddPendingInstanceClient(
      std::make_unique<DelayedFakeEmbeddedWorkerInstanceClient>(helper_.get()));
  job_coordinator()->Register(
      GURL("https://www.example.com/service_worker1.js"), options, key,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kErrorTimeout,
                       &registration1, loop1.QuitClosure()),
      PolicyContainerPolicies());

  task_environment_.FastForwardBy(base::Minutes(1));

  // Make a job that gets stuck due to a worker that doesn't finish the install
  // event. The callback is called with kOk, but the job will be stuck until
  // the install event times out.
  base::RunLoop loop2;
  helper_->AddPendingServiceWorker(
      std::make_unique<WaitForeverInstallWorker>(helper_.get()));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      GURL("https://www.example.com/service_worker2.js"), options, key,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kOk, &registration2,
                       loop2.QuitClosure()),
      PolicyContainerPolicies());

  task_environment_.FastForwardBy(base::Minutes(1));

  // Make a normal job.
  base::RunLoop loop3;
  scoped_refptr<ServiceWorkerRegistration> registration3;
  job_coordinator()->Register(
      GURL("https://www.example.com/service_worker3.js"), options, key,
      blink::mojom::FetchClientSettingsObject::New(),
      /*requesting_frame_id=*/GlobalRenderFrameHostId(),
      /*ancestor_frame_type=*/blink::mojom::AncestorFrameType::kNormalFrame,
      SaveRegistration(blink::ServiceWorkerStatusCode::kOk, &registration3,
                       loop3.QuitClosure()),
      PolicyContainerPolicies());

  task_environment_.FastForwardBy(base::Minutes(1));

  // Timeout the first job.
  task_environment_.FastForwardBy(base::Minutes(2));
  loop1.Run();

  // Let the second job run until the install event is dispatched, then
  // time out the event.
  loop2.Run();
  scoped_refptr<ServiceWorkerVersion> version =
      registration2->installing_version();
  ASSERT_TRUE(version);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING, version->status());
  task_environment_.FastForwardBy(base::Minutes(5));
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());

  // Let the third job finish successfully. It might have already been
  // progressing so we don't know what state its worker is in, but it will
  // eventually reach ACTIVATED.
  loop3.Run();
  version = registration3->GetNewestVersion();
  ASSERT_TRUE(version);
  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilStatusChange(version.get(), ServiceWorkerVersion::ACTIVATED);
}

}  // namespace content

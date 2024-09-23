// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/test_service_worker_observer.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/test/fake_network.h"
#include "content/test/storage_partition_test_helpers.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// From service_worker_registration.cc.
constexpr base::TimeDelta kMaxLameDuckTime = base::Minutes(5);

int CreateInflightRequest(ServiceWorkerVersion* version) {
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, StartServiceWorker(version));
  return version->StartRequest(ServiceWorkerMetrics::EventType::PUSH,
                               base::DoNothing());
}

void SaveStatusCallback(bool* called,
                        blink::ServiceWorkerStatusCode* out,
                        blink::ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

}  // namespace

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  AllowServiceWorkerResult AllowServiceWorker(
      const GURL& scope,
      const net::SiteForCookies& site_for_cookies,
      const std::optional<url::Origin>& top_frame_origin,
      const GURL& script_url,
      content::BrowserContext* context) override {
    return AllowServiceWorkerResult::No();
  }
};

void RequestTermination(
    mojo::AssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>* host) {
  // We can't wait for the callback since Stop() arrives first which severs
  // the connection.
  (*host)->RequestTermination(base::DoNothing());
}

class MockServiceWorkerRegistrationObject
    : public blink::mojom::ServiceWorkerRegistrationObject {
 public:
  explicit MockServiceWorkerRegistrationObject(
      mojo::PendingAssociatedReceiver<
          blink::mojom::ServiceWorkerRegistrationObject> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~MockServiceWorkerRegistrationObject() override = default;

  int update_found_called_count() const { return update_found_called_count_; }
  int set_version_attributes_called_count() const {
    return set_version_attributes_called_count_;
  }
  int set_update_via_cache_called_count() const {
    return set_update_via_cache_called_count_;
  }
  const blink::mojom::ChangedServiceWorkerObjectsMask& changed_mask() const {
    return *changed_mask_;
  }
  const blink::mojom::ServiceWorkerObjectInfoPtr& installing() const {
    return installing_;
  }
  const blink::mojom::ServiceWorkerObjectInfoPtr& waiting() const {
    return waiting_;
  }
  const blink::mojom::ServiceWorkerObjectInfoPtr& active() const {
    return active_;
  }
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache() const {
    return update_via_cache_;
  }

 private:
  // Implements blink::mojom::ServiceWorkerRegistrationObject.
  void SetServiceWorkerObjects(
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
      blink::mojom::ServiceWorkerObjectInfoPtr installing,
      blink::mojom::ServiceWorkerObjectInfoPtr waiting,
      blink::mojom::ServiceWorkerObjectInfoPtr active) override {
    set_version_attributes_called_count_++;
    changed_mask_ = std::move(changed_mask);
    installing_ = std::move(installing);
    waiting_ = std::move(waiting);
    active_ = std::move(active);
  }
  void SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache) override {
    set_update_via_cache_called_count_++;
    update_via_cache_ = update_via_cache;
  }
  void UpdateFound() override { update_found_called_count_++; }

  int update_found_called_count_ = 0;
  int set_version_attributes_called_count_ = 0;
  int set_update_via_cache_called_count_ = 0;
  blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask_;
  blink::mojom::ServiceWorkerObjectInfoPtr installing_;
  blink::mojom::ServiceWorkerObjectInfoPtr waiting_;
  blink::mojom::ServiceWorkerObjectInfoPtr active_;
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_ =
      blink::mojom::ServiceWorkerUpdateViaCache::kImports;

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerRegistrationObject>
      receiver_;
};

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = std::make_unique<EmbeddedWorkerTestHelper>(base::FilePath());
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
    storage_partition_impl_->OnBrowserContextWillBeDestroyed();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerRegistry* registry() { return helper_->context()->registry(); }

  class RegistrationListener : public ServiceWorkerRegistration::Listener {
   public:
    RegistrationListener() {}
    ~RegistrationListener() {
      if (observed_registration_.get()) {
        observed_registration_->RemoveListener(this);
      }
    }

    void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask)
        override {
      observed_registration_ = registration;
      observed_changed_mask_ = std::move(changed_mask);
    }

    void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) override {
      NOTREACHED_IN_MIGRATION();
    }

    void OnUpdateFound(ServiceWorkerRegistration* registration) override {
      NOTREACHED_IN_MIGRATION();
    }

    void Reset() {
      observed_registration_ = nullptr;
      observed_changed_mask_ = nullptr;
    }

    scoped_refptr<ServiceWorkerRegistration> observed_registration_;
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr observed_changed_mask_;
  };

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::unique_ptr<StoragePartitionImpl> storage_partition_impl_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScript("http://www.example.not/service_worker.js");
  int64_t kRegistrationId = 1L;

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      ServiceWorkerRegistration::Create(
          options, kKey, kRegistrationId, context()->AsWeakPtr(),
          blink::mojom::AncestorFrameType::kNormalFrame);

  const int64_t version_1_id = 1L;
  const int64_t version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScript, blink::mojom::ScriptType::kClassic,
          version_1_id,
          mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
          context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_2 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScript, blink::mojom::ScriptType::kClassic,
          version_2_id,
          mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
          context()->AsWeakPtr());

  RegistrationListener listener;
  registration->AddListener(&listener);
  registration->SetActiveVersion(version_1);

  EXPECT_EQ(version_1.get(), registration->active_version());
  EXPECT_EQ(registration, listener.observed_registration_);
  EXPECT_TRUE(listener.observed_changed_mask_->active);
  EXPECT_EQ(kScope, listener.observed_registration_->scope());
  EXPECT_EQ(version_1_id,
            listener.observed_registration_->active_version()->version_id());
  EXPECT_EQ(kScript,
            listener.observed_registration_->active_version()->script_url());
  EXPECT_FALSE(listener.observed_registration_->installing_version());
  EXPECT_FALSE(listener.observed_registration_->waiting_version());
  listener.Reset();

  registration->SetInstallingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_->installing);
  EXPECT_EQ(version_1_id,
            listener.observed_registration_->active_version()->version_id());
  EXPECT_EQ(
      version_2_id,
      listener.observed_registration_->installing_version()->version_id());
  EXPECT_FALSE(listener.observed_registration_->waiting_version());
  listener.Reset();

  registration->SetWaitingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_->waiting);
  EXPECT_TRUE(listener.observed_changed_mask_->installing);
  EXPECT_EQ(version_1_id,
            listener.observed_registration_->active_version()->version_id());
  EXPECT_EQ(version_2_id,
            listener.observed_registration_->waiting_version()->version_id());
  EXPECT_FALSE(listener.observed_registration_->installing_version());
  listener.Reset();

  registration->UnsetVersion(version_2.get());

  EXPECT_FALSE(registration->waiting_version());
  EXPECT_TRUE(listener.observed_changed_mask_->waiting);
  EXPECT_EQ(version_1_id,
            listener.observed_registration_->active_version()->version_id());
  EXPECT_FALSE(listener.observed_registration_->waiting_version());
  EXPECT_FALSE(listener.observed_registration_->installing_version());
}

TEST_F(ServiceWorkerRegistrationTest, FailedRegistrationNoCrash) {
  const GURL kScope("http://www.example.not/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  int64_t kRegistrationId = 1L;
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  auto registration = ServiceWorkerRegistration::Create(
      options, kKey, kRegistrationId, context()->AsWeakPtr(),
      blink::mojom::AncestorFrameType::kNormalFrame);
  // Prepare a ServiceWorkerContainerHost.
  auto service_worker_client = CommittedServiceWorkerClient(
      CreateServiceWorkerClient(context()),
      GlobalRenderFrameHostId(helper_->mock_render_process_id(),
                              /*mock frame_routing_id=*/1));
  auto registration_object_host =
      std::make_unique<ServiceWorkerRegistrationObjectHost>(
          context()->AsWeakPtr(), &service_worker_client.container_host(),
          registration);
  // To enable the caller end point
  // |registration_object_host->remote_registration_| to make calls safely with
  // no need to pass |object_info_->receiver| through a message pipe endpoint.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr object_info =
      registration_object_host->CreateObjectInfo();
  object_info->receiver.EnableUnassociatedUsage();

  registration->NotifyRegistrationFailed();
  // Don't crash when |registration_object_host| gets destructed.
}

TEST_F(ServiceWorkerRegistrationTest, NavigationPreload) {
  const GURL kScope("http://www.example.not/");
  const blink::StorageKey kKey =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
  const GURL kScript("https://www.example.not/service_worker.js");
  // Setup.

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateNewServiceWorkerRegistration(context()->registry(), options, kKey);
  scoped_refptr<ServiceWorkerVersion> version_1 = CreateNewServiceWorkerVersion(
      context()->registry(), registration.get(), kScript,
      blink::mojom::ScriptType::kClassic);
  version_1->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  registration->SetActiveVersion(version_1);
  version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  scoped_refptr<ServiceWorkerVersion> version_2 = CreateNewServiceWorkerVersion(
      context()->registry(), registration.get(), kScript,
      blink::mojom::ScriptType::kClassic);
  version_2->set_fetch_handler_type(
      ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
  registration->SetWaitingVersion(version_2);
  version_2->SetStatus(ServiceWorkerVersion::INSTALLED);

  // Navigation preload is disabled by default.
  EXPECT_FALSE(version_1->navigation_preload_state().enabled);
  // Enabling it sets the flag on the active version.
  registration->EnableNavigationPreload(true);
  EXPECT_TRUE(version_1->navigation_preload_state().enabled);
  // A new active version gets the flag.
  registration->SetActiveVersion(version_2);
  version_2->SetStatus(ServiceWorkerVersion::ACTIVATING);
  EXPECT_TRUE(version_2->navigation_preload_state().enabled);
  // Disabling it unsets the flag on the active version.
  registration->EnableNavigationPreload(false);
  EXPECT_FALSE(version_2->navigation_preload_state().enabled);
}

// Sets up a registration with a waiting worker, and an active worker
// with a controllee and an inflight request.
class ServiceWorkerActivationTest : public ServiceWorkerRegistrationTest,
                                    public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerActivationTest() : ServiceWorkerRegistrationTest() {}

  ServiceWorkerActivationTest(const ServiceWorkerActivationTest&) = delete;
  ServiceWorkerActivationTest& operator=(const ServiceWorkerActivationTest&) =
      delete;

  void SetUp() override {
    ServiceWorkerRegistrationTest::SetUp();

    const GURL kUrl("https://www.example.not/");
    const GURL kScope("https://www.example.not/");
    const blink::StorageKey kKey =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(kScope));
    const GURL kScript("https://www.example.not/service_worker.js");

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = kScope;
    registration_ = CreateNewServiceWorkerRegistration(context()->registry(),
                                                       options, kKey);

    // Create an active version.
    scoped_refptr<ServiceWorkerVersion> version_1 =
        CreateNewServiceWorkerVersion(context()->registry(),
                                      registration_.get(), kScript,
                                      blink::mojom::ScriptType::kClassic);
    version_1->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    registration_->SetActiveVersion(version_1);
    version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);

    // Store the registration.
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records_1;
    records_1.push_back(WriteToDiskCacheSync(
        helper_->context()->GetStorageControl(), version_1->script_url(),
        {} /* headers */, "I'm the body", "I'm the meta data"));
    version_1->script_cache_map()->SetResources(records_1);
    version_1->SetMainScriptResponse(
        EmbeddedWorkerTestHelper::CreateMainScriptResponse());
    std::optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    context()->registry()->StoreRegistration(
        registration_.get(), version_1.get(),
        ReceiveServiceWorkerStatus(&status, run_loop.QuitClosure()));
    run_loop.Run();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

    // Give the active version a controllee.
    service_worker_client_ = std::make_unique<ScopedServiceWorkerClient>(
        CreateServiceWorkerClient(context(), kUrl));
    (*service_worker_client_)
        ->SetControllerRegistration(registration_,
                                    false /* notify_controllerchange */);

    // Setup the Mojo implementation fakes for the renderer-side service worker.
    // These will be bound once the service worker starts.
    version_1_client_ =
        helper_
            ->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
                helper_.get())
            ->GetWeakPtr();
    version_1_service_worker_ =
        helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get())
            ->AsWeakPtr();

    // Start the active version and give it an in-flight request.
    inflight_request_id_ = CreateInflightRequest(version_1.get());

    // Create a waiting version.
    scoped_refptr<ServiceWorkerVersion> version_2 =
        CreateNewServiceWorkerVersion(context()->registry(),
                                      registration_.get(), kScript,
                                      blink::mojom::ScriptType::kClassic);
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records_2;
    records_2.push_back(WriteToDiskCacheSync(
        helper_->context()->GetStorageControl(), version_2->script_url(),
        {} /* headers */, "I'm the body", "I'm the meta data"));
    version_2->script_cache_map()->SetResources(records_2);
    version_2->SetMainScriptResponse(
        EmbeddedWorkerTestHelper::CreateMainScriptResponse());
    version_2->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    registration_->SetWaitingVersion(version_2);

    // Setup the Mojo implementation fakes for the renderer-side service worker.
    // These will be bound once the service worker starts.
    version_2_client_ =
        helper_->AddNewPendingInstanceClient<FakeEmbeddedWorkerInstanceClient>(
            helper_.get());
    version_2_service_worker_ =
        helper_->AddNewPendingServiceWorker<FakeServiceWorker>(helper_.get());

    // Start the worker.
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk,
              StartServiceWorker(version_2.get()));
    version_2->SetStatus(ServiceWorkerVersion::INSTALLED);

    // Set it to activate when ready. The original version should still be
    // active.
    registration_->ActivateWaitingVersionWhenReady();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(version_1.get(), registration_->active_version());

    if (devtools_should_be_attached()) {
      // Attach DevTools to the active worker. This shouldn't prevent from
      // promoting the waiting worker to active.
      version_1->SetDevToolsAttached(true);
    }
  }

  void TearDown() override { ServiceWorkerRegistrationTest::TearDown(); }

  bool devtools_should_be_attached() const { return GetParam(); }

  ServiceWorkerRegistration* registration() { return registration_.get(); }
  int inflight_request_id() const { return inflight_request_id_; }

  void AddControllee() {
    (*service_worker_client_)
        ->SetControllerRegistration(registration(),
                                    false /* notify_controllerchange */);
  }

  void RemoveControllee() {
    (*service_worker_client_)
        ->SetControllerRegistration(nullptr,
                                    false /* notify_controllerchange */);
  }

  bool IsLameDuckTimerRunning() {
    return registration_->lame_duck_timer_.IsRunning();
  }

  void RunLameDuckTimer() { registration_->RemoveLameDuckIfNeeded(); }

  // Simulates skipWaiting(). Note that skipWaiting() might not try to activate
  // the worker "immediately", if it can't yet be activated yet. If activation
  // is delayed, |out_result| will not be set. If activation is attempted,
  // |out_result| is generally true but false in case of a fatal/unexpected
  // error like ServiceWorkerContext shutdown.
  void SimulateSkipWaiting(ServiceWorkerVersion* version,
                           std::optional<bool>* out_result) {
    SimulateSkipWaitingWithCallback(version, out_result, base::DoNothing());
  }

  void SimulateSkipWaitingWithCallback(ServiceWorkerVersion* version,
                                       std::optional<bool>* out_result,
                                       base::OnceClosure done_callback) {
    version->SkipWaiting(base::BindOnce(
        [](base::OnceClosure done_callback, std::optional<bool>* out_result,
           bool success) {
          *out_result = success;
          std::move(done_callback).Run();
        },
        std::move(done_callback), out_result));
    base::RunLoop().RunUntilIdle();
  }

  base::WeakPtr<FakeEmbeddedWorkerInstanceClient> version_1_client() {
    return version_1_client_;
  }
  FakeEmbeddedWorkerInstanceClient* version_2_client() {
    return version_2_client_;
  }
  base::WeakPtr<FakeServiceWorker> version_1_service_worker() {
    return version_1_service_worker_;
  }
  FakeServiceWorker* version_2_service_worker() {
    return version_2_service_worker_;
  }

 private:
  scoped_refptr<ServiceWorkerRegistration> registration_;

  // Mojo implementation fakes for the renderer-side service workers. Their
  // lifetime is bound to the Mojo connection.
  base::WeakPtr<FakeEmbeddedWorkerInstanceClient> version_1_client_;
  base::WeakPtr<FakeServiceWorker> version_1_service_worker_;
  raw_ptr<FakeEmbeddedWorkerInstanceClient> version_2_client_ = nullptr;
  raw_ptr<FakeServiceWorker> version_2_service_worker_ = nullptr;

  std::unique_ptr<ScopedServiceWorkerClient> service_worker_client_;
  int inflight_request_id_ = -1;
};

// Test activation triggered by finishing all requests.
TEST_P(ServiceWorkerActivationTest, NoInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  reg->SetTaskRunnerForTest(runner);

  // Remove the controllee. Since there is an in-flight request,
  // activation should not yet happen.
  RemoveControllee();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  // The idle timer living in the renderer is requested to notify the idle state
  // to the browser ASAP.
  ASSERT_TRUE(version_1_service_worker());
  EXPECT_EQ(base::Seconds(0), version_1_service_worker()->idle_delay().value());

  // Finish the request. Activation should happen.
  version_1->FinishRequest(inflight_request_id(), /*was_handled=*/true);
  EXPECT_EQ(version_1.get(), reg->active_version());
  ASSERT_TRUE(version_1_client());
  RequestTermination(&version_1_client()->host());

  TestServiceWorkerObserver observer(helper_->context_wrapper());
  observer.RunUntilActivated(version_2.get(), runner);
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting and finishing requests.
TEST_P(ServiceWorkerActivationTest, SkipWaitingWithInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  std::optional<bool> result;
  base::RunLoop skip_waiting_loop;
  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen.
  SimulateSkipWaitingWithCallback(version_2.get(), &result,
                                  skip_waiting_loop.QuitClosure());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(version_1.get(), reg->active_version());
  ASSERT_TRUE(version_1_service_worker());
  EXPECT_EQ(base::Seconds(0), version_1_service_worker()->idle_delay().value());

  // Finish the request. FinishRequest() doesn't immediately make the worker
  // reach the "no work" state. It needs to be notfied of the idle state by
  // RequestTermination().
  version_1->FinishRequest(inflight_request_id(), /*was_handled=*/true);

  EXPECT_EQ(version_1.get(), reg->active_version());
  ASSERT_TRUE(version_1_client());
  RequestTermination(&version_1_client()->host());

  // Wait until SkipWaiting resolves.
  skip_waiting_loop.Run();

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting.
TEST_P(ServiceWorkerActivationTest, SkipWaiting) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Finish the in-flight request. Since there is a controllee,
  // activation should not happen.
  version_1->FinishRequest(inflight_request_id(), /*was_handled=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Call skipWaiting. Activation happens after RequestTermination is triggered.
  std::optional<bool> result;
  base::RunLoop skip_waiting_loop;
  SimulateSkipWaitingWithCallback(version_2.get(), &result,
                                  skip_waiting_loop.QuitClosure());

  ASSERT_TRUE(version_1_service_worker());
  EXPECT_EQ(base::Seconds(0), version_1_service_worker()->idle_delay().value());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(version_1.get(), reg->active_version());
  ASSERT_TRUE(version_1_client());
  RequestTermination(&version_1_client()->host());

  // Wait until SkipWaiting resolves.
  skip_waiting_loop.Run();

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  EXPECT_EQ(version_2.get(), reg->active_version());
}

TEST_P(ServiceWorkerActivationTest, TimeSinceSkipWaiting_Installing) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version = reg->waiting_version();
  base::SimpleTestTickClock clock;
  clock.SetNowTicks(base::TimeTicks::Now());
  version->SetTickClockForTesting(&clock);

  // Reset version to the installing phase.
  reg->UnsetVersion(version.get());
  version->SetStatus(ServiceWorkerVersion::INSTALLING);

  std::optional<bool> result;
  // Call skipWaiting(). The time ticks since skip waiting shouldn't start
  // since the version is not yet installed.
  SimulateSkipWaiting(version.get(), &result);
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  clock.Advance(base::Seconds(11));
  EXPECT_EQ(base::TimeDelta(), version->TimeSinceSkipWaiting());

  // Install the version. Now the skip waiting time starts ticking.
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  reg->SetWaitingVersion(version);
  base::RunLoop().RunUntilIdle();
  clock.Advance(base::Seconds(33));
  EXPECT_EQ(base::Seconds(33), version->TimeSinceSkipWaiting());

  result.reset();
  // Call skipWaiting() again. It doesn't reset the time.
  SimulateSkipWaiting(version.get(), &result);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(base::Seconds(33), version->TimeSinceSkipWaiting());

  // Restore the TickClock to the default. This is required because the
  // TickClock must outlive ServiceWorkerVersion, otherwise ServiceWorkerVersion
  // will hold a dangling pointer.
  version->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
}

// Test lame duck timer triggered by skip waiting.
TEST_P(ServiceWorkerActivationTest, LameDuckTime_SkipWaiting) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();
  base::SimpleTestTickClock clock_1;
  base::SimpleTestTickClock clock_2;
  clock_1.SetNowTicks(base::TimeTicks::Now());
  clock_2.SetNowTicks(clock_1.NowTicks());
  version_1->SetTickClockForTesting(&clock_1);
  version_2->SetTickClockForTesting(&clock_2);

  std::optional<bool> result;
  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen. But the lame duck timer should start.
  EXPECT_FALSE(IsLameDuckTimerRunning());
  SimulateSkipWaiting(version_2.get(), &result);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move forward by lame duck time.
  clock_2.Advance(kMaxLameDuckTime + base::Seconds(1));

  // Activation should happen by the lame duck timer.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  EXPECT_EQ(version_2.get(), reg->active_version());
  EXPECT_FALSE(IsLameDuckTimerRunning());

  // Restore the TickClock to the default. This is required because the
  // TickClock must outlive ServiceWorkerVersion, otherwise ServiceWorkerVersion
  // will hold a dangling pointer.
  version_1->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
  version_2->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
}

// Test lame duck timer triggered by loss of controllee.
TEST_P(ServiceWorkerActivationTest, LameDuckTime_NoControllee) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();
  base::SimpleTestTickClock clock_1;
  base::SimpleTestTickClock clock_2;
  clock_1.SetNowTicks(base::TimeTicks::Now());
  clock_2.SetNowTicks(clock_1.NowTicks());
  version_1->SetTickClockForTesting(&clock_1);
  version_2->SetTickClockForTesting(&clock_2);

  // Remove the controllee. Since there is still an in-flight request,
  // activation should not happen. But the lame duck timer should start.
  EXPECT_FALSE(IsLameDuckTimerRunning());
  RemoveControllee();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move clock forward by a little bit.
  constexpr base::TimeDelta kLittleBit = base::Minutes(1);
  clock_1.Advance(kLittleBit);

  // Add a controllee again to reset the lame duck period.
  AddControllee();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Remove the controllee.
  RemoveControllee();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move clock forward to the next lame duck timer tick.
  clock_1.Advance(kMaxLameDuckTime - kLittleBit + base::Seconds(1));

  // Run the lame duck timer. Activation should not yet happen
  // since the lame duck period has not expired.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Continue on to the next lame duck timer tick.
  clock_1.Advance(kMaxLameDuckTime + base::Seconds(1));

  // Activation should happen by the lame duck timer.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
  EXPECT_FALSE(IsLameDuckTimerRunning());

  // Restore the TickClock to the default. This is required because the
  // TickClock must outlive ServiceWorkerVersion, otherwise ServiceWorkerVersion
  // will hold a dangling pointer.
  version_1->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
  version_2->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
}

INSTANTIATE_TEST_SUITE_P(ServiceWorkerActivationTestWithDevTools,
                         ServiceWorkerActivationTest,
                         testing::Bool());

// Sets up a registration with a ServiceWorkerRegistrationObjectHost to hold it.
class ServiceWorkerRegistrationObjectHostTest
    : public ServiceWorkerRegistrationTest {
 protected:
  void SetUp() override {
    ServiceWorkerRegistrationTest::SetUp();
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &ServiceWorkerRegistrationObjectHostTest::OnMojoError,
        base::Unretained(this)));
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
    ServiceWorkerRegistrationTest::TearDown();
  }

  // Pass nullptr for |out_error_msg| if it's not needed.
  blink::mojom::ServiceWorkerErrorType CallUpdate(
      blink::mojom::ServiceWorkerRegistrationObjectHost* registration_host,
      std::string* out_error_msg) {
    blink::mojom::ServiceWorkerErrorType out_error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    registration_host->Update(
        blink::mojom::FetchClientSettingsObject::New(),
        base::BindLambdaForTesting(
            [&out_error, &out_error_msg](
                blink::mojom::ServiceWorkerErrorType error,
                const std::optional<std::string>& error_msg) {
              out_error = error;
              if (out_error_msg) {
                *out_error_msg = error_msg ? *error_msg : "";
              }
            }));
    base::RunLoop().RunUntilIdle();
    return out_error;
  }

  blink::mojom::ServiceWorkerErrorType CallDelayUpdate(
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion& version) {
    std::optional<blink::mojom::ServiceWorkerErrorType> error;
    base::RunLoop run_loop;
    registration->DelayUpdate(
        version, blink::mojom::FetchClientSettingsObject::New(),
        base::BindOnce(
            [](std::optional<blink::mojom::ServiceWorkerErrorType>* out_error,
               base::OnceClosure callback,
               blink::mojom::ServiceWorkerErrorType error,
               const std::optional<std::string>& error_msg) {
              *out_error = error;
              std::move(callback).Run();
            },
            &error, run_loop.QuitClosure()));
    run_loop.Run();
    return error.value();
  }

  blink::mojom::ServiceWorkerErrorType CallUnregister(
      blink::mojom::ServiceWorkerRegistrationObjectHost* registration_host) {
    blink::mojom::ServiceWorkerErrorType error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    registration_host->Unregister(base::BindOnce(
        [](blink::mojom::ServiceWorkerErrorType* out_error,
           blink::mojom::ServiceWorkerErrorType error,
           const std::optional<std::string>& error_msg) { *out_error = error; },
        &error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  blink::ServiceWorkerStatusCode FindRegistrationInStorage(
      int64_t registration_id,
      const blink::StorageKey& key) {
    std::optional<blink::ServiceWorkerStatusCode> status;
    registry()->FindRegistrationForId(
        registration_id, key,
        base::BindOnce(
            [](std::optional<blink::ServiceWorkerStatusCode>* out_status,
               blink::ServiceWorkerStatusCode status,
               scoped_refptr<ServiceWorkerRegistration> registration) {
              *out_status = status;
            },
            &status));
    base::RunLoop().RunUntilIdle();
    return status.value();
  }

  scoped_refptr<ServiceWorkerRegistration> CreateNewRegistration(
      const GURL& scope) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    const blink::StorageKey key =
        blink::StorageKey::CreateFirstParty(url::Origin::Create(scope));
    return CreateNewServiceWorkerRegistration(context()->registry(), options,
                                              key);
  }

  scoped_refptr<ServiceWorkerVersion> CreateVersion(
      ServiceWorkerRegistration* registration,
      const GURL& script_url) {
    scoped_refptr<ServiceWorkerVersion> version = CreateNewServiceWorkerVersion(
        context()->registry(), registration, script_url,
        blink::mojom::ScriptType::kClassic);
    std::vector<storage::mojom::ServiceWorkerResourceRecordPtr> records;
    records.push_back(WriteToDiskCacheSync(
        helper_->context()->GetStorageControl(), version->script_url(),
        {} /* headers */, "I'm the body", "I'm the meta data"));
    version->script_cache_map()->SetResources(records);
    version->SetMainScriptResponse(
        EmbeddedWorkerTestHelper::CreateMainScriptResponse());
    version->set_fetch_handler_type(
        ServiceWorkerVersion::FetchHandlerType::kNotSkippable);
    version->SetStatus(ServiceWorkerVersion::INSTALLING);
    return version;
  }

  int64_t SetUpRegistration(const GURL& scope, const GURL& script_url) {
    // Prepare ServiceWorkerRegistration and ServiceWorkerVersion.
    scoped_refptr<ServiceWorkerRegistration> registration =
        CreateNewRegistration(scope);
    scoped_refptr<ServiceWorkerVersion> version =
        CreateVersion(registration.get(), script_url);

    // Make the registration findable via storage functions.
    bool called = false;
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    registry()->StoreRegistration(
        registration.get(), version.get(),
        base::BindOnce(&SaveStatusCallback, &called, &status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);

    return registration->id();
  }

  CommittedServiceWorkerClient PrepareServiceWorkerContainerHost(
      const GURL& document_url) {
    return CommittedServiceWorkerClient(
        CreateServiceWorkerClient(context(), document_url),
        GlobalRenderFrameHostId(helper_->mock_render_process_id(),
                                /*mock frame_routing_id=*/1));
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  GetRegistrationFromRemote(
      blink::mojom::ServiceWorkerContainerHost* container_host,
      const GURL& url) {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info;
    container_host->GetRegistration(
        url, base::BindOnce(
                 [](blink::mojom::ServiceWorkerRegistrationObjectInfoPtr*
                        out_registration_info,
                    blink::mojom::ServiceWorkerErrorType error,
                    const std::optional<std::string>& error_msg,
                    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                        registration) {
                   ASSERT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
                             error);
                   *out_registration_info = std::move(registration);
                 },
                 &registration_info));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registration_info->host_remote.is_valid());
    return registration_info;
  }

  void OnMojoError(const std::string& error) { bad_messages_.push_back(error); }

  std::vector<std::string> bad_messages_;
};

class ServiceWorkerRegistrationObjectHostUpdateTest
    : public ServiceWorkerRegistrationObjectHostTest,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerRegistrationObjectHostUpdateTest()
      : interceptor_(base::BindRepeating(&FakeNetwork::HandleRequest,
                                         base::Unretained(&fake_network_))) {}

 private:
  FakeNetwork fake_network_;
  URLLoaderInterceptor interceptor_;
};

INSTANTIATE_TEST_SUITE_P(ServiceWorkerRegistrationObjectHostUpdateTestP,
                         ServiceWorkerRegistrationObjectHostUpdateTest,
                         testing::Bool());

TEST_F(ServiceWorkerRegistrationObjectHostTest, BreakConnection_Destroy) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  registration_host.Bind(std::move(info->host_remote));

  EXPECT_NE(nullptr, context()->GetLiveRegistration(registration_id));
  registration_host.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, context()->GetLiveRegistration(registration_id));
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest, Update_Success) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  registration_host.Bind(std::move(info->host_remote));
  // Ignore the messages to the registration object, otherwise the callbacks
  // issued from |registration_host| may wait for receiving the messages to
  // |info->receiver|.
  info->receiver.reset();

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            CallUpdate(registration_host.get(), /*out_error_msg=*/nullptr));
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest,
       Update_CrossOriginShouldFail) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  registration_host.Bind(std::move(info->host_remote));

  ASSERT_TRUE(bad_messages_.empty());
  GURL url("https://does.not.exist/");
  service_worker_client->UpdateUrlsAfterCommitResponseForTesting(
      url, url::Origin::Create(url),
      blink::StorageKey::CreateFirstParty(url::Origin::Create(url)));
  CallUpdate(registration_host.get(), /*out_error_msg=*/nullptr);
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest,
       Update_ContentSettingsDisallowsServiceWorker) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  registration_host.Bind(std::move(info->host_remote));

  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);
  std::string error_msg;
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            CallUpdate(registration_host.get(), &error_msg));
  EXPECT_EQ(
      error_msg,
      "Failed to update a ServiceWorker for scope ('https://www.example.com/') "
      "with script ('https://www.example.com/sw.js'): The user denied "
      "permission to use Service Worker.");
  SetBrowserClientForTesting(old_browser_client);
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest,
       Update_NoDelayFromControllee) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  registration_host.Bind(std::move(info->host_remote));
  // Ignore the messages to the registration object, otherwise the callbacks
  // issued from |registration_host| may wait for receiving the messages to
  // |info->receiver|.
  info->receiver.reset();

  // Get registration and set |self_update_delay| to zero.
  scoped_refptr<ServiceWorkerRegistration> registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_TRUE(registration);
  registration->set_self_update_delay(base::TimeDelta());
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            CallUpdate(registration_host.get(), /*out_error_msg=*/nullptr));
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest,
       Update_DelayFromWorkerWithoutControllee) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateNewRegistration(kScope);
  scoped_refptr<ServiceWorkerVersion> version =
      CreateVersion(registration.get(), kScriptUrl);
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);

  // Initially set |self_update_delay| to zero.
  registration->set_self_update_delay(base::TimeDelta());
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNotFound,
            CallDelayUpdate(registration.get(), *version));
  EXPECT_LT(base::TimeDelta(), registration->self_update_delay());

  // TODO(falken): Add a test verifying that a delayed update will be executed
  // eventually.

  // Set |self_update_delay| to a time so that update() will reject immediately.
  registration->set_self_update_delay(base::Minutes(5));
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kTimeout,
            CallDelayUpdate(registration.get(), *version));
  EXPECT_LE(base::Minutes(5), registration->self_update_delay());
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest,
       Update_NoDelayFromWorkerWithControllee) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateNewRegistration(kScope);

  scoped_refptr<ServiceWorkerVersion> version =
      CreateVersion(registration.get(), kScriptUrl);
  version->SetStatus(ServiceWorkerVersion::ACTIVATED);

  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  version->AddControllee(service_worker_client.get());

  // Initially set |self_update_delay| to zero.
  registration->set_self_update_delay(base::TimeDelta());
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNotFound,
            CallDelayUpdate(registration.get(), *version));
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());

  // Set |self_update_delay| to a time so that update() will reject immediately
  // if the worker doesn't have at least one controlee.
  registration->set_self_update_delay(base::Minutes(5));
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNotFound,
            CallDelayUpdate(registration.get(), *version));
  EXPECT_EQ(base::Minutes(5), registration->self_update_delay());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, Unregister_Success) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  registration_host.Bind(std::move(info->host_remote));
  // Ignore the messages to the registration object and corresponding service
  // worker objects, otherwise the callbacks issued from |registration_host|
  // may wait for receiving the messages to them.
  info->receiver.reset();
  info->waiting->receiver.reset();

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationInStorage(registration_id,
                                      blink::StorageKey::CreateFirstParty(
                                          url::Origin::Create(kScope))));
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            CallUnregister(registration_host.get()));

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationInStorage(registration_id,
                                      blink::StorageKey::CreateFirstParty(
                                          url::Origin::Create(kScope))));
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNotFound,
            CallUnregister(registration_host.get()));
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Unregister_CrossOriginShouldFail) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  registration_host.Bind(std::move(info->host_remote));

  ASSERT_TRUE(bad_messages_.empty());
  service_worker_client->UpdateUrlsAfterCommitResponseForTesting(
      GURL("https://does.not.exist/"),
      url::Origin::Create(GURL("https://does.not.exist/")),
      blink::StorageKey::CreateFromStringForTesting("https://does.not.exist/"));
  CallUnregister(registration_host.get());
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Unregister_ContentSettingsDisallowsServiceWorker) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerRegistrationObjectHost>
      registration_host;
  registration_host.Bind(std::move(info->host_remote));

  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            CallUnregister(registration_host.get()));
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, SetVersionAttributes) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  EXPECT_EQ(registration_id, info->registration_id);
  EXPECT_TRUE(info->receiver.is_valid());
  auto mock_registration_object =
      std::make_unique<MockServiceWorkerRegistrationObject>(
          std::move(info->receiver));

  scoped_refptr<ServiceWorkerRegistration> registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_NE(nullptr, registration);
  const int64_t version_1_id = 1L;
  const int64_t version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScriptUrl, blink::mojom::ScriptType::kClassic,
          version_1_id,
          mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
          context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_2 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScriptUrl, blink::mojom::ScriptType::kClassic,
          version_2_id,
          mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>(),
          context()->AsWeakPtr());

  // Set an active worker.
  registration->SetActiveVersion(version_1);
  EXPECT_EQ(version_1.get(), registration->active_version());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_registration_object->set_version_attributes_called_count());
  EXPECT_FALSE(mock_registration_object->changed_mask().installing);
  EXPECT_FALSE(mock_registration_object->installing());
  EXPECT_FALSE(mock_registration_object->changed_mask().waiting);
  EXPECT_FALSE(mock_registration_object->waiting());
  EXPECT_TRUE(mock_registration_object->changed_mask().active);
  EXPECT_TRUE(mock_registration_object->active());
  EXPECT_EQ(version_1_id, mock_registration_object->active()->version_id);
  EXPECT_EQ(kScriptUrl, mock_registration_object->active()->url);

  // Set an installing worker.
  registration->SetInstallingVersion(version_2);
  EXPECT_EQ(version_2.get(), registration->installing_version());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, mock_registration_object->set_version_attributes_called_count());
  EXPECT_TRUE(mock_registration_object->changed_mask().installing);
  EXPECT_TRUE(mock_registration_object->installing());
  EXPECT_FALSE(mock_registration_object->changed_mask().waiting);
  EXPECT_FALSE(mock_registration_object->waiting());
  EXPECT_FALSE(mock_registration_object->changed_mask().active);
  EXPECT_FALSE(mock_registration_object->active());
  EXPECT_EQ(version_2_id, mock_registration_object->installing()->version_id);
  EXPECT_EQ(kScriptUrl, mock_registration_object->installing()->url);

  // Promote the installing worker to waiting.
  registration->SetWaitingVersion(version_2);
  EXPECT_EQ(version_2.get(), registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, mock_registration_object->set_version_attributes_called_count());
  EXPECT_TRUE(mock_registration_object->changed_mask().installing);
  EXPECT_FALSE(mock_registration_object->installing());
  EXPECT_TRUE(mock_registration_object->changed_mask().waiting);
  EXPECT_TRUE(mock_registration_object->waiting());
  EXPECT_FALSE(mock_registration_object->changed_mask().active);
  EXPECT_FALSE(mock_registration_object->active());
  EXPECT_EQ(version_2_id, mock_registration_object->waiting()->version_id);
  EXPECT_EQ(kScriptUrl, mock_registration_object->waiting()->url);

  // Remove the waiting worker.
  registration->UnsetVersion(version_2.get());
  EXPECT_FALSE(registration->waiting_version());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, mock_registration_object->set_version_attributes_called_count());
  EXPECT_FALSE(mock_registration_object->changed_mask().installing);
  EXPECT_FALSE(mock_registration_object->installing());
  EXPECT_TRUE(mock_registration_object->changed_mask().waiting);
  EXPECT_FALSE(mock_registration_object->waiting());
  EXPECT_FALSE(mock_registration_object->changed_mask().active);
  EXPECT_FALSE(mock_registration_object->active());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, SetUpdateViaCache) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  EXPECT_EQ(registration_id, info->registration_id);
  EXPECT_TRUE(info->receiver.is_valid());
  auto mock_registration_object =
      std::make_unique<MockServiceWorkerRegistrationObject>(
          std::move(info->receiver));

  scoped_refptr<ServiceWorkerRegistration> registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kImports,
            registration->update_via_cache());
  ASSERT_EQ(0, mock_registration_object->set_update_via_cache_called_count());
  ASSERT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kImports,
            mock_registration_object->update_via_cache());

  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, mock_registration_object->set_update_via_cache_called_count());
  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kImports,
            mock_registration_object->update_via_cache());

  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kAll);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_registration_object->set_update_via_cache_called_count());
  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kAll,
            mock_registration_object->update_via_cache());

  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, mock_registration_object->set_update_via_cache_called_count());
  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kNone,
            mock_registration_object->update_via_cache());

  registration->SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, mock_registration_object->set_update_via_cache_called_count());
  EXPECT_EQ(blink::mojom::ServiceWorkerUpdateViaCache::kImports,
            mock_registration_object->update_via_cache());
}

TEST_P(ServiceWorkerRegistrationObjectHostUpdateTest, UpdateFound) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  CommittedServiceWorkerClient service_worker_client =
      PrepareServiceWorkerContainerHost(kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(service_worker_client.host_remote().get(),
                                kScope);
  EXPECT_EQ(registration_id, info->registration_id);
  EXPECT_TRUE(info->receiver.is_valid());
  auto mock_registration_object =
      std::make_unique<MockServiceWorkerRegistrationObject>(
          std::move(info->receiver));

  scoped_refptr<ServiceWorkerRegistration> registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_NE(nullptr, registration);
  EXPECT_EQ(0, mock_registration_object->update_found_called_count());
  registration->NotifyUpdateFound();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_registration_object->update_found_called_count());
}

}  // namespace content

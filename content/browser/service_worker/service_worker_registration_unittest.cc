// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include <stdint.h>
#include <utility>

#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration_object_host.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {
namespace service_worker_registration_unittest {

// From service_worker_registration.cc.
constexpr base::TimeDelta kMaxLameDuckTime = base::TimeDelta::FromMinutes(5);

int CreateInflightRequest(ServiceWorkerVersion* version) {
  version->StartWorker(ServiceWorkerMetrics::EventType::PUSH,
                       base::DoNothing());
  base::RunLoop().RunUntilIdle();
  return version->StartRequest(ServiceWorkerMetrics::EventType::PUSH,
                               base::DoNothing());
}

static void SaveStatusCallback(bool* called,
                               blink::ServiceWorkerStatusCode* out,
                               blink::ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  bool AllowServiceWorker(
      const GURL& scope,
      const GURL& first_party,
      content::ResourceContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter) override {
    return false;
  }
};

class MockServiceWorkerRegistrationObject
    : public blink::mojom::ServiceWorkerRegistrationObject {
 public:
  explicit MockServiceWorkerRegistrationObject(
      blink::mojom::ServiceWorkerRegistrationObjectAssociatedRequest request)
      : binding_(this) {
    binding_.Bind(std::move(request));
  }
  ~MockServiceWorkerRegistrationObject() override = default;

  int update_found_called_count() const { return update_found_called_count_; };
  int set_version_attributes_called_count() const {
    return set_version_attributes_called_count_;
  };
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

  mojo::AssociatedBinding<blink::mojom::ServiceWorkerRegistrationObject>
      binding_;
};

// We need this for NoInflightRequest test. The test expects that a worker
// will be terminated when SetIdleTimerDelayToZero() is called.
class RegistrationTestHelper : public EmbeddedWorkerTestHelper {
 public:
  RegistrationTestHelper()
      : EmbeddedWorkerTestHelper(base::FilePath()), weak_factory_(this) {}
  ~RegistrationTestHelper() override = default;

  void RequestTermination(int embedded_worker_id) {
    GetEmbeddedWorkerInstanceHost(embedded_worker_id)
        ->RequestTermination(
            base::BindOnce(&RegistrationTestHelper::OnRequestedTermination,
                           weak_factory_.GetWeakPtr()));
  }

  const base::Optional<bool>& will_be_terminated() const {
    return will_be_terminated_;
  }

  bool is_zero_idle_timer_delay() const { return is_zero_idle_timer_delay_; }

 protected:
  void OnSetIdleTimerDelayToZero(int embedded_worker_id) override {
    is_zero_idle_timer_delay_ = true;
  }

  void OnRequestedTermination(bool will_be_terminated) {
    will_be_terminated_ = will_be_terminated;
  }

 private:
  bool is_zero_idle_timer_delay_ = false;
  base::Optional<bool> will_be_terminated_;
  base::WeakPtrFactory<RegistrationTestHelper> weak_factory_;
};

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_ = std::make_unique<RegistrationTestHelper>();

    context()->storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

  class RegistrationListener : public ServiceWorkerRegistration::Listener {
   public:
    RegistrationListener() {}
    ~RegistrationListener() {
      if (observed_registration_.get())
        observed_registration_->RemoveListener(this);
    }

    void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
        const ServiceWorkerRegistrationInfo& info) override {
      observed_registration_ = registration;
      observed_changed_mask_ = std::move(changed_mask);
      observed_info_ = info;
    }

    void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void OnUpdateFound(ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void Reset() {
      observed_registration_ = nullptr;
      observed_changed_mask_ = nullptr;
      observed_info_ = ServiceWorkerRegistrationInfo();
    }

    scoped_refptr<ServiceWorkerRegistration> observed_registration_;
    blink::mojom::ChangedServiceWorkerObjectsMaskPtr observed_changed_mask_;
    ServiceWorkerRegistrationInfo observed_info_;
  };

 protected:
  std::unique_ptr<RegistrationTestHelper> helper_;
  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64_t kRegistrationId = 1L;

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      base::MakeRefCounted<ServiceWorkerRegistration>(options, kRegistrationId,
                                                      context()->AsWeakPtr());

  const int64_t version_1_id = 1L;
  const int64_t version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScript, blink::mojom::ScriptType::kClassic,
          version_1_id, context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_2 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScript, blink::mojom::ScriptType::kClassic,
          version_2_id, context()->AsWeakPtr());

  RegistrationListener listener;
  registration->AddListener(&listener);
  registration->SetActiveVersion(version_1);

  EXPECT_EQ(version_1.get(), registration->active_version());
  EXPECT_EQ(registration, listener.observed_registration_);
  EXPECT_TRUE(listener.observed_changed_mask_->active);
  EXPECT_EQ(kScope, listener.observed_info_.scope);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(kScript, listener.observed_info_.active_version.script_url);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetInstallingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_->installing);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id,
            listener.observed_info_.installing_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetWaitingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_->waiting);
  EXPECT_TRUE(listener.observed_changed_mask_->installing);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id, listener.observed_info_.waiting_version.version_id);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->UnsetVersion(version_2.get());

  EXPECT_FALSE(registration->waiting_version());
  EXPECT_TRUE(listener.observed_changed_mask_->waiting);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            blink::mojom::kInvalidServiceWorkerVersionId);
}

TEST_F(ServiceWorkerRegistrationTest, FailedRegistrationNoCrash) {
  const GURL kScope("http://www.example.not/");
  int64_t kRegistrationId = 1L;
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  auto registration = base::MakeRefCounted<ServiceWorkerRegistration>(
      options, kRegistrationId, context()->AsWeakPtr());
  // Prepare a ServiceWorkerProviderHost.
  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerProviderHost> provider_host =
      CreateProviderHostForWindow(helper_->mock_render_process_id(),
                                  1 /* dummy provider_id */,
                                  true /* is_parent_frame_secure */,
                                  context()->AsWeakPtr(), &remote_endpoint);
  auto registration_object_host =
      std::make_unique<ServiceWorkerRegistrationObjectHost>(
          context()->AsWeakPtr(), provider_host.get(), registration);
  // To enable the caller end point
  // |registration_object_host->remote_registration_| to make calls safely with
  // no need to pass |object_info_->request| through a message pipe endpoint.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr object_info =
      registration_object_host->CreateObjectInfo();
  mojo::AssociateWithDisconnectedPipe(object_info->request.PassHandle());

  registration->NotifyRegistrationFailed();
  // Don't crash when |registration_object_host| gets destructed.
}

TEST_F(ServiceWorkerRegistrationTest, NavigationPreload) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("https://www.example.not/service_worker.js");
  // Setup.

  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = kScope;
  scoped_refptr<ServiceWorkerRegistration> registration =
      base::MakeRefCounted<ServiceWorkerRegistration>(
          options, storage()->NewRegistrationId(), context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_1 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScript, blink::mojom::ScriptType::kClassic,
          storage()->NewVersionId(), context()->AsWeakPtr());
  version_1->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  registration->SetActiveVersion(version_1);
  version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  scoped_refptr<ServiceWorkerVersion> version_2 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration.get(), kScript, blink::mojom::ScriptType::kClassic,
          storage()->NewVersionId(), context()->AsWeakPtr());
  version_2->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
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

  void SetUp() override {
    ServiceWorkerRegistrationTest::SetUp();

    const GURL kScope("https://www.example.not/");
    const GURL kScript("https://www.example.not/service_worker.js");

    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = kScope;
    registration_ = base::MakeRefCounted<ServiceWorkerRegistration>(
        options, storage()->NewRegistrationId(), context()->AsWeakPtr());

    // Create an active version.
    scoped_refptr<ServiceWorkerVersion> version_1 =
        base::MakeRefCounted<ServiceWorkerVersion>(
            registration_.get(), kScript, blink::mojom::ScriptType::kClassic,
            storage()->NewVersionId(), context()->AsWeakPtr());
    version_1->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    registration_->SetActiveVersion(version_1);
    version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);

    // Store the registration.
    std::vector<ServiceWorkerDatabase::ResourceRecord> records_1;
    records_1.push_back(WriteToDiskCacheSync(
        helper_->context()->storage(), version_1->script_url(),
        helper_->context()->storage()->NewResourceId(), {} /* headers */,
        "I'm the body", "I'm the meta data"));
    version_1->script_cache_map()->SetResources(records_1);
    version_1->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    base::Optional<blink::ServiceWorkerStatusCode> status;
    context()->storage()->StoreRegistration(
        registration_.get(), version_1.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status.value());

    // Give the active version a controllee.
    host_ = CreateProviderHostForWindow(
        helper_->mock_render_process_id(), 1 /* dummy provider_id */,
        true /* is_parent_frame_secure */, context()->AsWeakPtr(),
        &remote_endpoint_);
    DCHECK(remote_endpoint_.client_request()->is_pending());
    DCHECK(remote_endpoint_.host_ptr()->is_bound());
    version_1->AddControllee(host_.get());

    // Give the active version an in-flight request.
    inflight_request_id_ = CreateInflightRequest(version_1.get());

    // Create a waiting version.
    scoped_refptr<ServiceWorkerVersion> version_2 =
        base::MakeRefCounted<ServiceWorkerVersion>(
            registration_.get(), kScript, blink::mojom::ScriptType::kClassic,
            storage()->NewVersionId(), context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records_2;
    records_2.push_back(WriteToDiskCacheSync(
        helper_->context()->storage(), version_2->script_url(),
        helper_->context()->storage()->NewResourceId(), {} /* headers */,
        "I'm the body", "I'm the meta data"));
    version_2->script_cache_map()->SetResources(records_2);
    version_2->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    version_2->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    registration_->SetWaitingVersion(version_2);
    version_2->StartWorker(ServiceWorkerMetrics::EventType::INSTALL,
                           base::DoNothing());
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

  void TearDown() override {
    registration_->active_version()->RemoveObserver(registration_.get());
    ServiceWorkerRegistrationTest::TearDown();
  }

  bool devtools_should_be_attached() const { return GetParam(); }

  ServiceWorkerRegistration* registration() { return registration_.get(); }
  ServiceWorkerProviderHost* controllee() { return host_.get(); }
  int inflight_request_id() const { return inflight_request_id_; }

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
                           base::Optional<bool>* out_result) {
    SimulateSkipWaitingWithCallback(version, out_result, base::DoNothing());
  }

  void SimulateSkipWaitingWithCallback(ServiceWorkerVersion* version,
                                       base::Optional<bool>* out_result,
                                       base::OnceClosure done_callback) {
    version->SkipWaiting(base::BindOnce(
        [](base::OnceClosure done_callback, base::Optional<bool>* out_result,
           bool success) {
          *out_result = success;
          std::move(done_callback).Run();
        },
        std::move(done_callback), out_result));
    base::RunLoop().RunUntilIdle();
  }

 private:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  std::unique_ptr<ServiceWorkerProviderHost> host_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;
  int inflight_request_id_ = -1;
};

// Test activation triggered by finishing all requests.
TEST_P(ServiceWorkerActivationTest, NoInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Remove the controllee. Since there is an in-flight request,
  // activation should not yet happen.
  // When S13nServiceWorker is on, the idle timer living in the renderer is
  // requested to notify the browser the idle state ASAP.
  version_1->RemoveControllee(controllee()->client_uuid());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    EXPECT_TRUE(helper_->is_zero_idle_timer_delay());

  // Finish the request. Activation should happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::TimeTicks::Now());
  base::RunLoop().RunUntilIdle();

  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    EXPECT_EQ(version_1.get(), reg->active_version());
    helper_->RequestTermination(
        version_1->embedded_worker()->embedded_worker_id());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(helper_->will_be_terminated().value());
  }

  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by loss of controllee.
TEST_P(ServiceWorkerActivationTest, NoControllee) {
  // S13nServiceWorker: activation only happens when the service worker reports
  // it's idle, so this test doesn't make sense.
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    return;
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Finish the request. Since there is a controllee, activation should not yet
  // happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::TimeTicks::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Remove the controllee. Activation should happen.
  version_1->RemoveControllee(controllee()->client_uuid());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting and finishing requests.
TEST_P(ServiceWorkerActivationTest, SkipWaitingWithInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  base::Optional<bool> result;
  base::RunLoop skip_waiting_loop;
  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen.
  SimulateSkipWaitingWithCallback(version_2.get(), &result,
                                  skip_waiting_loop.QuitClosure());
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(version_1.get(), reg->active_version());
  if (blink::ServiceWorkerUtils::IsServicificationEnabled())
    EXPECT_TRUE(helper_->is_zero_idle_timer_delay());

  // Finish the request.
  // non-S13nServiceWorker: The service worker becomes idle.
  // S13nServiceWorker: FinishRequest() doesn't immediately make the worker
  // "no work" state. It needs to be notfied the idle state by
  // RequestTermination().
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::TimeTicks::Now());

  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    EXPECT_EQ(version_1.get(), reg->active_version());
    helper_->RequestTermination(
        version_1->embedded_worker()->embedded_worker_id());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(helper_->will_be_terminated().value());
  }

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
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::TimeTicks::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Call skipWaiting.
  // non-S13nServiceWorker: Activation should happen.
  // S13nServiceWorker: Activation should happen after RequestTermination is
  // triggered.
  base::Optional<bool> result;
  base::RunLoop skip_waiting_loop;
  SimulateSkipWaitingWithCallback(version_2.get(), &result,
                                  skip_waiting_loop.QuitClosure());

  if (blink::ServiceWorkerUtils::IsServicificationEnabled()) {
    EXPECT_TRUE(helper_->is_zero_idle_timer_delay());
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(version_1.get(), reg->active_version());
    helper_->RequestTermination(
        version_1->embedded_worker()->embedded_worker_id());
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(helper_->will_be_terminated().value());
  }

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

  base::Optional<bool> result;
  // Call skipWaiting(). The time ticks since skip waiting shouldn't start
  // since the version is not yet installed.
  SimulateSkipWaiting(version.get(), &result);
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  clock.Advance(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta(), version->TimeSinceSkipWaiting());

  // Install the version. Now the skip waiting time starts ticking.
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  reg->SetWaitingVersion(version);
  base::RunLoop().RunUntilIdle();
  clock.Advance(base::TimeDelta::FromSeconds(33));
  EXPECT_EQ(base::TimeDelta::FromSeconds(33), version->TimeSinceSkipWaiting());

  result.reset();
  // Call skipWaiting() again. It doesn't reset the time.
  SimulateSkipWaiting(version.get(), &result);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(base::TimeDelta::FromSeconds(33), version->TimeSinceSkipWaiting());
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

  base::Optional<bool> result;
  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen. But the lame duck timer should start.
  EXPECT_FALSE(IsLameDuckTimerRunning());
  SimulateSkipWaiting(version_2.get(), &result);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move forward by lame duck time.
  clock_2.Advance(kMaxLameDuckTime + base::TimeDelta::FromSeconds(1));

  // Activation should happen by the lame duck timer.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(*result);
  EXPECT_EQ(version_2.get(), reg->active_version());
  EXPECT_FALSE(IsLameDuckTimerRunning());
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
  version_1->RemoveControllee(controllee()->client_uuid());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move clock forward by a little bit.
  constexpr base::TimeDelta kLittleBit = base::TimeDelta::FromMinutes(1);
  clock_1.Advance(kLittleBit);

  // Add a controllee again to reset the lame duck period.
  version_1->AddControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Remove the controllee.
  version_1->RemoveControllee(controllee()->client_uuid());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move clock forward to the next lame duck timer tick.
  clock_1.Advance(kMaxLameDuckTime - kLittleBit +
                  base::TimeDelta::FromSeconds(1));

  // Run the lame duck timer. Activation should not yet happen
  // since the lame duck period has not expired.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Continue on to the next lame duck timer tick.
  clock_1.Advance(kMaxLameDuckTime + base::TimeDelta::FromSeconds(1));

  // Activation should happen by the lame duck timer.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
  EXPECT_FALSE(IsLameDuckTimerRunning());
}

INSTANTIATE_TEST_CASE_P(ServiceWorkerActivationTestWithDevTools,
                        ServiceWorkerActivationTest,
                        testing::Bool());

// Sets up a registration with a ServiceWorkerRegistrationObjectHost to hold it.
class ServiceWorkerRegistrationObjectHostTest
    : public ServiceWorkerRegistrationTest {
 protected:
  void SetUp() override {
    ServiceWorkerRegistrationTest::SetUp();
    mojo::core::SetDefaultProcessErrorCallback(base::AdaptCallbackForRepeating(
        base::BindOnce(&ServiceWorkerRegistrationObjectHostTest::OnMojoError,
                       base::Unretained(this))));
  }

  void TearDown() override {
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
    ServiceWorkerRegistrationTest::TearDown();
  }

  blink::mojom::ServiceWorkerErrorType CallUpdate(
      blink::mojom::ServiceWorkerRegistrationObjectHost* registration_host) {
    blink::mojom::ServiceWorkerErrorType error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    registration_host->Update(base::BindOnce(
        [](blink::mojom::ServiceWorkerErrorType* out_error,
           blink::mojom::ServiceWorkerErrorType error,
           const base::Optional<std::string>& error_msg) {
          *out_error = error;
        },
        &error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  blink::ServiceWorkerStatusCode CallDelayUpdate(
      blink::mojom::ServiceWorkerProviderType provider_type,
      ServiceWorkerRegistration* registration,
      ServiceWorkerVersion* version) {
    base::Optional<blink::ServiceWorkerStatusCode> status;
    base::RunLoop run_loop;
    ServiceWorkerRegistrationObjectHost::DelayUpdate(
        blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
        registration, version,
        base::BindOnce(
            [](base::Optional<blink::ServiceWorkerStatusCode>* out_status,
               base::OnceClosure callback,
               blink::ServiceWorkerStatusCode status) {
              *out_status = status;
              std::move(callback).Run();
            },
            &status, run_loop.QuitClosure()));
    run_loop.Run();
    return status.value();
  }

  blink::mojom::ServiceWorkerErrorType CallUnregister(
      blink::mojom::ServiceWorkerRegistrationObjectHost* registration_host) {
    blink::mojom::ServiceWorkerErrorType error =
        blink::mojom::ServiceWorkerErrorType::kUnknown;
    registration_host->Unregister(base::BindOnce(
        [](blink::mojom::ServiceWorkerErrorType* out_error,
           blink::mojom::ServiceWorkerErrorType error,
           const base::Optional<std::string>& error_msg) {
          *out_error = error;
        },
        &error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

  blink::ServiceWorkerStatusCode FindRegistrationInStorage(
      int64_t registration_id,
      const GURL& scope) {
    base::Optional<blink::ServiceWorkerStatusCode> status;
    storage()->FindRegistrationForId(
        registration_id, scope,
        base::AdaptCallbackForRepeating(base::BindOnce(
            [](base::Optional<blink::ServiceWorkerStatusCode>* out_status,
               blink::ServiceWorkerStatusCode status,
               scoped_refptr<ServiceWorkerRegistration> registration) {
              *out_status = status;
            },
            &status)));
    base::RunLoop().RunUntilIdle();
    return status.value();
  }

  scoped_refptr<ServiceWorkerRegistration> CreateRegistration(
      const GURL& scope) {
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope;
    return base::MakeRefCounted<ServiceWorkerRegistration>(
        options, storage()->NewRegistrationId(), context()->AsWeakPtr());
  }

  scoped_refptr<ServiceWorkerVersion> CreateVersion(
      ServiceWorkerRegistration* registration,
      const GURL& script_url) {
    scoped_refptr<ServiceWorkerVersion> version =
        base::MakeRefCounted<ServiceWorkerVersion>(
            registration, script_url, blink::mojom::ScriptType::kClassic,
            storage()->NewVersionId(), context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(WriteToDiskCacheSync(
        storage(), version->script_url(), storage()->NewResourceId(),
        {} /* headers */, "I'm the body", "I'm the meta data"));
    version->script_cache_map()->SetResources(records);
    version->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    version->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version->SetStatus(ServiceWorkerVersion::INSTALLING);
    return version;
  }

  int64_t SetUpRegistration(const GURL& scope, const GURL& script_url) {
    storage()->LazyInitializeForTest(base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // Prepare ServiceWorkerRegistration and ServiceWorkerVersion.
    scoped_refptr<ServiceWorkerRegistration> registration =
        CreateRegistration(scope);
    scoped_refptr<ServiceWorkerVersion> version =
        CreateVersion(registration.get(), script_url);

    // Make the registration findable via storage functions.
    bool called = false;
    blink::ServiceWorkerStatusCode status =
        blink::ServiceWorkerStatusCode::kErrorFailed;
    storage()->StoreRegistration(
        registration.get(), version.get(),
        base::BindOnce(&SaveStatusCallback, &called, &status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);

    return registration->id();
  }

  ServiceWorkerRemoteProviderEndpoint PrepareProviderHost(
      int64_t provider_id,
      const GURL& document_url) {
    ServiceWorkerRemoteProviderEndpoint remote_endpoint;
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(helper_->mock_render_process_id(),
                                    provider_id,
                                    true /* is_parent_frame_secure */,
                                    context()->AsWeakPtr(), &remote_endpoint);
    host->SetDocumentUrl(document_url);
    context()->AddProviderHost(std::move(host));
    return remote_endpoint;
  }

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  GetRegistrationFromRemote(mojom::ServiceWorkerContainerHost* container_host,
                            const GURL& url) {
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info;
    container_host->GetRegistration(
        url, base::BindOnce(
                 [](blink::mojom::ServiceWorkerRegistrationObjectInfoPtr*
                        out_registration_info,
                    blink::mojom::ServiceWorkerErrorType error,
                    const base::Optional<std::string>& error_msg,
                    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
                        registration) {
                   ASSERT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
                             error);
                   *out_registration_info = std::move(registration);
                 },
                 &registration_info));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(registration_info->host_ptr_info.is_valid());
    return registration_info;
  }

  void OnMojoError(const std::string& error) { bad_messages_.push_back(error); }

  std::vector<std::string> bad_messages_;
};

TEST_F(ServiceWorkerRegistrationObjectHostTest, BreakConnection_Destroy) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;
  registration_host_ptr.Bind(std::move(info->host_ptr_info));

  EXPECT_NE(nullptr, context()->GetLiveRegistration(registration_id));
  registration_host_ptr.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, context()->GetLiveRegistration(registration_id));
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, Update_Success) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  registration_host_ptr.Bind(std::move(info->host_ptr_info));
  // Ignore the messages to the registration object, otherwise the callbacks
  // issued from |registration_host_ptr| may wait for receiving the messages to
  // |info->request|.
  info->request = nullptr;

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            CallUpdate(registration_host_ptr.get()));
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, Update_CrossOriginShouldFail) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;
  registration_host_ptr.Bind(std::move(info->host_ptr_info));

  ASSERT_TRUE(bad_messages_.empty());
  context()
      ->GetProviderHost(helper_->mock_render_process_id(), kProviderId)
      ->SetDocumentUrl(GURL("https://does.not.exist/"));
  CallUpdate(registration_host_ptr.get());
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Update_ContentSettingsDisallowsServiceWorker) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;
  registration_host_ptr.Bind(std::move(info->host_ptr_info));

  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            CallUpdate(registration_host_ptr.get()));
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, Update_NoDelayFromControllee) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;

  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  registration_host_ptr.Bind(std::move(info->host_ptr_info));
  // Ignore the messages to the registration object, otherwise the callbacks
  // issued from |registration_host_ptr| may wait for receiving the messages to
  // |info->request|.
  info->request = nullptr;

  // Get registration and set |self_update_delay| to zero.
  ServiceWorkerRegistration* registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_TRUE(registration);
  registration->set_self_update_delay(base::TimeDelta());
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());

  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            CallUpdate(registration_host_ptr.get()));
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Update_DelayFromWorkerWithoutControllee) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateRegistration(kScope);
  scoped_refptr<ServiceWorkerVersion> version =
      CreateVersion(registration.get(), kScriptUrl);

  // Initially set |self_update_delay| to zero.
  registration->set_self_update_delay(base::TimeDelta());
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            CallDelayUpdate(
                blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
                registration.get(), version.get()));
  EXPECT_LT(base::TimeDelta(), registration->self_update_delay());

  // TODO(falken): Add a test verifying that a delayed update will be executed
  // eventually.

  // Set |self_update_delay| to a time so that update() will reject immediately.
  registration->set_self_update_delay(base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorTimeout,
            CallDelayUpdate(
                blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
                registration.get(), version.get()));
  EXPECT_LE(base::TimeDelta::FromMinutes(5), registration->self_update_delay());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Update_NoDelayFromWorkerWithControllee) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  const int64_t kProviderId = 99;  // Dummy value
  scoped_refptr<ServiceWorkerRegistration> registration =
      CreateRegistration(kScope);
  scoped_refptr<ServiceWorkerVersion> version =
      CreateVersion(registration.get(), kScriptUrl);
  ServiceWorkerRemoteProviderEndpoint remote_endpoint;
  std::unique_ptr<ServiceWorkerProviderHost> host = CreateProviderHostForWindow(
      helper_->mock_render_process_id(), kProviderId,
      true /* is_parent_frame_secure */, context()->AsWeakPtr(),
      &remote_endpoint);
  host->SetDocumentUrl(kScope);
  version->AddControllee(host.get());

  // Initially set |self_update_delay| to zero.
  registration->set_self_update_delay(base::TimeDelta());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            CallDelayUpdate(
                blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
                registration.get(), version.get()));
  EXPECT_EQ(base::TimeDelta(), registration->self_update_delay());

  // Set |self_update_delay| to a time so that update() will reject immediately
  // if the worker doesn't have at least one controlee.
  registration->set_self_update_delay(base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            CallDelayUpdate(
                blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
                registration.get(), version.get()));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registration->self_update_delay());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, Unregister_Success) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  registration_host_ptr.Bind(std::move(info->host_ptr_info));
  // Ignore the messages to the registration object and corresponding service
  // worker objects, otherwise the callbacks issued from |registration_host_ptr|
  // may wait for receiving the messages to them.
  info->request = nullptr;
  info->waiting->request = nullptr;

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk,
            FindRegistrationInStorage(registration_id, kScope));
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNone,
            CallUnregister(registration_host_ptr.get()));

  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorNotFound,
            FindRegistrationInStorage(registration_id, kScope));
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kNotFound,
            CallUnregister(registration_host_ptr.get()));
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Unregister_CrossOriginShouldFail) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;
  registration_host_ptr.Bind(std::move(info->host_ptr_info));

  ASSERT_TRUE(bad_messages_.empty());
  context()
      ->GetProviderHost(helper_->mock_render_process_id(), kProviderId)
      ->SetDocumentUrl(GURL("https://does.not.exist/"));
  CallUnregister(registration_host_ptr.get());
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(ServiceWorkerRegistrationObjectHostTest,
       Unregister_ContentSettingsDisallowsServiceWorker) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  blink::mojom::ServiceWorkerRegistrationObjectHostAssociatedPtr
      registration_host_ptr;
  registration_host_ptr.Bind(std::move(info->host_ptr_info));

  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);
  EXPECT_EQ(blink::mojom::ServiceWorkerErrorType::kDisabled,
            CallUnregister(registration_host_ptr.get()));
  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerRegistrationObjectHostTest, SetVersionAttributes) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  EXPECT_EQ(registration_id, info->registration_id);
  EXPECT_TRUE(info->request.is_pending());
  auto mock_registration_object =
      std::make_unique<MockServiceWorkerRegistrationObject>(
          std::move(info->request));

  ServiceWorkerRegistration* registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_NE(nullptr, registration);
  const int64_t version_1_id = 1L;
  const int64_t version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration, kScriptUrl, blink::mojom::ScriptType::kClassic,
          version_1_id, context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_2 =
      base::MakeRefCounted<ServiceWorkerVersion>(
          registration, kScriptUrl, blink::mojom::ScriptType::kClassic,
          version_2_id, context()->AsWeakPtr());

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
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  EXPECT_EQ(registration_id, info->registration_id);
  EXPECT_TRUE(info->request.is_pending());
  auto mock_registration_object =
      std::make_unique<MockServiceWorkerRegistrationObject>(
          std::move(info->request));

  ServiceWorkerRegistration* registration =
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

TEST_F(ServiceWorkerRegistrationObjectHostTest, UpdateFound) {
  const GURL kScope("https://www.example.com/");
  const GURL kScriptUrl("https://www.example.com/sw.js");
  int64_t registration_id = SetUpRegistration(kScope, kScriptUrl);
  const int64_t kProviderId = 99;  // Dummy value
  ServiceWorkerRemoteProviderEndpoint remote_endpoint =
      PrepareProviderHost(kProviderId, kScope);
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info =
      GetRegistrationFromRemote(remote_endpoint.host_ptr()->get(), kScope);
  EXPECT_EQ(registration_id, info->registration_id);
  EXPECT_TRUE(info->request.is_pending());
  auto mock_registration_object =
      std::make_unique<MockServiceWorkerRegistrationObject>(
          std::move(info->request));

  ServiceWorkerRegistration* registration =
      context()->GetLiveRegistration(registration_id);
  ASSERT_NE(nullptr, registration);
  EXPECT_EQ(0, mock_registration_object->update_found_called_count());
  registration->NotifyUpdateFound();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, mock_registration_object->update_found_called_count());
}

}  // namespace service_worker_registration_unittest
}  // namespace content

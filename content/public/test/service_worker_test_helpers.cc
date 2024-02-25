// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/service_worker_test_helpers.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "third_party/blink/public/common/notifications/platform_notification_data.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom-forward.h"
#include "url/gurl.h"

// Allow `ServiceWorkerVersionCreatedWatcher` to scoped observe the custom
// observer add/remove methods on `ServiceWorkerContextCore`.
namespace base {
template <>
struct ScopedObservationTraits<
    content::ServiceWorkerContextCore,
    content::ServiceWorkerContextCore::TestVersionObserver> {
  static void AddObserver(
      content::ServiceWorkerContextCore* source,
      content::ServiceWorkerContextCore::TestVersionObserver* observer) {
    source->AddVersionObserverForTest(observer);
  }
  static void RemoveObserver(
      content::ServiceWorkerContextCore* source,
      content::ServiceWorkerContextCore::TestVersionObserver* observer) {
    source->RemoveVersionObserverForTest(observer);
  }
};
}  // namespace base

namespace content {

namespace {

class StoppedObserver : public base::RefCountedThreadSafe<StoppedObserver> {
 public:
  StoppedObserver(const StoppedObserver&) = delete;
  StoppedObserver& operator=(const StoppedObserver&) = delete;

  static void StartObserving(ServiceWorkerContextWrapper* context,
                             int64_t service_worker_version_id,
                             base::OnceClosure completion_callback_ui) {
    auto observer = base::WrapRefCounted(
        new StoppedObserver(std::move(completion_callback_ui)));
    // Adds a ref to StoppedObserver to keep |this| around until the worker is
    // stopped.
    observer->inner_observer_ = std::make_unique<Observer>(
        context, service_worker_version_id,
        base::BindOnce(&StoppedObserver::OnStopped, observer));
  }

 private:
  friend class base::RefCountedThreadSafe<StoppedObserver>;

  explicit StoppedObserver(base::OnceClosure completion_callback_ui)
      : completion_callback_ui_(std::move(completion_callback_ui)) {}
  ~StoppedObserver() {}

  class Observer : public ServiceWorkerContextCoreObserver {
   public:
    Observer(ServiceWorkerContextWrapper* context,
             int64_t service_worker_version_id,
             base::OnceClosure stopped_callback)
        : context_(context),
          version_id_(service_worker_version_id),
          stopped_callback_(std::move(stopped_callback)) {
      context_->AddObserver(this);
    }

    // ServiceWorkerContextCoreObserver:
    void OnStopped(int64_t version_id) override {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      if (version_id != version_id_)
        return;
      std::move(stopped_callback_).Run();
    }
    ~Observer() override { context_->RemoveObserver(this); }

   private:
    const raw_ptr<ServiceWorkerContextWrapper> context_;
    int64_t version_id_;
    base::OnceClosure stopped_callback_;
  };

  void OnStopped() {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&StoppedObserver::OnStopped, this));
      return;
    }
    std::move(completion_callback_ui_).Run();
  }

  std::unique_ptr<Observer> inner_observer_;
  base::OnceClosure completion_callback_ui_;
};

void StopServiceWorkerForRegistration(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    base::OnceClosure completion_callback_ui,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, service_worker_status);
  int64_t version_id =
      service_worker_registration->active_version()->version_id();
  StoppedObserver::StartObserving(context_wrapper.get(), version_id,
                                  std::move(completion_callback_ui));
  service_worker_registration->active_version()->embedded_worker()->Stop();
}

void DispatchNotificationClickForRegistration(
    const blink::PlatformNotificationData& notification_data,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, service_worker_status);
  scoped_refptr<ServiceWorkerVersion> version =
      service_worker_registration->active_version();
  version->StartRequest(ServiceWorkerMetrics::EventType::NOTIFICATION_CLICK,
                        base::DoNothing());
  version->endpoint()->DispatchNotificationClickEvent(
      "notification_id", notification_data, -1 /* action_index */,
      std::nullopt /* reply */,
      base::BindOnce([](blink::mojom::ServiceWorkerEventStatus event_status) {
        DCHECK_EQ(blink::mojom::ServiceWorkerEventStatus::COMPLETED,
                  event_status);
      }));
}

}  // namespace

// Implementation for `content::ServiceWorkerContextCore::TestVersionObserver`.
// Observes new versions created and sets a state observer new versions that it
// observes being created.
class ServiceWorkerTestHelper::ServiceWorkerVersionCreatedWatcher
    : public content::ServiceWorkerContextCore::TestVersionObserver {
 public:
  ServiceWorkerVersionCreatedWatcher(
      content::ServiceWorkerContextCore* context_core,
      ServiceWorkerTestHelper* parent)
      : parent_(parent) {
    scoped_observation_.Observe(context_core);
  }

 private:
  // content::ServiceWorkerContextCore::TestObserver
  void OnServiceWorkerVersionCreated(ServiceWorkerVersion* version) override {
    // Create a `ServiceWorkerVersionStateManager` for this version.
    parent_->OnServiceWorkerVersionCreated(version);
  }

  raw_ptr<ServiceWorkerTestHelper> const parent_;
  base::ScopedObservation<
      content::ServiceWorkerContextCore,
      content::ServiceWorkerContextCore::TestVersionObserver>
      scoped_observation_{this};
};

// Observes state changes of the `ServiceWorkerVersion` it is observing.
class ServiceWorkerTestHelper::ServiceWorkerVersionStateManager
    : public ServiceWorkerVersion::Observer {
 public:
  ServiceWorkerVersionStateManager(ServiceWorkerTestHelper* parent,
                                   ServiceWorkerVersion* version)
      : parent_(parent), sw_version_(version) {
    scoped_observation_.Observe(sw_version_);
  }

  ~ServiceWorkerVersionStateManager() override {
    // Release potential dangling pointers.
    sw_version_ = nullptr;
    parent_ = nullptr;
  }

 private:
  // ServiceWorkerVersion::Observer
  void OnRunningStateChanged(ServiceWorkerVersion* version) override {
    parent_->OnDidRunningStatusChange(version->running_status(),
                                      version->version_id());
  }

  raw_ptr<ServiceWorkerTestHelper> parent_;
  raw_ptr<ServiceWorkerVersion> sw_version_;
  base::ScopedObservation<ServiceWorkerVersion, ServiceWorkerVersion::Observer>
      scoped_observation_{this};
};

ServiceWorkerTestHelper::ServiceWorkerTestHelper(ServiceWorkerContext* context,
                                                 int64_t worker_version_id) {
  DCHECK(context);
  if (worker_version_id != blink::mojom::kInvalidServiceWorkerVersionId) {
    RegisterStateObserver(context, worker_version_id);
  } else {
    RegisterVersionCreatedObserver(context);
  }
}

ServiceWorkerTestHelper::~ServiceWorkerTestHelper() {
  version_created_watcher_.reset();
  for (auto& version_state_manager : version_state_managers_) {
    version_state_manager.reset();
  }
  version_state_managers_.clear();
}

void ServiceWorkerTestHelper::RegisterVersionCreatedObserver(
    ServiceWorkerContext* context) {
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper(
      static_cast<ServiceWorkerContextWrapper*>(context));
  version_created_watcher_ =
      std::make_unique<ServiceWorkerVersionCreatedWatcher>(
          context_wrapper->GetContextCoreForTest(), this);
}

void ServiceWorkerTestHelper::RegisterStateObserver(
    ServiceWorkerContext* context,
    int64_t worker_version_id) {
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper(
      static_cast<ServiceWorkerContextWrapper*>(context));
  version_state_managers_.push_back(
      std::make_unique<ServiceWorkerVersionStateManager>(
          this, context_wrapper->GetLiveVersion(worker_version_id)));
}

void ServiceWorkerTestHelper::OnServiceWorkerVersionCreated(
    ServiceWorkerVersion* version) {
  version_state_managers_.push_back(
      std::make_unique<ServiceWorkerVersionStateManager>(this, version));
}

void StopServiceWorkerForScope(ServiceWorkerContext* context,
                               const GURL& scope,
                               base::OnceClosure completion_callback_ui) {
  DCHECK(context);
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper(
      static_cast<ServiceWorkerContextWrapper*>(context));

  context_wrapper->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&StopServiceWorkerForRegistration, context_wrapper,
                     std::move(completion_callback_ui)));
}

void DispatchServiceWorkerNotificationClick(
    ServiceWorkerContext* context,
    const GURL& scope,
    const blink::PlatformNotificationData& notification_data) {
  DCHECK(context);
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper(
      static_cast<ServiceWorkerContextWrapper*>(context));

  context_wrapper->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&DispatchNotificationClickForRegistration,
                     notification_data));
}

void AdvanceClockAfterRequestTimeout(ServiceWorkerContext* context,
                                     int64_t service_worker_version_id,
                                     base::SimpleTestTickClock* tick_clock) {
  tick_clock->SetNowTicks(base::TimeTicks::Now());

  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  service_worker_version->SetTickClockForTesting(tick_clock);

  base::TimeDelta timeout_beyond_request_timeout =
      // Timeout for a request to be handled.
      ServiceWorkerVersion::kRequestTimeout +
      // A little past that.
      base::Minutes(1);
  tick_clock->Advance(timeout_beyond_request_timeout);
}

void ResetTickClockToDefaultForAllLiveServiceWorkerVersions(
    ServiceWorkerContext* context) {
  content::ServiceWorkerContextWrapper* context_wrapper =
      static_cast<content::ServiceWorkerContextWrapper*>(context);
  for (const auto& version_info : context_wrapper->GetAllLiveVersionInfo()) {
    content::ServiceWorkerVersion* version =
        context_wrapper->GetLiveVersion(version_info.version_id);
    DCHECK(version);
    version->SetTickClockForTesting(base::DefaultTickClock::GetInstance());
  }
}

bool TriggerTimeoutAndCheckRunningState(ServiceWorkerContext* context,
                                        int64_t service_worker_version_id) {
  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  service_worker_version->RunUserTasksForTesting();

  // TODO(b/266799118): Investigate the need to call OnRequestTermination()
  service_worker_version->OnRequestTermination();
  return service_worker_version->running_status() ==
         blink::EmbeddedWorkerStatus::kRunning;
}

bool CheckServiceWorkerIsRunning(ServiceWorkerContext* context,
                                 int64_t service_worker_version_id) {
  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  return service_worker_version && service_worker_version->running_status() ==
                                       blink::EmbeddedWorkerStatus::kRunning;
}

bool CheckServiceWorkerIsStarting(ServiceWorkerContext* context,
                                  int64_t service_worker_version_id) {
  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  return service_worker_version && service_worker_version->running_status() ==
                                       blink::EmbeddedWorkerStatus::kStarting;
}

bool CheckServiceWorkerIsStopping(ServiceWorkerContext* context,
                                  int64_t service_worker_version_id) {
  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  return service_worker_version && service_worker_version->running_status() ==
                                       blink::EmbeddedWorkerStatus::kStopping;
}

bool CheckServiceWorkerIsStopped(ServiceWorkerContext* context,
                                 int64_t service_worker_version_id) {
  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  return !service_worker_version || service_worker_version->running_status() ==
                                        blink::EmbeddedWorkerStatus::kStopped;
}

void SetServiceWorkerIdleDelay(ServiceWorkerContext* context,
                               int64_t service_worker_version_id,
                               base::TimeDelta delta) {
  ServiceWorkerVersion* service_worker_version =
      static_cast<ServiceWorkerContextWrapper*>(context)->GetLiveVersion(
          service_worker_version_id);
  service_worker_version->endpoint()->SetIdleDelay(delta);
}

}  // namespace content

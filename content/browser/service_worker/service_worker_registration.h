// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerVersion;
struct ServiceWorkerRegistrationInfo;

// Represents the core of a service worker registration object. Other
// registration derivatives (WebServiceWorkerRegistration etc) ultimately refer
// to this class. This is refcounted via ServiceWorkerRegistrationObjectHost to
// facilitate multiple controllees being associated with the same registration.
class CONTENT_EXPORT ServiceWorkerRegistration
    : public base::RefCounted<ServiceWorkerRegistration>,
      public ServiceWorkerVersion::Observer {
 public:
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;

  class CONTENT_EXPORT Listener {
   public:
    virtual void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
        const ServiceWorkerRegistrationInfo& info) {}
    virtual void OnUpdateViaCacheChanged(
        ServiceWorkerRegistration* registation) {}
    virtual void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) {}
    virtual void OnRegistrationFinishedUninstalling(
        ServiceWorkerRegistration* registration) {}
    virtual void OnRegistrationDeleted(
        ServiceWorkerRegistration* registration) {}
    virtual void OnUpdateFound(
        ServiceWorkerRegistration* registration) {}
    virtual void OnSkippedWaiting(ServiceWorkerRegistration* registation) {}
  };

  enum class Status {
    // This registration has not been deleted.
    kIntact,
    // The registration data has been deleted from the database, but it's still
    // usable: if a page has an existing controller from this registration, the
    // controller will continue to function until all such pages are unloaded.
    // The registration may also be resurrected if register() is called while in
    // this state.
    kUninstalling,
    // This registration is completely uninstalled. It cannot be resurrected or
    // used.
    kUninstalled,
  };

  ServiceWorkerRegistration(
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      int64_t registration_id,
      base::WeakPtr<ServiceWorkerContextCore> context);

  int64_t id() const { return registration_id_; }
  const GURL& scope() const { return scope_; }
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache() const {
    return update_via_cache_;
  }

  bool is_deleted() const { return status_ != Status::kIntact; }
  bool is_uninstalling() const { return status_ == Status::kUninstalling; }
  bool is_uninstalled() const { return status_ == Status::kUninstalled; }
  Status status() const { return status_; }
  void SetStatus(Status status);

  int64_t resources_total_size_bytes() const {
    return resources_total_size_bytes_;
  }

  void set_resources_total_size_bytes(int64_t resources_total_size_bytes) {
    resources_total_size_bytes_ = resources_total_size_bytes;
  }

  // Returns the active version. This version may be in ACTIVATING or ACTIVATED
  // state. If you require an ACTIVATED version, use
  // ServiceWorkerContextWrapper::FindReadyRegistration* to get a registration
  // with such a version. Alternatively, use
  // ServiceWorkerVersion::Observer::OnVersionStateChanged to wait for the
  // ACTIVATING version to become ACTIVATED.
  ServiceWorkerVersion* active_version() const {
    return active_version_.get();
  }

  ServiceWorkerVersion* waiting_version() const {
    return waiting_version_.get();
  }

  ServiceWorkerVersion* installing_version() const {
    return installing_version_.get();
  }

  ServiceWorkerVersion* newest_installed_version() const {
    return waiting_version() ? waiting_version() : active_version();
  }

  const blink::mojom::NavigationPreloadState navigation_preload_state() const {
    return navigation_preload_state_;
  }

  ServiceWorkerVersion* GetNewestVersion() const;

  virtual void AddListener(Listener* listener);
  virtual void RemoveListener(Listener* listener);
  void NotifyRegistrationFailed();
  void NotifyUpdateFound();
  void NotifyVersionAttributesChanged(
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr mask);

  ServiceWorkerRegistrationInfo GetInfo();

  // Sets the corresposding version attribute and resets the position
  // (if any) left vacant (ie. by a waiting version being promoted).
  // Also notifies listeners via OnVersionAttributesChanged.
  void SetActiveVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void SetWaitingVersion(const scoped_refptr<ServiceWorkerVersion>& version);
  void SetInstallingVersion(const scoped_refptr<ServiceWorkerVersion>& version);

  // If version is the installing, waiting, active version of this
  // registation, the method will reset that field to NULL, and notify
  // listeners via OnVersionAttributesChanged.
  void UnsetVersion(ServiceWorkerVersion* version);

  // Sets the updateViaCache attribute, and notifies listeners via
  // OnUpdateViaCacheChanged.
  void SetUpdateViaCache(
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache);

  // Triggers the [[Activate]] algorithm when the currently active version is
  // ready to become redundant (see IsReadyToActivate()). The algorithm is
  // triggered immediately if it's already ready.
  void ActivateWaitingVersionWhenReady();

  // Takes over control of provider hosts which are currently not controlled or
  // controlled by other registrations.
  void ClaimClients();

  // Triggers the [[ClearRegistration]] algorithm when the currently
  // active version has no controllees. Deletes this registration
  // from storage immediately.
  void ClearWhenReady();

  // Restores this registration in storage and cancels the pending
  // [[ClearRegistration]] algorithm.
  void AbortPendingClear(StatusCallback callback);

  // The time of the most recent update check.
  base::Time last_update_check() const { return last_update_check_; }
  void set_last_update_check(base::Time last) { last_update_check_ = last; }

  // The delay for self-updating service workers, to prevent them from running
  // forever (see https://crbug.com/805496).
  base::TimeDelta self_update_delay() const { return self_update_delay_; }
  void set_self_update_delay(const base::TimeDelta& delay) {
    self_update_delay_ = delay;
  }

  // An emergency measure to forcibly delete the registration even if it has
  // controllees. The controllees will stop using the registration. Called when
  // a service worker can't be read from disk, a potentially sticky failure
  // that would prevent the site from being loaded.
  void ForceDelete();

  void RegisterRegistrationFinishedCallback(base::OnceClosure callback);
  void NotifyRegistrationFinished();

  void SetTaskRunnerForTest(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void EnableNavigationPreload(bool enable);
  void SetNavigationPreloadHeader(const std::string& value);

 protected:
  ~ServiceWorkerRegistration() override;

 private:
  friend class base::RefCounted<ServiceWorkerRegistration>;
  friend class ServiceWorkerActivationTest;

  void UnsetVersionInternal(
      ServiceWorkerVersion* version,
      blink::mojom::ChangedServiceWorkerObjectsMask* mask);

  // ServiceWorkerVersion::Observer override.
  void OnNoControllees(ServiceWorkerVersion* version) override;
  void OnNoWork(ServiceWorkerVersion* version) override;

  bool IsReadyToActivate() const;
  bool IsLameDuckActiveVersion() const;
  void StartLameDuckTimer();
  void RemoveLameDuckIfNeeded();

  // Promotes the waiting version to active version. If |delay| is true, waits
  // a short time before attempting to start and dispatch the activate event
  // to the version.
  void ActivateWaitingVersion(bool delay);
  void ContinueActivation(
      scoped_refptr<ServiceWorkerVersion> activating_version);
  void DispatchActivateEvent(
      scoped_refptr<ServiceWorkerVersion> activating_version,
      blink::ServiceWorkerStatusCode start_worker_status);
  void OnActivateEventFinished(
      scoped_refptr<ServiceWorkerVersion> activating_version,
      blink::ServiceWorkerStatusCode status);

  void OnDeleteFinished(blink::ServiceWorkerStatusCode status);

  // This method corresponds to the [[ClearRegistration]] algorithm.
  void Clear();

  void OnRestoreFinished(StatusCallback callback,
                         scoped_refptr<ServiceWorkerVersion> version,
                         blink::ServiceWorkerStatusCode status);

  const GURL scope_;
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  const int64_t registration_id_;
  Status status_;
  bool should_activate_when_ready_;
  blink::mojom::NavigationPreloadState navigation_preload_state_;
  base::Time last_update_check_;
  base::TimeDelta self_update_delay_;
  int64_t resources_total_size_bytes_;

  // This registration is the primary owner of these versions.
  scoped_refptr<ServiceWorkerVersion> active_version_;
  scoped_refptr<ServiceWorkerVersion> waiting_version_;
  scoped_refptr<ServiceWorkerVersion> installing_version_;

  base::ObserverList<Listener>::Unchecked listeners_;
  std::vector<base::OnceClosure> registration_finished_callbacks_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // |lame_duck_timer_| is started when the active version is considered a lame
  // duck: the waiting version called skipWaiting() or the active
  // version has no controllees, but activation is waiting for the active
  // version to finish its inflight requests.
  // It is stopped when activation completes or the active version is no
  // longer considered a lame duck.
  base::RepeatingTimer lame_duck_timer_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistration);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

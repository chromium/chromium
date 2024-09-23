// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/navigation_preload_state.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_ancestor_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;
struct ServiceWorkerRegistrationInfo;

// Represents the core of a service worker registration object. Other
// registration derivatives (WebServiceWorkerRegistration etc) ultimately refer
// to this class. This is refcounted via ServiceWorkerRegistrationObjectHost to
// facilitate multiple controllees being associated with the same registration.
class CONTENT_EXPORT ServiceWorkerRegistration
    : public base::RefCounted<ServiceWorkerRegistration> {
 public:
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode status)>;

  class CONTENT_EXPORT Listener {
   public:
    virtual void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) {}
    virtual void OnUpdateViaCacheChanged(
        ServiceWorkerRegistration* registation) {}
    virtual void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) {}
    virtual void OnRegistrationFinishedUninstalling(
        ServiceWorkerRegistration* registration) {}
    virtual void OnRegistrationDeleted(
        ServiceWorkerRegistration* registration) {}
    virtual void OnUpdateFound(ServiceWorkerRegistration* registration) {}
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

  // This is a factory method and should be used instead of the constructor.
  static scoped_refptr<ServiceWorkerRegistration> Create(
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      const blink::StorageKey& key,
      int64_t registration_id,
      base::WeakPtr<ServiceWorkerContextCore> context,
      blink::mojom::AncestorFrameType ancestor_frame_type);

  ServiceWorkerRegistration(const ServiceWorkerRegistration&) = delete;
  ServiceWorkerRegistration& operator=(const ServiceWorkerRegistration&) =
      delete;

  int64_t id() const { return registration_id_; }
  const GURL& scope() const { return scope_; }
  const blink::StorageKey& key() const { return key_; }
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache() const {
    return update_via_cache_;
  }
  blink::mojom::AncestorFrameType ancestor_frame_type() const {
    return ancestor_frame_type_;
  }

  bool is_deleted() const { return status_ != Status::kIntact; }
  bool is_uninstalling() const { return status_ == Status::kUninstalling; }
  bool is_uninstalled() const { return status_ == Status::kUninstalled; }
  Status status() const { return status_; }
  void SetStatus(Status status);

  // Returns true when this registration is stored in storage and corresponding
  // ServiceWorkerContextCore is valid. The context becomes invalid when
  // ServiceWorkerContextCore::DeleteAndStartOver() is called.
  bool IsStored() const;
  void SetStored();
  void UnsetStored();

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

  // Takes over control of container hosts which are currently not controlled or
  // controlled by other registrations.
  void ClaimClients();

  // Deletes this registration from storage immediately. Triggers the
  // [[ClearRegistration]] algorithm when the currently active version has no
  // controllees.
  void DeleteAndClearWhenReady();

  // Deletes this registration from storage immediately and then triggers the
  // [[ClearRegistration]] algorithm.
  void DeleteAndClearImmediately();

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

  // Called when all controllees are removed from |version|.
  void OnNoControllees(ServiceWorkerVersion* version);

  // Called when there is no work in |version|.
  void OnNoWork(ServiceWorkerVersion* version);

  // Delays an update if it is called by a ServiceWorker without controllee, to
  // prevent workers from running forever (see https://crbug.com/805496).
  void DelayUpdate(
      ServiceWorkerVersion& version,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
          callback);
  void ExecuteUpdate(
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
          callback);

  std::string ComposeUpdateErrorMessagePrefix(
      const ServiceWorkerVersion* version_to_update) const;

 protected:
  virtual ~ServiceWorkerRegistration();

 private:
  friend class base::RefCounted<ServiceWorkerRegistration>;
  friend class ServiceWorkerActivationTest;

  // Callers should use `ServiceWorkerRegistration` factory `Create()` method
  // instead.
  ServiceWorkerRegistration(
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      const blink::StorageKey& key,
      int64_t registration_id,
      base::WeakPtr<ServiceWorkerContextCore> context,
      blink::mojom::AncestorFrameType ancestor_frame_type);

  void UnsetVersionInternal(
      ServiceWorkerVersion* version,
      blink::mojom::ChangedServiceWorkerObjectsMask* mask);

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

  // Called back from ServiceWorkerContextCore when an update is complete.
  void UpdateComplete(
      blink::mojom::ServiceWorkerRegistrationObjectHost::UpdateCallback
          callback,
      blink::ServiceWorkerStatusCode status,
      const std::string& status_message,
      int64_t registration_id);

  enum class StoreState {
    // This registration is not stored yet in storage.
    kNotStored,
    // This registration is stored in storage.
    kStored,
  };

  const GURL scope_;
  const blink::StorageKey key_;
  blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  const int64_t registration_id_;
  Status status_;
  StoreState store_state_;
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

  // TODO(crbug.com/40737650): Remove once the bug is fixed.
  bool in_activate_waiting_version_ = false;

  const blink::mojom::AncestorFrameType ancestor_frame_type_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_H_

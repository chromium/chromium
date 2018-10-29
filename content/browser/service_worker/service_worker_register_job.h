// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_update_checker.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {

// Handles the initial registration of a Service Worker and the
// subsequent update of existing registrations.
//
// The control flow includes most or all of the following,
// depending on what is already registered:
//  - creating a ServiceWorkerRegistration instance if there isn't
//    already something registered
//  - creating a ServiceWorkerVersion for the new version.
//  - starting a worker for the ServiceWorkerVersion
//  - firing the 'install' event at the ServiceWorkerVersion
//  - firing the 'activate' event at the ServiceWorkerVersion
//  - waiting for older ServiceWorkerVersions to deactivate
//  - designating the new version to be the 'active' version
//  - updating storage
class ServiceWorkerRegisterJob : public ServiceWorkerRegisterJobBase {
 public:
  typedef base::OnceCallback<void(blink::ServiceWorkerStatusCode status,
                                  const std::string& status_message,
                                  ServiceWorkerRegistration* registration)>
      RegistrationCallback;

  // For registration jobs.
  CONTENT_EXPORT ServiceWorkerRegisterJob(
      base::WeakPtr<ServiceWorkerContextCore> context,
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options);

  // For update jobs.
  CONTENT_EXPORT ServiceWorkerRegisterJob(
      base::WeakPtr<ServiceWorkerContextCore> context,
      ServiceWorkerRegistration* registration,
      bool force_bypass_cache,
      bool skip_script_comparison);
  ~ServiceWorkerRegisterJob() override;

  // Registers a callback to be called when the promise would resolve (whether
  // successfully or not). Multiple callbacks may be registered.
  void AddCallback(RegistrationCallback callback);

  // ServiceWorkerRegisterJobBase implementation:
  void Start() override;
  void Abort() override;
  bool Equals(ServiceWorkerRegisterJobBase* job) const override;
  RegistrationJobType GetType() const override;

  void DoomInstallingWorker();

 private:
  enum Phase {
    INITIAL,
    START,
    REGISTER,
    UPDATE,
    INSTALL,
    STORE,
    COMPLETE,
    ABORT,
  };

  // Holds internal state of ServiceWorkerRegistrationJob, to compel use of the
  // getter/setter functions.
  struct Internal {
    Internal();
    ~Internal();
    scoped_refptr<ServiceWorkerRegistration> registration;

    // Holds the version created by this job. It can be the 'installing',
    // 'waiting', or 'active' version depending on the phase.
    scoped_refptr<ServiceWorkerVersion> new_version;
  };

  void set_registration(scoped_refptr<ServiceWorkerRegistration> registration);
  ServiceWorkerRegistration* registration();
  void set_new_version(ServiceWorkerVersion* version);
  ServiceWorkerVersion* new_version();

  void SetPhase(Phase phase);

  void StartImpl();
  void ContinueWithRegistration(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void ContinueWithUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  // This method is only called when ServiceWorkerImportedScriptUpdateCheck is
  // enabled.
  void OnUpdateCheckFinished(bool script_changed);

  void RegisterAndContinue();
  void ContinueWithUninstallingRegistration(
      scoped_refptr<ServiceWorkerRegistration> existing_registration,
      blink::ServiceWorkerStatusCode status);
  void ContinueWithRegistrationForSameScriptUrl(
      scoped_refptr<ServiceWorkerRegistration> existing_registration,
      blink::ServiceWorkerStatusCode status);
  void UpdateAndContinue();
  void OnStartWorkerFinished(blink::ServiceWorkerStatusCode status);
  void OnStoreRegistrationComplete(blink::ServiceWorkerStatusCode status);
  void InstallAndContinue();
  void DispatchInstallEvent(blink::ServiceWorkerStatusCode start_worker_status);
  void OnInstallFinished(int request_id,
                         blink::mojom::ServiceWorkerEventStatus event_status,
                         bool has_fetch_handler,
                         base::TimeTicks dispatch_event_time);
  void OnInstallFailed(blink::ServiceWorkerStatusCode status);
  void Complete(blink::ServiceWorkerStatusCode status);
  void Complete(blink::ServiceWorkerStatusCode status,
                const std::string& status_message);
  void CompleteInternal(blink::ServiceWorkerStatusCode status,
                        const std::string& status_message);
  void ResolvePromise(blink::ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);

  void AddRegistrationToMatchingProviderHosts(
      ServiceWorkerRegistration* registration);

  void OnPausedAfterDownload();

  void BumpLastUpdateCheckTimeIfNeeded();

  // The ServiceWorkerContextCore object should always outlive this.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  std::unique_ptr<ServiceWorkerUpdateChecker> update_checker_;

  RegistrationJobType job_type_;
  const GURL scope_;
  GURL script_url_;
  // "A job has a worker type ("classic" or "module")."
  // https://w3c.github.io/ServiceWorker/#dfn-job-worker-type
  blink::mojom::ScriptType worker_script_type_ =
      blink::mojom::ScriptType::kClassic;
  const blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  std::vector<RegistrationCallback> callbacks_;
  Phase phase_;
  Internal internal_;
  bool doom_installing_worker_;
  bool is_promise_resolved_;
  bool should_uninstall_on_failure_;
  bool force_bypass_cache_;
  bool skip_script_comparison_;
  blink::ServiceWorkerStatusCode promise_resolved_status_;
  std::string promise_resolved_status_message_;
  scoped_refptr<ServiceWorkerRegistration> promise_resolved_registration_;

  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

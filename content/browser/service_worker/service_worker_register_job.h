// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_update_checker.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_ancestor_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerNewScriptFetcher;

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
  ServiceWorkerRegisterJob(
      ServiceWorkerContextCore* context,
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      const blink::StorageKey& key,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      const GlobalRenderFrameHostId& requesting_frame_id,
      blink::mojom::AncestorFrameType ancestor_frame_type,
      PolicyContainerPolicies policy_container_policies);

  // For update jobs.
  ServiceWorkerRegisterJob(ServiceWorkerContextCore* context,
                           ServiceWorkerRegistration* registration,
                           bool force_bypass_cache,
                           bool skip_script_comparison,
                           blink::mojom::FetchClientSettingsObjectPtr
                               outside_fetch_client_settings_object);

  ServiceWorkerRegisterJob(const ServiceWorkerRegisterJob&) = delete;
  ServiceWorkerRegisterJob& operator=(const ServiceWorkerRegisterJob&) = delete;

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
  ServiceWorkerRegistration* registration() const;
  void set_new_version(scoped_refptr<ServiceWorkerVersion> version);
  ServiceWorkerVersion* new_version();

  void SetPhase(Phase phase);

  void StartImpl();
  void ContinueWithRegistration(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void ContinueWithUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  bool IsUpdateCheckNeeded() const;

  // Refer ServiceWorkerUpdateChecker::UpdateStatusCallback for the meaning of
  // the parameters.
  void OnUpdateCheckFinished(
      ServiceWorkerSingleScriptUpdateChecker::Result result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
          failure_info,
      // TODO(crbug.com/40241479): `updated_sha256_script_checksums` will be
      // used in a follow-up CL.
      const std::map<GURL, std::string>& updated_sha256_script_checksums);

  void RegisterAndContinue();
  void ContinueWithNewRegistration(
      scoped_refptr<ServiceWorkerRegistration> new_registration);
  void ContinueWithUninstallingRegistration(
      scoped_refptr<ServiceWorkerRegistration> existing_registration,
      blink::ServiceWorkerStatusCode status);
  void ContinueWithRegistrationWithSameRegistrationOptions(
      scoped_refptr<ServiceWorkerRegistration> existing_registration,
      blink::ServiceWorkerStatusCode status);
  void UpdateAndContinue();

  // With PlzServiceWorker, we start fetching the script before starting the
  // worker. The 3 functions below represent the expected order of execution
  // in this process:
  // - Devtools might decide it wants to auto-attach to new targets and to start
  //   intercepting messages before the fetch starts. If so it needs to start
  //   some handlers asynchronously. We pass down to the handlers a "throttle"
  //   that can resume script fetching via a callback when ready.
  // - We create a factory and a pass it to a ServiceWorkerNewScriptFetcher.
  //   Once the script fetch succeeded (or failed), it calls into the final
  //   step.
  // - We inspect the response, determine if its a failure or not and update
  //   state. If successful we start the worker with load parameters returned by
  //   the ServiceWorkerNewScriptFetcher.
  void MaybeThrottleForDevToolsBeforeStartingScriptFetch(
      scoped_refptr<ServiceWorkerVersion> version);
  void StartScriptFetchForNewWorker(
      scoped_refptr<ServiceWorkerVersion> version);
  void OnScriptFetchCompleted(
      scoped_refptr<ServiceWorkerVersion> version,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params);

  // Starts a service worker for [[Update]]. The script comparison has finished
  // at this point. It starts install phase.
  void StartWorkerForUpdate(scoped_refptr<ServiceWorkerVersion> version);
  void OnStartWorkerFinished(blink::ServiceWorkerStatusCode status);
  void OnStoreRegistrationComplete(blink::ServiceWorkerStatusCode status);
  void InstallAndContinue();
  void DispatchInstallEvent(blink::ServiceWorkerStatusCode start_worker_status);
  void OnInstallFinished(int request_id,
                         blink::mojom::ServiceWorkerEventStatus event_status,
                         uint32_t fetch_count);
  void OnInstallFailed(uint32_t fetch_count,
                       blink::ServiceWorkerStatusCode status);
  void Complete(blink::ServiceWorkerStatusCode status);
  void Complete(blink::ServiceWorkerStatusCode status,
                const std::string& status_message);
  void CompleteInternal(blink::ServiceWorkerStatusCode status,
                        const std::string& status_message);
  void ResolvePromise(blink::ServiceWorkerStatusCode status,
                      const std::string& status_message,
                      ServiceWorkerRegistration* registration);

  void AddRegistrationToMatchingContainerHosts(
      ServiceWorkerRegistration* registration);

  void OnPausedAfterDownload();

  void BumpLastUpdateCheckTimeIfNeeded();

  // The ServiceWorkerContextCore object must outlive this.
  const raw_ptr<ServiceWorkerContextCore> context_;

  // Valid when the worker is being updated.
  std::unique_ptr<ServiceWorkerUpdateChecker> update_checker_;
  // Valid when the worker is new.
  std::unique_ptr<ServiceWorkerNewScriptFetcher> new_script_fetcher_;

  RegistrationJobType job_type_;
  const GURL scope_;
  GURL script_url_;
  const blink::StorageKey key_;
  // "A job has a worker type ("classic" or "module")."
  // https://w3c.github.io/ServiceWorker/#dfn-job-worker-type
  blink::mojom::ScriptType worker_script_type_ =
      blink::mojom::ScriptType::kClassic;
  const blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  // "A job has a client (a service worker client). It is initially null."
  // https://w3c.github.io/ServiceWorker/#dfn-job-client
  // This fetch client settings object roughly corresponds to the job's client.
  blink::mojom::FetchClientSettingsObjectPtr
      outside_fetch_client_settings_object_;
  std::vector<RegistrationCallback> callbacks_;
  Phase phase_;
  Internal internal_;
  bool is_promise_resolved_;
  bool should_uninstall_on_failure_;
  bool force_bypass_cache_;
  bool skip_script_comparison_;
  blink::ServiceWorkerStatusCode promise_resolved_status_;
  std::string promise_resolved_status_message_;
  scoped_refptr<ServiceWorkerRegistration> promise_resolved_registration_;
  const GlobalRenderFrameHostId requesting_frame_id_;
  const blink::mojom::AncestorFrameType ancestor_frame_type_;
  PolicyContainerPolicies creator_policy_container_policies_;

  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

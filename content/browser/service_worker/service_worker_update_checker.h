// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/service_worker/service_worker_single_script_update_checker.h"
#include "content/browser/service_worker/service_worker_updated_script_loader.h"
#include "content/common/content_export.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// This is responsible for byte-for-byte update checking. Mostly corresponding
// to step 1-9 in [[Update]] in the spec, but this stops to fetch scripts after
// any changes found.
// https://w3c.github.io/ServiceWorker/#update-algorithm
//
// This is owned and used by ServiceWorkerRegisterJob as a part of the update
// logic.
class CONTENT_EXPORT ServiceWorkerUpdateChecker {
 public:
  // Data of each compared script needed in remaining update process
  struct CONTENT_EXPORT ComparedScriptInfo {
    ComparedScriptInfo();
    ComparedScriptInfo(
        int64_t old_resource_id,
        ServiceWorkerSingleScriptUpdateChecker::Result result,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
            paused_state,
        std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
            failure_info);
    ComparedScriptInfo(ComparedScriptInfo&& other);
    ComparedScriptInfo& operator=(ComparedScriptInfo&& other);
    ~ComparedScriptInfo();

    // Resource id of the compared script already in storage
    int64_t old_resource_id;

    // Compare result of a single script
    ServiceWorkerSingleScriptUpdateChecker::Result result;

    // This is only valid for script compared to be different. It consists of
    // some state variables to continue the update process, including
    // ServiceWorkerCacheWriter, and Mojo endpoints for downloading.
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
        paused_state;

    // This is set when |result| is kFailed. This is used to keep the error code
    // which the update checker saw to the renderer when installing the worker.
    std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
        failure_info;
  };

  // This is to notify the update check result. Value of |result| can be:
  // 1. ServiceWorkerSingleScriptUpdateChecker::Result::kIdentical
  //    All the scripts are identical with existing version, no need to update.
  //    |failure_info| is nullptr.
  // 2. ServiceWorkerSingleScriptUpdateChecker::Result::kDifferent
  //    Some script changed, update is needed. |failure_info| is nullptr.
  // 3. ServiceWorkerSingleScriptUpdateChecker::Result::kFailed
  //    The update check failed, detailed error info is in |failure_info|.
  //
  // |updated_sha256_script_checksums| contains the pair of updated hash string
  // and its script url. It's normally empty, except for the case when
  // |main_script_sha256_checksum_| has a value, and the update check finished
  // with kIdentical.
  using UpdateStatusCallback = base::OnceCallback<void(
      ServiceWorkerSingleScriptUpdateChecker::Result result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
          failure_info,
      const std::map<GURL, std::string>& updated_sha256_script_checksums)>;

  ServiceWorkerUpdateChecker() = delete;

  ServiceWorkerUpdateChecker(
      std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>
          scripts_to_compare,
      const GURL& main_script_url,
      int64_t main_script_resource_id,
      const std::optional<std::string>& main_script_sha256_checksum,
      scoped_refptr<ServiceWorkerVersion> version_to_update,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      bool force_bypass_cache,
      blink::mojom::ScriptType worker_script_type,
      blink::mojom::ServiceWorkerUpdateViaCache update_via_cache,
      base::TimeDelta time_since_last_check,
      ServiceWorkerContextCore* context,
      blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings_object);

  ServiceWorkerUpdateChecker(const ServiceWorkerUpdateChecker&) = delete;
  ServiceWorkerUpdateChecker& operator=(const ServiceWorkerUpdateChecker&) =
      delete;

  ~ServiceWorkerUpdateChecker();

  // |callback| is always triggered when the update check finishes.
  void Start(UpdateStatusCallback callback);

  // This transfers the ownership of the check result to the caller. It only
  // contains the information about scripts which have already been compared.
  std::map<GURL, ComparedScriptInfo> TakeComparedResults();

  void OnOneUpdateCheckFinished(
      int64_t old_resource_id,
      const GURL& script_url,
      ServiceWorkerSingleScriptUpdateChecker::Result result,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::FailureInfo>
          failure_info,
      std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker::PausedState>
          paused_state,
      const std::optional<std::string>& sha256_checksum);

  const GURL& updated_script_url() const { return updated_script_url_; }
  bool network_accessed() const { return network_accessed_; }
  const scoped_refptr<PolicyContainerHost> policy_container_host() const {
    return policy_container_host_;
  }

 private:
  void CheckOneScript(const GURL& url, const int64_t resource_id);
  void OnResourceIdAssignedForOneScriptCheck(const GURL& url,
                                             const int64_t resource_id,
                                             const int64_t new_resource_id);

  const GURL main_script_url_;
  const int64_t main_script_resource_id_;
  const std::optional<std::string> main_script_sha256_checksum_;

  std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>
      scripts_to_compare_;
  size_t next_script_index_to_compare_ = 0;
  std::map<GURL, ComparedScriptInfo> script_check_results_;

  // The URL of the script for which a byte-for-byte change was found, or else
  // the empty GURL if there is no such script.
  GURL updated_script_url_;

  // The version which triggered this update.
  scoped_refptr<ServiceWorkerVersion> version_to_update_;

  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> running_checker_;

  UpdateStatusCallback callback_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  const bool force_bypass_cache_;
  const blink::mojom::ScriptType worker_script_type_;
  const blink::mojom::ServiceWorkerUpdateViaCache update_via_cache_;
  const base::TimeDelta time_since_last_check_;

  // True if any at least one of the scripts is fetched by network.
  bool network_accessed_ = false;

  // Contains the pair of updated hash string and its script url.
  // This manages newly calculated checksum strings so that we store them on
  // memory and database.
  std::map<GURL, std::string> updated_sha256_script_checksums_;

  scoped_refptr<PolicyContainerHost> policy_container_host_;

  // |context_| outlives |this| because it owns |this| through
  // ServiceWorkerJobCoordinator and ServiceWorkerRegisterJob.
  const raw_ptr<ServiceWorkerContextCore> context_;

  blink::mojom::FetchClientSettingsObjectPtr fetch_client_settings_object_;

  base::WeakPtrFactory<ServiceWorkerUpdateChecker> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

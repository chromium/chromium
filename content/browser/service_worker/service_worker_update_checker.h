// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

#include "base/callback.h"
#include "content/browser/service_worker/service_worker_database.h"
#include "content/browser/service_worker/service_worker_single_script_update_checker.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class ServiceWorkerVersion;

class ServiceWorkerUpdateChecker {
 public:
  using UpdateStatusCallback = base::OnceCallback<void(bool)>;

  ServiceWorkerUpdateChecker(
      std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare,
      scoped_refptr<ServiceWorkerVersion> version_to_update,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory);
  ~ServiceWorkerUpdateChecker();

  // |callback| is always triggered when Start() finishes. If the scripts are
  // found to have any changes, the argument of |callback| is true and otherwise
  // false.
  void Start(UpdateStatusCallback callback);

  void OnOneUpdateCheckFinished(bool is_script_changed);

 private:
  void CheckOneScript();

  std::vector<ServiceWorkerDatabase::ResourceRecord> scripts_to_compare_;
  size_t scripts_compared_ = 0;

  // The version which triggered this update.
  scoped_refptr<ServiceWorkerVersion> version_to_update_;

  std::unique_ptr<ServiceWorkerSingleScriptUpdateChecker> running_checker_;

  UpdateStatusCallback callback_;

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  base::WeakPtrFactory<ServiceWorkerUpdateChecker> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UPDATE_CHECKER_H_

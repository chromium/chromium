// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_TEST_SERVICE_WORKER_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_TEST_SERVICE_WORKER_OBSERVER_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace base {
class TestSimpleTaskRunner;
}

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

class ServiceWorkerContextWrapper;

// Observes events related to service workers. Exposes convenience methods for
// use in tests.
class TestServiceWorkerObserver : public ServiceWorkerContextCoreObserver {
 public:
  explicit TestServiceWorkerObserver(
      scoped_refptr<ServiceWorkerContextWrapper> wrapper);

  TestServiceWorkerObserver(const TestServiceWorkerObserver&) = delete;
  TestServiceWorkerObserver& operator=(const TestServiceWorkerObserver&) =
      delete;

  ~TestServiceWorkerObserver() override;

  // Returns when |version| reaches |status|.
  void RunUntilStatusChange(ServiceWorkerVersion* version,
                            ServiceWorkerVersion::Status status);

  // Returns when |version| reaches ACTIVATED. |runner| should
  // be the version's registration's task runner. This function is
  // useful for skipping the 1 second wall time delay for the activate event in
  // ServiceWorkerRegistration. If not for that delay, a single
  // RunUntilStatusChange() call for ACTIVATED would suffice.
  void RunUntilActivated(ServiceWorkerVersion* version,
                         scoped_refptr<base::TestSimpleTaskRunner> runner);

  // Returns when there is a live worker service version.
  void RunUntilLiveVersion();

 private:
  // ServiceWorkerContextCoreObserver overrides:
  void OnVersionStateChanged(int64_t version_id,
                             const GURL& scope,
                             const blink::StorageKey& key,
                             ServiceWorkerVersion::Status status) override;
  void OnNewLiveVersion(const ServiceWorkerVersionInfo& version_info) override;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  int64_t version_id_for_status_change_ = -1;
  ServiceWorkerVersion::Status status_for_status_change_ =
      ServiceWorkerVersion::NEW;
  base::OnceClosure quit_closure_for_status_change_;

  base::OnceClosure quit_closure_for_live_version_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_TEST_SERVICE_WORKER_OBSERVER_H_

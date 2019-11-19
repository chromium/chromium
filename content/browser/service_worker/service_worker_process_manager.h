// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"

class GURL;

namespace content {

class BrowserContext;
class SiteInstance;
class StoragePartitionImpl;

// Interacts with the UI thread to keep RenderProcessHosts alive while the
// ServiceWorker system is using them. There is one process manager per
// ServiceWorkerContextWrapper. Each instance of ServiceWorkerProcessManager is
// destroyed on the UI thread shortly after its ServiceWorkerContextWrapper is
// destroyed.
class CONTENT_EXPORT ServiceWorkerProcessManager {
 public:
  // The return value for AllocateWorkerProcess().
  struct AllocatedProcessInfo {
    // Same as RenderProcessHost::GetID().
    int process_id;

    // This must be one of NEW_PROCESS, EXISTING_UNREADY_PROCESS or
    // EXISTING_READY_PROCESS.
    ServiceWorkerMetrics::StartSituation start_situation;
  };

  // |*this| must be owned by a ServiceWorkerContextWrapper in a
  // StoragePartition within |browser_context|.
  explicit ServiceWorkerProcessManager(BrowserContext* browser_context);

  // Shutdown must be called before the ProcessManager is destroyed.
  ~ServiceWorkerProcessManager();

  // Called on the UI thread.
  BrowserContext* browser_context();

  // Synchronously prevents new processes from being allocated
  // and drops references to RenderProcessHosts. Called on the UI thread.
  void Shutdown();

  // Returns true if Shutdown() has been called. May be called by any thread.
  bool IsShutdown();

  // Returns a reference to a renderer process suitable for starting the service
  // worker described by |emdedded_worker_id|, and |script_url|. The process
  // will be kept alive until ReleaseWorkerProcess() is called.
  //
  // An existing process is used when possible. If |can_use_existing_process| is
  // false, or a suitable existing process is not found, a new process may be
  // created.
  //
  // If blink::ServiceWorkerStatusCode::kOk is returned,
  // |out_info| contains information about the process.
  //
  // Called on the UI thread.
  blink::ServiceWorkerStatusCode AllocateWorkerProcess(
      int embedded_worker_id,
      const GURL& script_url,
      bool can_use_existing_process,
      AllocatedProcessInfo* out_info);

  // Drops a reference to a process that was running a Service Worker, and its
  // SiteInstance. This must match a call to AllocateWorkerProcess().
  //
  // Called on the UI thread.
  void ReleaseWorkerProcess(int embedded_worker_id);

  // Sets a single process ID that will be used for all embedded workers.  This
  // bypasses the work of creating a process and managing its worker refcount so
  // that unittests can run without a BrowserContext.  The test is in charge of
  // making sure this is only called on the same thread as runs the UI message
  // loop.
  void SetProcessIdForTest(int process_id) {
    process_id_for_test_ = process_id;
  }

  // Sets the process ID to be used for tests that force creating a new process.
  void SetNewProcessIdForTest(int process_id) {
    new_process_id_for_test_ = process_id;
  }

  // AsWeakPtr() can be called from any thread, but the WeakPtr must be
  // dereferenced on the UI thread only.
  base::WeakPtr<ServiceWorkerProcessManager> AsWeakPtr() { return weak_this_; }

  void set_storage_partition(StoragePartitionImpl* storage_partition) {
    storage_partition_ = storage_partition;
  }

  SiteInstance* GetSiteInstanceForWorker(int embedded_worker_id);

 private:
  friend class ServiceWorkerProcessManagerTest;

  // Guarded by |browser_context_lock_|.
  // Written only on the UI thread, so the UI thread doesn't need to acquire the
  // lock when reading. Can be read from other threads with the lock.
  BrowserContext* browser_context_;

  // Protects |browser_context_|.
  base::Lock browser_context_lock_;

  //////////////////////////////////////////////////////////////////////////////
  // All fields below are only accessed on the UI thread.

  // May be null during initialization and in unit tests.
  StoragePartitionImpl* storage_partition_;

  // Maps the ID of a running EmbeddedWorkerInstance to the SiteInstance whose
  // renderer process it's running inside. Since the embedded workers themselves
  // live on the IO thread, this can be slightly out of date:
  //  * The map is populated while a worker is STARTING and before it's RUNNING.
  //  * The map is depopulated in a message sent as the worker becomes STOPPED.
  std::map<int, scoped_refptr<SiteInstance>> worker_process_map_;

  // In unit tests, this will be returned as the process for all
  // EmbeddedWorkerInstances.
  int process_id_for_test_;
  int new_process_id_for_test_;

  // Used to double-check that we don't access *this after it's destroyed.
  base::WeakPtr<ServiceWorkerProcessManager> weak_this_;
  base::WeakPtrFactory<ServiceWorkerProcessManager> weak_this_factory_{this};
};

}  // namespace content

namespace std {
// Specialized to post the deletion to the UI thread.
template <>
struct CONTENT_EXPORT default_delete<content::ServiceWorkerProcessManager> {
  void operator()(content::ServiceWorkerProcessManager* ptr) const;
};
}  // namespace std

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_

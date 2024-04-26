// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "content/browser/process_lock.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerProcessManager::ServiceWorkerProcessManager()
    : storage_partition_(nullptr),
      process_id_for_test_(ChildProcessHost::kInvalidUniqueID),
      new_process_id_for_test_(ChildProcessHost::kInvalidUniqueID),
      force_new_process_for_test_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerProcessManager::~ServiceWorkerProcessManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsShutdown())
      << "Call Shutdown() before destroying |this|, so that racing method "
      << "invocations don't use a destroyed BrowserContext.";
  // TODO(horo): Remove after collecting crash data.
  // Temporary checks to verify that ServiceWorkerProcessManager doesn't prevent
  // render process hosts from shutting down: crbug.com/639193
  CHECK(worker_process_map_.empty());
}

void ServiceWorkerProcessManager::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // `StoragePartitionImpl` might be destroyed before `this` is destroyed. Set
  // `storage_partition_` to nullptr to avoid holding a dangling ptr.
  storage_partition_ = nullptr;

  // In single-process mode, Shutdown() is called when deleting the default
  // browser context, which is itself destroyed after the RenderProcessHost.
  // The refcount decrement can be skipped anyway since there's only one
  // process.
  if (!RenderProcessHost::run_renderer_in_process()) {
    for (const auto& it : worker_process_map_) {
      if (it.second->HasProcess()) {
        RenderProcessHost* process = it.second->GetProcess();
        if (!process->AreRefCountsDisabled())
          process->DecrementWorkerRefCount();
      }
    }
  }
  worker_process_map_.clear();
  is_shutdown_ = true;
}

bool ServiceWorkerProcessManager::IsShutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_shutdown_;
}

blink::ServiceWorkerStatusCode
ServiceWorkerProcessManager::AllocateWorkerProcess(
    int embedded_worker_id,
    const GURL& script_url,
    network::mojom::CrossOriginEmbedderPolicyValue coep_value,
    bool can_use_existing_process,
    blink::mojom::AncestorFrameType ancestor_frame_type,
    AllocatedProcessInfo* out_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (force_new_process_for_test_)
    can_use_existing_process = false;

  out_info->process_id = ChildProcessHost::kInvalidUniqueID;
  out_info->start_situation = ServiceWorkerMetrics::StartSituation::UNKNOWN;

  if (process_id_for_test_ != ChildProcessHost::kInvalidUniqueID) {
    // Let tests specify the returned process ID.
    int result = can_use_existing_process ? process_id_for_test_
                                          : new_process_id_for_test_;
    out_info->process_id = result;
    out_info->start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS;
    return blink::ServiceWorkerStatusCode::kOk;
  }

  if (IsShutdown()) {
    return blink::ServiceWorkerStatusCode::kErrorAbort;
  }

  DCHECK(!base::Contains(worker_process_map_, embedded_worker_id))
      << embedded_worker_id << " already has a process allocated";

  // Create a SiteInstance to get the renderer process from.
  //
  // TODO(alexmos): Support CrossOriginIsolated for guests.
  DCHECK(storage_partition_);
  const bool is_guest = storage_partition_->is_guest();
  const bool is_fenced =
      ancestor_frame_type == blink::mojom::AncestorFrameType::kFencedFrame &&
      SiteIsolationPolicy::IsProcessIsolationForFencedFramesEnabled();
  const bool is_coop_coep_cross_origin_isolated =
      !is_guest && network::CompatibleWithCrossOriginIsolated(coep_value);
  UrlInfo url_info(
      UrlInfoInit(script_url)
          .WithStoragePartitionConfig(storage_partition_->GetConfig())
          .WithWebExposedIsolationInfo(
              is_coop_coep_cross_origin_isolated
                  ? WebExposedIsolationInfo::CreateIsolated(
                        url::Origin::Create(script_url))
                  : WebExposedIsolationInfo::CreateNonIsolated()));
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForServiceWorker(
          storage_partition_->browser_context(), url_info,
          can_use_existing_process, is_guest, is_fenced);

  // Get the process from the SiteInstance.
  RenderProcessHost* rph = site_instance->GetProcess();
  DCHECK(!storage_partition_ ||
         rph->InSameStoragePartition(storage_partition_));

  // Let the embedder grant the worker process access to origins if the worker
  // is locked to the same origin as the worker.
  if (rph->GetProcessLock().MatchesOrigin(url::Origin::Create(script_url))) {
    GetContentClient()
        ->browser()
        ->GrantAdditionalRequestPrivilegesToWorkerProcess(rph->GetID(),
                                                          script_url);
  }

  ServiceWorkerMetrics::StartSituation start_situation;
  if (!rph->IsInitializedAndNotDead()) {
    // IsInitializedAndNotDead() is false means that Init() has not been called
    // or the process has been killed.
    start_situation = ServiceWorkerMetrics::StartSituation::NEW_PROCESS;
  } else if (!rph->IsReady()) {
    start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS;
  } else {
    start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS;
  }

  if (!rph->Init()) {
    LOG(ERROR) << "Couldn't start a new process!";
    return blink::ServiceWorkerStatusCode::kErrorProcessNotFound;
  }

  worker_process_map_.emplace(embedded_worker_id, std::move(site_instance));
  if (!rph->AreRefCountsDisabled())
    rph->IncrementWorkerRefCount();
  out_info->process_id = rph->GetID();
  out_info->start_situation = start_situation;
  return blink::ServiceWorkerStatusCode::kOk;
}

void ServiceWorkerProcessManager::ReleaseWorkerProcess(int embedded_worker_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (process_id_for_test_ != ChildProcessHost::kInvalidUniqueID) {
    // Unittests don't increment or decrement the worker refcount of a
    // RenderProcessHost.
    return;
  }

  if (IsShutdown()) {
    // Shutdown already released all instances.
    DCHECK(worker_process_map_.empty());
    return;
  }

  auto it = worker_process_map_.find(embedded_worker_id);
  // ReleaseWorkerProcess could be called for a nonexistent worker id, for
  // example, when request to start a worker is aborted on the IO thread during
  // process allocation that is failed on the UI thread.
  if (it == worker_process_map_.end())
    return;

  if (it->second->HasProcess()) {
    RenderProcessHost* process = it->second->GetProcess();
    if (!process->AreRefCountsDisabled())
      process->DecrementWorkerRefCount();
  }
  worker_process_map_.erase(it);
}

base::WeakPtr<ServiceWorkerProcessManager>
ServiceWorkerProcessManager::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_ptr_factory_.GetWeakPtr();
}

SiteInstance* ServiceWorkerProcessManager::GetSiteInstanceForWorker(
    int embedded_worker_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = worker_process_map_.find(embedded_worker_id);
  if (it == worker_process_map_.end())
    return nullptr;
  return it->second.get();
}

}  // namespace content

namespace std {
// Destroying ServiceWorkerProcessManagers only on the UI thread allows the
// member WeakPtr to safely guard the object's lifetime when used on that
// thread.
void default_delete<content::ServiceWorkerProcessManager>::operator()(
    content::ServiceWorkerProcessManager* ptr) const {
  content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, ptr);
}
}  // namespace std

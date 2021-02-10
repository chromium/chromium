// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {
namespace {

void AdaptSessionStorageUsageInfo(
    DOMStorageContext::GetSessionStorageUsageCallback callback,
    std::vector<storage::mojom::SessionStorageUsageInfoPtr> usage) {
  std::vector<SessionStorageUsageInfo> result;
  result.reserve(usage.size());
  for (const auto& entry : usage) {
    SessionStorageUsageInfo info;
    info.origin = entry->origin.GetURL();
    info.namespace_id = entry->namespace_id;
    result.push_back(std::move(info));
  }
  std::move(callback).Run(result);
}

void AdaptStorageUsageInfo(
    DOMStorageContext::GetLocalStorageUsageCallback callback,
    std::vector<storage::mojom::StorageUsageInfoPtr> usage) {
  std::vector<StorageUsageInfo> result;
  result.reserve(usage.size());
  for (const auto& info : usage) {
    result.emplace_back(info->origin, info->total_size_bytes,
                        info->last_modified);
  }
  std::move(callback).Run(result);
}

}  // namespace

scoped_refptr<DOMStorageContextWrapper> DOMStorageContextWrapper::Create(
    StoragePartitionImpl* partition,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  auto wrapper = base::MakeRefCounted<DOMStorageContextWrapper>(partition);
  if (special_storage_policy) {
    wrapper->storage_policy_observer_.emplace(
        // `storage_policy_observer_` is owned by `wrapper` and so it is safe
        // to use base::Unretained here.
        base::BindRepeating(&DOMStorageContextWrapper::ApplyPolicyUpdates,
                            base::Unretained(wrapper.get())),
        base::CreateSequencedTaskRunner(BrowserThread::IO),
        std::move(special_storage_policy));
  }

  wrapper->local_storage_control_->GetUsage(base::BindOnce(
      &DOMStorageContextWrapper::OnStartupUsageRetrieved, wrapper));
  return wrapper;
}

DOMStorageContextWrapper::DOMStorageContextWrapper(
    StoragePartitionImpl* partition)
    : partition_(partition) {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&DOMStorageContextWrapper::OnMemoryPressure,
                          base::Unretained(this)));

  MaybeBindSessionStorageControl();
  MaybeBindLocalStorageControl();
}

DOMStorageContextWrapper::~DOMStorageContextWrapper() {
  DCHECK(!local_storage_control_)
      << "Shutdown should be called before destruction";
}

storage::mojom::SessionStorageControl*
DOMStorageContextWrapper::GetSessionStorageControl() {
  if (!session_storage_control_)
    return nullptr;
  return session_storage_control_.get();
}

storage::mojom::LocalStorageControl*
DOMStorageContextWrapper::GetLocalStorageControl() {
  DCHECK(local_storage_control_);
  return local_storage_control_.get();
}

void DOMStorageContextWrapper::GetLocalStorageUsage(
    GetLocalStorageUsageCallback callback) {
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run(std::vector<StorageUsageInfo>());
    return;
  }

  local_storage_control_->GetUsage(
      base::BindOnce(&AdaptStorageUsageInfo, std::move(callback)));
}

void DOMStorageContextWrapper::GetSessionStorageUsage(
    GetSessionStorageUsageCallback callback) {
  if (!session_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run(std::vector<SessionStorageUsageInfo>());
    return;
  }

  session_storage_control_->GetUsage(
      base::BindOnce(&AdaptSessionStorageUsageInfo, std::move(callback)));
}

void DOMStorageContextWrapper::DeleteLocalStorage(const url::Origin& origin,
                                                  base::OnceClosure callback) {
  DCHECK(callback);
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  local_storage_control_->DeleteStorage(origin, std::move(callback));
}

void DOMStorageContextWrapper::PerformLocalStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  local_storage_control_->CleanUpStorage(std::move(callback));
}

void DOMStorageContextWrapper::DeleteSessionStorage(
    const SessionStorageUsageInfo& usage_info,
    base::OnceClosure callback) {
  if (!session_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }
  session_storage_control_->DeleteStorage(
      url::Origin::Create(usage_info.origin), usage_info.namespace_id,
      std::move(callback));
}

void DOMStorageContextWrapper::PerformSessionStorageCleanup(
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!session_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  session_storage_control_->CleanUpStorage(std::move(callback));
}

scoped_refptr<SessionStorageNamespace>
DOMStorageContextWrapper::RecreateSessionStorage(
    const std::string& namespace_id) {
  return SessionStorageNamespaceImpl::Create(this, namespace_id);
}

void DOMStorageContextWrapper::StartScavengingUnusedSessionStorage() {
  if (!session_storage_control_) {
    // Shutdown() has been called.
    return;
  }

  session_storage_control_->ScavengeUnusedNamespaces(base::NullCallback());
}

void DOMStorageContextWrapper::SetForceKeepSessionState() {
  if (!local_storage_control_) {
    // Shutdown() has been called.
    return;
  }

  local_storage_control_->ForceKeepSessionState();
}

void DOMStorageContextWrapper::Shutdown() {
  // |partition_| is about to be destroyed, so we must not dereference it after
  // this call.
  partition_ = nullptr;

  // Signals the implementation to perform shutdown operations.
  session_storage_control_.reset();
  local_storage_control_.reset();
  memory_pressure_listener_.reset();

  // Make sure the observer drops its reference to |this|.
  storage_policy_observer_.reset();
}

void DOMStorageContextWrapper::Flush() {
  if (session_storage_control_)
    session_storage_control_->Flush(base::NullCallback());
  if (local_storage_control_)
    local_storage_control_->Flush(base::NullCallback());
}

void DOMStorageContextWrapper::OpenLocalStorage(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  DCHECK(local_storage_control_);
  local_storage_control_->BindStorageArea(origin, std::move(receiver));
  if (storage_policy_observer_)
    storage_policy_observer_->StartTrackingOrigin(origin);
}

void DOMStorageContextWrapper::BindNamespace(
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  DCHECK(session_storage_control_);
  session_storage_control_->BindNamespace(namespace_id, std::move(receiver),
                                          base::DoNothing());
}

void DOMStorageContextWrapper::BindStorageArea(
    ChildProcessSecurityPolicyImpl::Handle security_policy_handle,
    const url::Origin& origin,
    const std::string& namespace_id,
    mojo::ReportBadMessageCallback bad_message_callback,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  if (!security_policy_handle.CanAccessDataForOrigin(origin)) {
    std::move(bad_message_callback)
        .Run("Access denied for sessionStorage request");
    return;
  }

  DCHECK(session_storage_control_);
  session_storage_control_->BindStorageArea(
      origin, namespace_id, std::move(receiver), base::DoNothing());
}

void DOMStorageContextWrapper::RecoverFromStorageServiceCrash() {
  DCHECK(partition_);
  MaybeBindSessionStorageControl();
  MaybeBindLocalStorageControl();

  // Make sure the service is aware of namespaces we asked a previous instance
  // to create, so it can properly service renderers trying to manipulate those
  // namespaces.
  base::AutoLock lock(alive_namespaces_lock_);
  for (const auto& entry : alive_namespaces_)
    session_storage_control_->CreateNamespace(entry.first);
  session_storage_control_->ScavengeUnusedNamespaces(base::NullCallback());
}

void DOMStorageContextWrapper::MaybeBindSessionStorageControl() {
  if (!partition_)
    return;
  session_storage_control_.reset();
  partition_->GetStorageServicePartition()->BindSessionStorageControl(
      session_storage_control_.BindNewPipeAndPassReceiver());
}

void DOMStorageContextWrapper::MaybeBindLocalStorageControl() {
  if (!partition_)
    return;
  local_storage_control_.reset();
  partition_->GetStorageServicePartition()->BindLocalStorageControl(
      local_storage_control_.BindNewPipeAndPassReceiver());
}

scoped_refptr<SessionStorageNamespaceImpl>
DOMStorageContextWrapper::MaybeGetExistingNamespace(
    const std::string& namespace_id) const {
  base::AutoLock lock(alive_namespaces_lock_);
  auto it = alive_namespaces_.find(namespace_id);
  return (it != alive_namespaces_.end()) ? it->second : nullptr;
}

void DOMStorageContextWrapper::AddNamespace(
    const std::string& namespace_id,
    SessionStorageNamespaceImpl* session_namespace) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(alive_namespaces_.find(namespace_id) == alive_namespaces_.end());
  alive_namespaces_[namespace_id] = session_namespace;
}

void DOMStorageContextWrapper::RemoveNamespace(
    const std::string& namespace_id) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(alive_namespaces_.find(namespace_id) != alive_namespaces_.end());
  alive_namespaces_.erase(namespace_id);
}

void DOMStorageContextWrapper::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  PurgeOption purge_option = PURGE_UNOPENED;
  if (memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    purge_option = PURGE_AGGRESSIVE;
  }
  PurgeMemory(purge_option);
}

void DOMStorageContextWrapper::PurgeMemory(PurgeOption purge_option) {
  if (!local_storage_control_) {
    // Shutdown was called.
    return;
  }

  if (purge_option == PURGE_AGGRESSIVE) {
    DCHECK(session_storage_control_);
    session_storage_control_->PurgeMemory();
    local_storage_control_->PurgeMemory();
  }
}

void DOMStorageContextWrapper::OnStartupUsageRetrieved(
    std::vector<storage::mojom::StorageUsageInfoPtr> usage) {
  if (!storage_policy_observer_)
    return;

  std::vector<url::Origin> origins;
  for (const auto& info : usage)
    origins.emplace_back(std::move(info->origin));
  storage_policy_observer_->StartTrackingOrigins(std::move(origins));
}

void DOMStorageContextWrapper::ApplyPolicyUpdates(
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates) {
  if (!local_storage_control_)
    return;

  if (!policy_updates.empty())
    local_storage_control_->ApplyPolicyUpdates(std::move(policy_updates));
}

}  // namespace content

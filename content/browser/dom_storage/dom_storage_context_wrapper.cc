// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/services/storage/dom_storage/local_storage_impl.h"
#include "components/services/storage/dom_storage/session_storage_impl.h"
#include "components/services/storage/public/mojom/partition.mojom.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {
namespace {

void AdaptSessionStorageUsageInfo(
    DOMStorageContextWrapper::GetSessionStorageUsageCallback callback,
    std::vector<storage::mojom::SessionStorageUsageInfoPtr> usage) {
  std::vector<SessionStorageUsageInfo> result;
  result.reserve(usage.size());
  for (const auto& entry : usage) {
    SessionStorageUsageInfo info;
    info.storage_key = entry->storage_key;
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
    result.emplace_back(info->storage_key, info->total_size_bytes,
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
        GetIOThreadTaskRunner({}), std::move(special_storage_policy));
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

void DOMStorageContextWrapper::DeleteLocalStorage(
    const blink::StorageKey& storage_key,
    base::OnceClosure callback) {
  DCHECK(callback);
  if (!local_storage_control_) {
    // Shutdown() has been called.
    std::move(callback).Run();
    return;
  }

  local_storage_control_->DeleteStorage(storage_key, std::move(callback));
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
      usage_info.storage_key, usage_info.namespace_id, std::move(callback));
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
    session_storage_control_->Flush();
  if (local_storage_control_)
    local_storage_control_->Flush();
}

void DOMStorageContextWrapper::OpenLocalStorage(
    const blink::StorageKey& storage_key,
    std::optional<blink::LocalFrameToken> local_frame_token,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver,
    ChildProcessSecurityPolicyImpl::Handle security_policy_handle,
    mojo::ReportBadMessageCallback bad_message_callback) {
  if (!IsRequestValid(StorageType::kLocalStorage, storage_key,
                      local_frame_token, std::move(security_policy_handle),
                      std::move(bad_message_callback))) {
    return;
  }
  DCHECK(local_storage_control_);
  local_storage_control_->BindStorageArea(storage_key, std::move(receiver));
  if (storage_policy_observer_) {
    // TODO(crbug.com/40177656): Pass the real StorageKey when
    // StoragePolicyObserver is converted.
    storage_policy_observer_->StartTrackingOrigin(storage_key.origin());
  }
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
    const blink::StorageKey& storage_key,
    std::optional<blink::LocalFrameToken> local_frame_token,
    const std::string& namespace_id,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver,
    ChildProcessSecurityPolicyImpl::Handle security_policy_handle,
    mojo::ReportBadMessageCallback bad_message_callback) {
  if (!IsRequestValid(StorageType::kSessionStorage, storage_key,
                      local_frame_token, std::move(security_policy_handle),
                      std::move(bad_message_callback))) {
    return;
  }
  DCHECK(session_storage_control_);
  session_storage_control_->BindStorageArea(
      storage_key, namespace_id, std::move(receiver), base::DoNothing());
}

bool DOMStorageContextWrapper::IsRequestValid(
    const StorageType type,
    const blink::StorageKey& storage_key,
    std::optional<blink::LocalFrameToken> local_frame_token,
    ChildProcessSecurityPolicyImpl::Handle security_policy_handle,
    mojo::ReportBadMessageCallback bad_message_callback) {
  bool host_storage_key_did_not_match = false;
  if (local_frame_token) {
    RenderFrameHostImpl* host = RenderFrameHostImpl::FromFrameToken(
        security_policy_handle.child_id(), *local_frame_token,
        &bad_message_callback);
    if (!host) {
      return false;
    }
    host_storage_key_did_not_match = host->GetStorageKey() != storage_key;
    // If the storage keys did not match, but storage access has been granted
    // and the request was for a first-party storage key on the same origin as
    // the frame's storage key, we can allow the request to proceed. See:
    // third_party/blink/renderer/modules/storage_access/README.md
    if (host_storage_key_did_not_match) {
      auto* permission_controller =
          host->GetBrowserContext()->GetPermissionController();
      blink::mojom::PermissionStatus status =
          permission_controller->GetPermissionStatusForCurrentDocument(
              blink::PermissionType::STORAGE_ACCESS_GRANT, host);
      if (status == blink::mojom::PermissionStatus::GRANTED) {
        host_storage_key_did_not_match =
            blink::StorageKey::CreateFirstParty(
                host->GetStorageKey().origin()) != storage_key;
      }
    }
  }
  if (!security_policy_handle.CanAccessDataForOrigin(storage_key.origin())) {
    const std::string type_string =
        type == StorageType::kLocalStorage ? "localStorage" : "sessionStorage";
    SYSLOG(WARNING) << "Denying illegal " << type_string
                    << " request due to ChildProcessSecurityPolicy.";
    std::move(bad_message_callback)
        .Run(base::StrCat({"Access denied for ", type_string,
                           " request due to ChildProcessSecurityPolicy."}));
    return false;
  }
  if (host_storage_key_did_not_match) {
    // Ideally we would kill the renderer here, but it's possible this is the
    // result of a race condition between committing the new document and
    // binding the DOM Storage. For now, we'll just fail to bind.
    return false;
  }
  return true;
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
  return (it != alive_namespaces_.end()) ? it->second.get() : nullptr;
}

void DOMStorageContextWrapper::AddNamespace(
    const std::string& namespace_id,
    SessionStorageNamespaceImpl* session_namespace) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(!base::Contains(alive_namespaces_, namespace_id));
  alive_namespaces_[namespace_id] = session_namespace;
}

void DOMStorageContextWrapper::RemoveNamespace(
    const std::string& namespace_id) {
  base::AutoLock lock(alive_namespaces_lock_);
  DCHECK(base::Contains(alive_namespaces_, namespace_id));
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
  for (const auto& info : usage) {
    origins.emplace_back(std::move(info->storage_key.origin()));
  }
  // TODO(crbug.com/40177656): Pass the real StorageKey when
  // StoragePolicyObserver is converted.
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

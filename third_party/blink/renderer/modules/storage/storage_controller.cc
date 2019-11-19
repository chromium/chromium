// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_controller.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {
namespace {

const size_t kStorageControllerTotalCacheLimitInBytesLowEnd = 1 * 1024 * 1024;
const size_t kStorageControllerTotalCacheLimitInBytes = 5 * 1024 * 1024;

mojo::PendingRemote<mojom::blink::StoragePartitionService>
GetAndCreateStorageInterface() {
  mojo::PendingRemote<mojom::blink::StoragePartitionService> pending_remote;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}
}  // namespace

// static
StorageController* StorageController::GetInstance() {
  DEFINE_STATIC_LOCAL(StorageController, gCachedStorageAreaController,
                      (Thread::MainThread()->Scheduler()->IPCTaskRunner(),
                       GetAndCreateStorageInterface(),
                       base::SysInfo::IsLowEndDevice()
                           ? kStorageControllerTotalCacheLimitInBytesLowEnd
                           : kStorageControllerTotalCacheLimitInBytes));
  return &gCachedStorageAreaController;
}

// static
bool StorageController::CanAccessStorageArea(LocalFrame* frame,
                                             StorageArea::StorageType type) {
  if (auto* settings_client = frame->GetContentSettingsClient()) {
    return settings_client->AllowStorage(
        type == StorageArea::StorageType::kLocalStorage);
  }
  return true;
}

StorageController::StorageController(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_runner,
    mojo::PendingRemote<mojom::blink::StoragePartitionService>
        storage_partition_service,
    size_t total_cache_limit)
    : ipc_runner_(std::move(ipc_runner)),
      namespaces_(MakeGarbageCollected<
                  HeapHashMap<String, WeakMember<StorageNamespace>>>()),
      total_cache_limit_(total_cache_limit),
      storage_partition_service_(std::move(storage_partition_service)) {}

StorageNamespace* StorageController::CreateSessionStorageNamespace(
    const String& namespace_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There is an edge case where a user closes a tab that has other tabs in the
  // same process, then restores that tab. The old namespace might still be
  // around.
  auto it = namespaces_->find(namespace_id);
  if (it != namespaces_->end())
    return it->value;
  StorageNamespace* ns =
      MakeGarbageCollected<StorageNamespace>(this, namespace_id);
  namespaces_->insert(namespace_id, ns);
  return ns;
}

size_t StorageController::TotalCacheSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t total = 0;
  if (local_storage_namespace_)
    total = local_storage_namespace_->TotalCacheSize();
  for (const auto& pair : *namespaces_)
    total += pair.value->TotalCacheSize();
  return total;
}

void StorageController::ClearAreasIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (TotalCacheSize() < total_cache_limit_)
    return;
  if (local_storage_namespace_)
    local_storage_namespace_->CleanUpUnusedAreas();
  for (auto& pair : *namespaces_)
    pair.value->CleanUpUnusedAreas();
}

scoped_refptr<CachedStorageArea> StorageController::GetLocalStorageArea(
    const SecurityOrigin* origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureLocalStorageNamespaceCreated();
  return local_storage_namespace_->GetCachedArea(origin);
}

void StorageController::AddLocalStorageInspectorStorageAgent(
    InspectorDOMStorageAgent* agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureLocalStorageNamespaceCreated();
  local_storage_namespace_->AddInspectorStorageAgent(agent);
}

void StorageController::RemoveLocalStorageInspectorStorageAgent(
    InspectorDOMStorageAgent* agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureLocalStorageNamespaceCreated();
  local_storage_namespace_->RemoveInspectorStorageAgent(agent);
}

void StorageController::DidDispatchLocalStorageEvent(
    const SecurityOrigin* origin,
    const String& key,
    const String& old_value,
    const String& new_value) {
  if (local_storage_namespace_) {
    local_storage_namespace_->DidDispatchStorageEvent(origin, key, old_value,
                                                      new_value);
  }
}

void StorageController::EnsureLocalStorageNamespaceCreated() {
  if (local_storage_namespace_)
    return;
  local_storage_namespace_ = MakeGarbageCollected<StorageNamespace>(this);
}

}  // namespace blink

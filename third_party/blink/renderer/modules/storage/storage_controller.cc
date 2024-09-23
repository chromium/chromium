// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_controller.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

const size_t kStorageControllerTotalCacheLimitInBytesLowEnd = 1 * 1024 * 1024;
const size_t kStorageControllerTotalCacheLimitInBytes = 5 * 1024 * 1024;

StorageController::DomStorageConnection GetDomStorageConnection() {
  StorageController::DomStorageConnection connection;
  mojo::Remote<mojom::blink::DomStorageProvider> provider;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      provider.BindNewPipeAndPassReceiver());
  mojo::PendingRemote<mojom::blink::DomStorageClient> client;
  connection.client_receiver = client.InitWithNewPipeAndPassReceiver();
  provider->BindDomStorage(
      connection.dom_storage_remote.BindNewPipeAndPassReceiver(),
      std::move(client));
  return connection;
}

}  // namespace

// static
StorageController* StorageController::GetInstance() {
  DEFINE_STATIC_LOCAL(StorageController, gCachedStorageAreaController,
                      (GetDomStorageConnection(),
                       base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()
                           ? kStorageControllerTotalCacheLimitInBytesLowEnd
                           : kStorageControllerTotalCacheLimitInBytes));
  return &gCachedStorageAreaController;
}

// static
bool StorageController::CanAccessStorageArea(LocalFrame* frame,
                                             StorageArea::StorageType type) {
  switch (type) {
    case StorageArea::StorageType::kLocalStorage:
      return frame->AllowStorageAccessSyncAndNotify(
          WebContentSettingsClient::StorageType::kLocalStorage);
    case StorageArea::StorageType::kSessionStorage:
      return frame->AllowStorageAccessSyncAndNotify(
          WebContentSettingsClient::StorageType::kSessionStorage);
  }
  return true;
}

StorageController::StorageController(DomStorageConnection connection,
                                     size_t total_cache_limit)
    : namespaces_(MakeGarbageCollected<
                  HeapHashMap<String, WeakMember<StorageNamespace>>>()),
      total_cache_limit_(total_cache_limit),
      dom_storage_remote_(std::move(connection.dom_storage_remote)) {
  // May be null in tests.
  if (connection.client_receiver)
    dom_storage_client_receiver_.Bind(std::move(connection.client_receiver));
}

StorageNamespace* StorageController::CreateSessionStorageNamespace(
    Page& page,
    const String& namespace_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There is an edge case where a user closes a tab that has other tabs in the
  // same process, then restores that tab. The old namespace might still be
  // around.
  auto it = namespaces_->find(namespace_id);
  if (it != namespaces_->end())
    return it->value.Get();
  StorageNamespace* ns =
      MakeGarbageCollected<StorageNamespace>(page, this, namespace_id);
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
    LocalDOMWindow* local_dom_window,
    mojo::PendingRemote<mojom::blink::StorageArea> local_storage_area,
    StorageNamespace::StorageContext context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureLocalStorageNamespaceCreated();
  return local_storage_namespace_->GetCachedArea(
      local_dom_window, std::move(local_storage_area), context);
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

void StorageController::EnsureLocalStorageNamespaceCreated() {
  if (local_storage_namespace_)
    return;
  local_storage_namespace_ = MakeGarbageCollected<StorageNamespace>(this);
}

void StorageController::ResetStorageAreaAndNamespaceConnections() {
  for (auto& ns : *namespaces_) {
    if (ns.value)
      ns.value->ResetStorageAreaAndNamespaceConnections();
  }
  if (local_storage_namespace_)
    local_storage_namespace_->ResetStorageAreaAndNamespaceConnections();
}

}  // namespace blink

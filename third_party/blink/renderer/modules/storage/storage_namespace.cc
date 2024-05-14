/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/storage/storage_namespace.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

const char StorageNamespace::kSupplementName[] = "SessionStorageNamespace";

StorageNamespace::StorageNamespace(StorageController* controller)
    : Supplement(nullptr), controller_(controller) {}
StorageNamespace::StorageNamespace(Page& page,
                                   StorageController* controller,
                                   const String& namespace_id)
    : Supplement(nullptr),
      controller_(controller),
      namespace_id_(namespace_id),
      task_runner_(page.GetAgentGroupScheduler().DefaultTaskRunner()) {}

// static
void StorageNamespace::ProvideSessionStorageNamespaceTo(
    Page& page,
    const SessionStorageNamespaceId& namespace_id) {
  if (namespace_id.empty())
    return;
  auto* ss_namespace =
      StorageController::GetInstance()->CreateSessionStorageNamespace(
          page, String(namespace_id.data(), namespace_id.length()));
  if (!ss_namespace)
    return;
  ProvideTo(page, ss_namespace);
}

scoped_refptr<CachedStorageArea> StorageNamespace::GetCachedArea(
    LocalDOMWindow* local_dom_window,
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area,
    StorageContext context) {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheMetrics {
    kMiss = 0,    // Area not in cache.
    kHit = 1,     // Area with refcount = 0 loaded from cache.
    kUnused = 2,  // Cache was not used. Area had refcount > 0.
    kMaxValue = kUnused,
  };

  CacheMetrics metric = CacheMetrics::kMiss;
  scoped_refptr<CachedStorageArea> result;
  BlinkStorageKey storage_key = local_dom_window->GetStorageKey();
  // The Storage Access API needs to use the first-party version of the storage
  // key. For more see:
  // third_party/blink/renderer/modules/storage_access/README.md
  if (context == StorageContext::kStorageAccessAPI) {
    storage_key =
        BlinkStorageKey::CreateFirstParty(storage_key.GetSecurityOrigin());
  }
  auto cache_it = cached_areas_.find(&storage_key);
  if (cache_it != cached_areas_.end()) {
    metric = cache_it->value->HasOneRef() ? CacheMetrics::kHit
                                          : CacheMetrics::kUnused;
    result = cache_it->value;
  }
  if (IsSessionStorage()) {
    base::UmaHistogramEnumeration("Storage.SessionStorage.RendererAreaCacheHit",
                                  metric);
  } else {
    base::UmaHistogramEnumeration("LocalStorage.RendererAreaCacheHit", metric);
  }

  if (result)
    return result;

  controller_->ClearAreasIfNeeded();
  result = base::MakeRefCounted<CachedStorageArea>(
      IsSessionStorage() ? CachedStorageArea::AreaType::kSessionStorage
                         : CachedStorageArea::AreaType::kLocalStorage,
      storage_key, local_dom_window, this,
      /*is_session_storage_for_prerendering=*/false, std::move(storage_area));
  cached_areas_.insert(std::make_unique<const BlinkStorageKey>(storage_key),
                       result);
  return result;
}

scoped_refptr<CachedStorageArea> StorageNamespace::CreateCachedAreaForPrerender(
    LocalDOMWindow* local_dom_window,
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area) {
  DCHECK((IsSessionStorage()));
  return base::MakeRefCounted<CachedStorageArea>(
      IsSessionStorage() ? CachedStorageArea::AreaType::kSessionStorage
                         : CachedStorageArea::AreaType::kLocalStorage,
      local_dom_window->GetStorageKey(), local_dom_window, this,
      /*is_session_storage_for_prerendering=*/true, std::move(storage_area));
}

void StorageNamespace::EvictSessionStorageCachedData() {
  // Currently this is called to evict the cached data only when prerendering
  // was triggered. TODO(crbug.com/1215680): investigate if more cache eviction
  // is needed for non-prerender use cases.
  DCHECK(IsSessionStorage());
  for (auto& entry : cached_areas_) {
    entry.value->EvictCachedData();
  }
}

void StorageNamespace::CloneTo(const String& target) {
  DCHECK(IsSessionStorage()) << "Cannot clone a local storage namespace.";
  EnsureConnected();

  // Spec requires that all mutations on storage areas *before* cloning are
  // visible in the clone and that no mutations on the original storage areas
  // *after* cloning, are visible in the clone. Consider the following scenario
  // in the comments below:
  //
  //   1. Area A calls Put("x", 42)
  //   2. Area B calls Put("y", 13)
  //   3. Area A & B's StorageNamespace gets CloneTo()'d to a new namespace
  //   4. Area A calls Put("x", 43) in the original namespace
  //
  // First, we synchronize StorageNamespace against every cached StorageArea.
  // This ensures that all StorageArea operations (e.g. Put, Delete) up to this
  // point will have executed before the StorageNamespace implementation is able
  // to receive or process the following `Clone()` call. Given the above
  // example, this would mean that A.x=42 and B.y=13 definitely WILL be present
  // in the cloned namespace.
  for (auto& entry : cached_areas_) {
    namespace_.PauseReceiverUntilFlushCompletes(
        entry.value->RemoteArea().FlushAsync());
  }

  namespace_->Clone(target);

  // Finally, we synchronize every StorageArea against StorageNamespace. This
  // ensures that any future calls on each StorageArea cannot be received and
  // processed until after the above `Clone()` call executes.  Given the example
  // above, this would mean that A.x=43 definitely WILL NOT be present in the
  // cloned namespace; only the original namespace will be updated, and A.x will
  // still hold a value of 42 in the new clone.
  for (auto& entry : cached_areas_) {
    entry.value->RemoteArea().PauseReceiverUntilFlushCompletes(
        namespace_.FlushAsync());
  }
}

size_t StorageNamespace::TotalCacheSize() const {
  size_t total = 0;
  for (const auto& it : cached_areas_)
    total += it.value->quota_used();
  return total;
}

void StorageNamespace::CleanUpUnusedAreas() {
  Vector<const BlinkStorageKey*, 16> to_remove;
  for (const auto& area : cached_areas_) {
    if (area.value->HasOneRef())
      to_remove.push_back(area.key.get());
  }
  cached_areas_.RemoveAll(to_remove);
}

void StorageNamespace::AddInspectorStorageAgent(
    InspectorDOMStorageAgent* agent) {
  inspector_agents_.insert(agent);
}
void StorageNamespace::RemoveInspectorStorageAgent(
    InspectorDOMStorageAgent* agent) {
  inspector_agents_.erase(agent);
}

void StorageNamespace::Trace(Visitor* visitor) const {
  visitor->Trace(inspector_agents_);
  visitor->Trace(namespace_);
  Supplement<Page>::Trace(visitor);
}

void StorageNamespace::DidDispatchStorageEvent(
    const BlinkStorageKey& storage_key,
    const String& key,
    const String& old_value,
    const String& new_value) {
  for (InspectorDOMStorageAgent* agent : inspector_agents_) {
    agent->DidDispatchDOMStorageEvent(
        key, old_value, new_value,
        IsSessionStorage() ? StorageArea::StorageType::kSessionStorage
                           : StorageArea::StorageType::kLocalStorage,
        storage_key);
  }
}

void StorageNamespace::BindStorageArea(
    const BlinkStorageKey& storage_key,
    const LocalFrameToken& local_frame_token,
    mojo::PendingReceiver<mojom::blink::StorageArea> receiver) {
  if (IsSessionStorage()) {
    controller_->dom_storage()->BindSessionStorageArea(
        storage_key, local_frame_token, namespace_id_, std::move(receiver));
  } else {
    controller_->dom_storage()->OpenLocalStorage(storage_key, local_frame_token,
                                                 std::move(receiver));
  }
}

void StorageNamespace::ResetStorageAreaAndNamespaceConnections() {
  for (const auto& area : cached_areas_)
    area.value->ResetConnection();
  namespace_.reset();
}

void StorageNamespace::EnsureConnected() {
  DCHECK(IsSessionStorage());
  if (namespace_.is_bound())
    return;
  controller_->dom_storage()->BindSessionStorageNamespace(
      namespace_id_, namespace_.BindNewPipeAndPassReceiver(task_runner_));
}

}  // namespace blink

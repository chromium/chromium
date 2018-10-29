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
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_storage_area.h"
#include "third_party/blink/public/platform/web_storage_namespace.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"
#include "third_party/blink/renderer/modules/storage/inspector_dom_storage_agent.h"
#include "third_party/blink/renderer/modules/storage/storage_area.h"
#include "third_party/blink/renderer/modules/storage/storage_controller.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

const char StorageNamespace::kSupplementName[] = "SessionStorageNamespace";

StorageNamespace::StorageNamespace(StorageController* controller)
    : controller_(controller) {
  CHECK(base::FeatureList::IsEnabled(features::kOnionSoupDOMStorage));
}
StorageNamespace::StorageNamespace(StorageController* controller,
                                   const String& namespace_id)
    : controller_(controller), namespace_id_(namespace_id) {
  CHECK(base::FeatureList::IsEnabled(features::kOnionSoupDOMStorage));
}

StorageNamespace::StorageNamespace(
    std::unique_ptr<WebStorageNamespace> web_storage_namespace)
    : controller_(nullptr),
      namespace_id_(web_storage_namespace->GetNamespaceId()),
      web_storage_namespace_(std::move(web_storage_namespace)) {
  CHECK(!base::FeatureList::IsEnabled(features::kOnionSoupDOMStorage));
}

StorageNamespace::~StorageNamespace() = default;

// static
void StorageNamespace::ProvideSessionStorageNamespaceTo(Page& page,
                                                        WebViewClient* client) {
  if (client) {
    if (client->GetSessionStorageNamespaceId().empty())
      return;
    auto* ss_namespace =
        StorageController::GetInstance()->CreateSessionStorageNamespace(
            String(client->GetSessionStorageNamespaceId().data(),
                   client->GetSessionStorageNamespaceId().size()));
    if (!ss_namespace)
      return;
    ProvideTo(page, ss_namespace);
  }
}

scoped_refptr<CachedStorageArea> StorageNamespace::GetCachedArea(
    const SecurityOrigin* origin_ptr) {
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
  auto cache_it = cached_areas_.find(origin_ptr);
  if (cache_it != cached_areas_.end()) {
    metric = cache_it->value->HasOneRef() ? CacheMetrics::kHit
                                          : CacheMetrics::kUnused;
    result = cache_it->value;
  }
  if (IsSessionStorage())
    LOCAL_HISTOGRAM_ENUMERATION("SessionStorage.RendererAreaCacheHit", metric);
  else
    UMA_HISTOGRAM_ENUMERATION("LocalStorage.RendererAreaCacheHit", metric);

  if (result)
    return result;

  scoped_refptr<const SecurityOrigin> origin(origin_ptr);

  controller_->ClearAreasIfNeeded();
  if (IsSessionStorage()) {
    EnsureConnected();
    mojom::blink::StorageAreaAssociatedPtr area_ptr;
    namespace_->OpenArea(origin,
                         MakeRequest(&area_ptr, controller_->IPCTaskRunner()));
    result = CachedStorageArea::CreateForSessionStorage(
        origin, std::move(area_ptr), controller_->IPCTaskRunner(), this);
  } else {
    mojom::blink::StorageAreaPtr area_ptr;
    controller_->storage_partition_service()->OpenLocalStorage(
        origin, MakeRequest(&area_ptr, controller_->IPCTaskRunner()));
    result = CachedStorageArea::CreateForLocalStorage(
        origin, std::move(area_ptr), controller_->IPCTaskRunner(), this);
  }
  cached_areas_.insert(std::move(origin), result);
  return result;
}

void StorageNamespace::CloneTo(const String& target) {
  CHECK(base::FeatureList::IsEnabled(features::kOnionSoupDOMStorage));
  DCHECK(IsSessionStorage()) << "Cannot clone a local storage namespace.";
  EnsureConnected();
  namespace_->Clone(target);
}

size_t StorageNamespace::TotalCacheSize() const {
  size_t total = 0;
  for (const auto& it : cached_areas_)
    total += it.value->memory_used();
  return total;
}

void StorageNamespace::CleanUpUnusedAreas() {
  Vector<const SecurityOrigin*, 16> to_remove;
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

void StorageNamespace::Trace(Visitor* visitor) {
  visitor->Trace(inspector_agents_);
  Supplement<Page>::Trace(visitor);
}

void StorageNamespace::DidDispatchStorageEvent(const SecurityOrigin* origin,
                                               const String& key,
                                               const String& old_value,
                                               const String& new_value) {
  for (InspectorDOMStorageAgent* agent : inspector_agents_) {
    agent->DidDispatchDOMStorageEvent(
        key, old_value, new_value,
        IsSessionStorage() ? StorageArea::StorageType::kSessionStorage
                           : StorageArea::StorageType::kLocalStorage,
        origin);
  }
}
std::unique_ptr<WebStorageArea> StorageNamespace::GetWebStorageArea(
    const SecurityOrigin* origin) {
  CHECK(!base::FeatureList::IsEnabled(features::kOnionSoupDOMStorage));
  return base::WrapUnique(
      web_storage_namespace_->CreateStorageArea(WebSecurityOrigin(origin)));
}

void StorageNamespace::EnsureConnected() {
  DCHECK(IsSessionStorage());
  if (namespace_)
    return;
  auto request = MakeRequest(&namespace_, controller_->IPCTaskRunner());
  controller_->storage_partition_service()->OpenSessionStorage(
      namespace_id_, std::move(request));
}

}  // namespace blink

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_frontend.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "components/value_store/value_store_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/storage.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<StorageFrontend>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

events::HistogramValue StorageAreaToEventHistogram(
    StorageAreaNamespace storage_area) {
  switch (storage_area) {
    case StorageAreaNamespace::kLocal:
      return events::STORAGE_LOCAL_ON_CHANGE;
    case StorageAreaNamespace::kSync:
      return events::STORAGE_SYNC_ON_CHANGE;
    case StorageAreaNamespace::kManaged:
      return events::STORAGE_MANAGED_ON_CHANGE;
    case StorageAreaNamespace::kSession:
      return events::STORAGE_SESSION_ON_CHANGE;
    case StorageAreaNamespace::kInvalid:
      NOTREACHED();
      return events::UNKNOWN;
  }
}

}  // namespace

// static
StorageFrontend* StorageFrontend::Get(BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<StorageFrontend>::Get(context);
}

// static
std::unique_ptr<StorageFrontend> StorageFrontend::CreateForTesting(
    scoped_refptr<value_store::ValueStoreFactory> storage_factory,
    BrowserContext* context) {
  return base::WrapUnique(
      new StorageFrontend(std::move(storage_factory), context));
}

StorageFrontend::StorageFrontend(BrowserContext* context)
    : StorageFrontend(ExtensionSystem::Get(context)->store_factory(), context) {
}

StorageFrontend::StorageFrontend(
    scoped_refptr<value_store::ValueStoreFactory> factory,
    BrowserContext* context)
    : browser_context_(context) {
  Init(std::move(factory));
}

void StorageFrontend::Init(
    scoped_refptr<value_store::ValueStoreFactory> factory) {
  TRACE_EVENT0("browser,startup", "StorageFrontend::Init");

  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!browser_context_->IsOffTheRecord());

  caches_[settings_namespace::LOCAL] = new LocalValueStoreCache(factory);

  // Add any additional caches the embedder supports (for example, caches
  // for chrome.storage.managed and chrome.storage.sync).
  ExtensionsAPIClient::Get()->AddAdditionalValueStoreCaches(
      browser_context_, factory, GetObserver(), &caches_);
}

StorageFrontend::~StorageFrontend() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto it = caches_.begin(); it != caches_.end(); ++it) {
    ValueStoreCache* cache = it->second;
    cache->ShutdownOnUI();
    GetBackendTaskRunner()->DeleteSoon(FROM_HERE, cache);
  }
}

ValueStoreCache* StorageFrontend::GetValueStoreCache(
    settings_namespace::Namespace settings_namespace) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = caches_.find(settings_namespace);
  if (it != caches_.end())
    return it->second;
  return nullptr;
}

bool StorageFrontend::IsStorageEnabled(
    settings_namespace::Namespace settings_namespace) const {
  return caches_.find(settings_namespace) != caches_.end();
}

void StorageFrontend::RunWithStorage(
    scoped_refptr<const Extension> extension,
    settings_namespace::Namespace settings_namespace,
    ValueStoreCache::StorageCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(extension.get());

  ValueStoreCache* cache = caches_[settings_namespace];
  CHECK(cache);

  GetBackendTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(cache), std::move(callback), extension));
}

void StorageFrontend::DeleteStorageSoon(const std::string& extension_id,
                                        base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto subtask_done_callback =
      base::BarrierClosure(caches_.size(), std::move(done_callback));
  for (auto& cache_map : caches_) {
    ValueStoreCache* cache = cache_map.second;
    GetBackendTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&ValueStoreCache::DeleteStorageSoon,
                       base::Unretained(cache), extension_id),
        subtask_done_callback);
  }
}

SettingsChangedCallback StorageFrontend::GetObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return base::BindRepeating(&StorageFrontend::OnSettingsChanged,
                             weak_factory_.GetWeakPtr());
}

void StorageFrontend::DisableStorageForTesting(
    settings_namespace::Namespace settings_namespace) {
  auto it = caches_.find(settings_namespace);
  if (it != caches_.end()) {
    ValueStoreCache* cache = it->second;
    cache->ShutdownOnUI();
    GetBackendTaskRunner()->DeleteSoon(FROM_HERE, cache);
    caches_.erase(it);
  }
}

// Forwards changes on to the extension processes for |browser_context_| and its
// incognito partner if it exists.
void StorageFrontend::OnSettingsChanged(const std::string& extension_id,
                                        StorageAreaNamespace storage_area,
                                        base::Value changes) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("browser", "SettingsObserver:OnSettingsChanged", "extension_id",
               extension_id);

  // Alias extension_id for investigation of shutdown hangs. crbug.com/1154997
  // Extension IDs are exactly 32 characters in length.
  constexpr size_t kExtensionsIdLength = 32;
  char extension_id_str[kExtensionsIdLength + 1];
  base::strlcpy(extension_id_str, extension_id.c_str(),
                std::size(extension_id_str));
  base::debug::Alias(extension_id_str);

  const std::string namespace_string = StorageAreaToString(storage_area);
  EventRouter* event_router = EventRouter::Get(browser_context_);

  bool has_event_changed_listener = event_router->ExtensionHasEventListener(
      extension_id, api::storage::OnChanged::kEventName);

  // Event for StorageArea.
  auto area_event_name =
      base::StringPrintf("storage.%s.onChanged", namespace_string.c_str());
  bool has_area_changed_event_listener =
      event_router->ExtensionHasEventListener(extension_id, area_event_name);

  auto make_changed_event = [&namespace_string](base::Value changes) {
    base::Value::List args;
    args.Append(std::move(changes));
    args.Append(namespace_string);
    return std::make_unique<Event>(events::STORAGE_ON_CHANGED,
                                   api::storage::OnChanged::kEventName,
                                   std::move(args));
  };
  auto make_area_changed_event = [&storage_area,
                                  &area_event_name](base::Value changes) {
    base::Value::List args;
    args.Append(std::move(changes));
    return std::make_unique<Event>(StorageAreaToEventHistogram(storage_area),
                                   area_event_name, std::move(args));
  };
  // We only dispatch the events if there's a valid listener (even though
  // EventRouter would handle the no-listener case) since copying `changes`
  // can be expensive.
  // Event for each storage(sync, local, managed).
  if (has_event_changed_listener && has_area_changed_event_listener) {
    event_router->DispatchEventToExtension(
        extension_id, make_area_changed_event(changes.Clone()));
  }
  if (has_event_changed_listener) {
    event_router->DispatchEventToExtension(
        extension_id, make_changed_event(std::move(changes)));
  } else if (has_area_changed_event_listener) {
    event_router->DispatchEventToExtension(
        extension_id, make_area_changed_event(std::move(changes)));
  }
}

// BrowserContextKeyedAPI implementation.

// static
BrowserContextKeyedAPIFactory<StorageFrontend>*
StorageFrontend::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
const char* StorageFrontend::service_name() { return "StorageFrontend"; }

}  // namespace extensions

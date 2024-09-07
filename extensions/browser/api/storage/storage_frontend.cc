// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/storage_frontend.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
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
#include "extensions/browser/api/storage/storage_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/storage.h"
#include "extensions/common/extension_id.h"

using content::BrowserContext;
using content::BrowserThread;
using value_store::ValueStore;

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
      NOTREACHED_IN_MIGRATION();
      return events::UNKNOWN;
  }
}

void GetWithValueStore(
    std::optional<std::vector<std::string>> keys,
    base::OnceCallback<void(ValueStore::ReadResult)> callback,
    ValueStore* store) {
  ValueStore::ReadResult result =
      keys.has_value() ? store->Get(keys.value()) : store->Get();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void GetBytesInUseWithValueStore(std::optional<std::vector<std::string>> keys,
                                 base::OnceCallback<void(size_t)> callback,
                                 ValueStore* store) {
  size_t size = keys.has_value() ? store->GetBytesInUse(keys.value())
                                 : store->GetBytesInUse();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), size));
}

void SetWithValueStore(
    const base::Value::Dict& values,
    base::OnceCallback<void(ValueStore::WriteResult)> callback,
    ValueStore* store) {
  ValueStore::WriteResult result = store->Set(ValueStore::DEFAULTS, values);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void RemoveWithValueStore(
    std::vector<std::string> keys,
    base::OnceCallback<void(ValueStore::WriteResult)> callback,
    ValueStore* store) {
  ValueStore::WriteResult result = store->Remove(keys);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void ClearWithValueStore(
    base::OnceCallback<void(ValueStore::WriteResult)> callback,
    ValueStore* store) {
  ValueStore::WriteResult result = store->Clear();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

base::Value::List KeysFromDict(base::Value::Dict dict) {
  base::Value::List list = base::Value::List::with_capacity(dict.size());
  for (auto item : dict) {
    list.Append(std::move(item.first));
  }
  return list;
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

// Implementation of ResultStatus.

StorageFrontend::ResultStatus::ResultStatus() = default;

StorageFrontend::ResultStatus::ResultStatus(const ResultStatus&) = default;

StorageFrontend::ResultStatus::~ResultStatus() = default;

// Implementation of GetKeysResult.

StorageFrontend::GetKeysResult::GetKeysResult() = default;

StorageFrontend::GetKeysResult::GetKeysResult(GetKeysResult&& other) = default;

StorageFrontend::GetKeysResult::~GetKeysResult() = default;

// Implementation of GetResult.

StorageFrontend::GetResult::GetResult() = default;

StorageFrontend::GetResult::GetResult(GetResult&& other) = default;

StorageFrontend::GetResult::~GetResult() = default;

// Implementation of StorageFrontend.

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

void StorageFrontend::OnReadFinished(
    const ExtensionId& extension_id,
    StorageAreaNamespace storage_area,
    base::OnceCallback<void(StorageFrontend::GetResult)> callback,
    ValueStore::ReadResult result) {
  bool success = result.status().ok();

  GetResult get_result;

  get_result.status.success = success;
  get_result.status.error =
      success ? std::nullopt : std::optional(result.status().message);

  if (success) {
    get_result.data = result.PassSettings();
  }

  std::move(callback).Run(std::move(get_result));
}

void StorageFrontend::OnReadKeysFinished(
    base::OnceCallback<void(GetKeysResult)> callback,
    GetResult get_result) {
  GetKeysResult get_keys_result;

  get_keys_result.status = get_result.status;
  get_keys_result.data =
      get_result.status.success
          ? std::optional(KeysFromDict(std::move(*get_result.data)))
          : std::nullopt;

  std::move(callback).Run(std::move(get_keys_result));
}

void StorageFrontend::OnWriteFinished(
    const ExtensionId& extension_id,
    StorageAreaNamespace storage_area,
    base::OnceCallback<void(StorageFrontend::ResultStatus)> callback,
    ValueStore::WriteResult result) {
  bool success = result.status().ok();

  if (success && !result.changes().empty()) {
    OnSettingsChanged(
        extension_id, storage_area, std::nullopt,
        value_store::ValueStoreChange::ToValue(result.PassChanges()));
  }

  ResultStatus status;
  status.success = success;
  status.error =
      success ? std::nullopt : std::optional(result.status().message);
  std::move(callback).Run(status);
}

void StorageFrontend::GetValues(scoped_refptr<const Extension> extension,
                                StorageAreaNamespace storage_area,
                                std::optional<std::vector<std::string>> keys,
                                base::OnceCallback<void(GetResult)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (storage_area == StorageAreaNamespace::kSession) {
    SessionStorageManager* storage_manager =
        SessionStorageManager::GetForBrowserContext(browser_context_);

    std::map<std::string, const base::Value*> result =
        keys.has_value() ? storage_manager->Get(extension->id(), keys.value())
                         : storage_manager->GetAll(extension->id());

    GetResult get_result;
    get_result.data = base::Value::Dict();

    for (auto item : result) {
      get_result.data->Set(std::move(item.first), item.second->Clone());
    }

    // Using a task here is important since we want to consistently fire the
    // callback asynchronously.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(get_result)));
    return;
  }

  settings_namespace::Namespace settings_namespace =
      StorageAreaToSettingsNamespace(storage_area);

  CHECK(StorageFrontend::IsStorageEnabled(settings_namespace));

  RunWithStorage(
      extension, settings_namespace,
      base::BindOnce(&GetWithValueStore, std::move(keys),
                     base::BindOnce(&StorageFrontend::OnReadFinished,
                                    weak_factory_.GetWeakPtr(), extension->id(),
                                    storage_area, std::move(callback))));
}

void StorageFrontend::GetKeys(
    scoped_refptr<const Extension> extension,
    StorageAreaNamespace storage_area,
    base::OnceCallback<void(GetKeysResult)> callback) {
  GetValues(extension, storage_area, std::nullopt,
            base::BindOnce(&StorageFrontend::OnReadKeysFinished,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
}

void StorageFrontend::GetBytesInUse(
    scoped_refptr<const Extension> extension,
    StorageAreaNamespace storage_area,
    std::optional<std::vector<std::string>> keys,
    base::OnceCallback<void(size_t)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (storage_area == StorageAreaNamespace::kSession) {
    SessionStorageManager* storage_manager =
        SessionStorageManager::GetForBrowserContext(browser_context_);

    size_t bytes_in_use =
        keys.has_value()
            ? storage_manager->GetBytesInUse(extension->id(), keys.value())
            : storage_manager->GetTotalBytesInUse(extension->id());

    // Using a task here is important since we want to consistently fire the
    // callback asynchronously.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), bytes_in_use));
    return;
  }

  extensions::settings_namespace::Namespace settings_namespace =
      extensions::StorageAreaToSettingsNamespace(storage_area);

  CHECK(StorageFrontend::IsStorageEnabled(settings_namespace));

  RunWithStorage(extension, settings_namespace,
                 base::BindOnce(&GetBytesInUseWithValueStore, std::move(keys),
                                std::move(callback)));
}

void StorageFrontend::Set(scoped_refptr<const Extension> extension,
                          StorageAreaNamespace storage_area,
                          base::Value::Dict values,
                          base::OnceCallback<void(ResultStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (storage_area == StorageAreaNamespace::kSession) {
    SessionStorageManager* storage_manager =
        SessionStorageManager::GetForBrowserContext(browser_context_);

    std::map<std::string, base::Value> values_map;
    for (auto item : values) {
      values_map.emplace(std::move(item.first), std::move(item.second));
    }

    std::vector<SessionStorageManager::ValueChange> changes;
    std::string error;
    bool success = storage_manager->Set(extension->id(), std::move(values_map),
                                        changes, &error);

    if (success && !changes.empty()) {
      OnSettingsChanged(extension->id(), storage_area,
                        storage_utils::GetSessionAccessLevel(extension->id(),
                                                             *browser_context_),
                        storage_utils::ValueChangeToValue(std::move(changes)));
    }

    ResultStatus status;
    status.success = success;
    status.error = success ? std::nullopt : std::optional(error);

    // Using a task here is important since we want to consistently fire the
    // callback asynchronously.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), status));
    return;
  }

  settings_namespace::Namespace settings_namespace =
      StorageAreaToSettingsNamespace(storage_area);

  CHECK(StorageFrontend::IsStorageEnabled(settings_namespace));

  RunWithStorage(
      extension, settings_namespace,
      base::BindOnce(&SetWithValueStore, std::move(values),
                     base::BindOnce(&StorageFrontend::OnWriteFinished,
                                    weak_factory_.GetWeakPtr(), extension->id(),
                                    storage_area, std::move(callback))));
}

void StorageFrontend::Remove(scoped_refptr<const Extension> extension,
                             StorageAreaNamespace storage_area,
                             const std::vector<std::string>& keys,
                             base::OnceCallback<void(ResultStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (storage_area == StorageAreaNamespace::kSession) {
    SessionStorageManager* storage_manager =
        SessionStorageManager::GetForBrowserContext(browser_context_);

    std::vector<SessionStorageManager::ValueChange> changes;
    storage_manager->Remove(extension->id(), keys, changes);

    if (!changes.empty()) {
      OnSettingsChanged(extension->id(), storage_area,
                        storage_utils::GetSessionAccessLevel(extension->id(),
                                                             *browser_context_),
                        storage_utils::ValueChangeToValue(std::move(changes)));
    }

    // Using a task here is important since we want to consistently fire the
    // callback asynchronously.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageFrontend::ResultStatus()));
    return;
  }

  settings_namespace::Namespace settings_namespace =
      StorageAreaToSettingsNamespace(storage_area);

  CHECK(StorageFrontend::IsStorageEnabled(settings_namespace));

  RunWithStorage(
      extension, settings_namespace,
      base::BindOnce(&RemoveWithValueStore, keys,
                     base::BindOnce(&StorageFrontend::OnWriteFinished,
                                    weak_factory_.GetWeakPtr(), extension->id(),
                                    storage_area, std::move(callback))));
}

void StorageFrontend::Clear(
    scoped_refptr<const Extension> extension,
    StorageAreaNamespace storage_area,
    base::OnceCallback<void(StorageFrontend::ResultStatus)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (storage_area == StorageAreaNamespace::kSession) {
    SessionStorageManager* storage_manager =
        SessionStorageManager::GetForBrowserContext(browser_context_);

    std::vector<SessionStorageManager::ValueChange> changes;
    storage_manager->Clear(extension->id(), changes);

    if (!changes.empty()) {
      OnSettingsChanged(extension->id(), storage_area,
                        storage_utils::GetSessionAccessLevel(extension->id(),
                                                             *browser_context_),
                        storage_utils::ValueChangeToValue(std::move(changes)));
    }

    // Using a task here is important since we want to consistently fire the
    // callback asynchronously.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), StorageFrontend::ResultStatus()));
    return;
  }

  settings_namespace::Namespace settings_namespace =
      StorageAreaToSettingsNamespace(storage_area);

  CHECK(StorageFrontend::IsStorageEnabled(settings_namespace));

  RunWithStorage(
      extension, settings_namespace,
      base::BindOnce(&ClearWithValueStore,
                     base::BindOnce(&StorageFrontend::OnWriteFinished,
                                    weak_factory_.GetWeakPtr(), extension->id(),
                                    storage_area, std::move(callback))));
}

ValueStoreCache* StorageFrontend::GetValueStoreCache(
    settings_namespace::Namespace settings_namespace) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = caches_.find(settings_namespace);
  if (it != caches_.end()) {
    return it->second;
  }
  return nullptr;
}

bool StorageFrontend::IsStorageEnabled(
    settings_namespace::Namespace settings_namespace) const {
  return base::Contains(caches_, settings_namespace);
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

void StorageFrontend::DeleteStorageSoon(const ExtensionId& extension_id,
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

void StorageFrontend::SetCacheForTesting(
    settings_namespace::Namespace settings_namespace,
    std::unique_ptr<ValueStoreCache> cache) {
  DisableStorageForTesting(settings_namespace);  // IN-TEST
  caches_[settings_namespace] = cache.release();
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
void StorageFrontend::OnSettingsChanged(
    const ExtensionId& extension_id,
    StorageAreaNamespace storage_area,
    std::optional<api::storage::AccessLevel> session_access_level,
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

  // Restrict event to privileged context if session access level is set only to
  // trusted contexts.
  std::optional<mojom::ContextType> restrict_to_context_type = std::nullopt;
  if (storage_area == StorageAreaNamespace::kSession) {
    CHECK(session_access_level.has_value());
    if (session_access_level.value() ==
        api::storage::AccessLevel::kTrustedContexts) {
      restrict_to_context_type = mojom::ContextType::kPrivilegedExtension;
    }
  }

  auto make_changed_event = [&namespace_string,
                             restrict_to_context_type](base::Value changes) {
    base::Value::List args;
    args.Append(std::move(changes));
    args.Append(namespace_string);

    return std::make_unique<Event>(
        events::STORAGE_ON_CHANGED, api::storage::OnChanged::kEventName,
        std::move(args), /*restrict_to_browser_context=*/nullptr,
        restrict_to_context_type);
  };
  auto make_area_changed_event = [&storage_area, &area_event_name,
                                  restrict_to_context_type](
                                     base::Value changes) {
    base::Value::List args;
    args.Append(std::move(changes));
    return std::make_unique<Event>(StorageAreaToEventHistogram(storage_area),
                                   area_event_name, std::move(args), nullptr,
                                   restrict_to_context_type);
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

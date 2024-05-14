// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/value_store/value_store.h"
#include "components/value_store/value_store_factory_impl.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator_impl.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/lock_screen_data.h"
#include "extensions/common/extension_id.h"

namespace extensions {

namespace lock_screen_data {

namespace {

constexpr char kLockScreenDataPrefKey[] = "lockScreenDataItems";

constexpr char kExtensionStorageVersionPrefKey[] = "storage_version";
constexpr char kExtensionItemCountPrefKey[] = "item_count";

// Returns dictionary at `lock_screen_pref_dict[user_id][extension_id]`,
// creating it if needed.
base::Value::Dict& GetOrCreateExtensionInfoDict(
    const std::string& user_id,
    const ExtensionId& extension_id,
    base::Value::Dict& lock_screen_pref_dict) {
  return *lock_screen_pref_dict.EnsureDict(user_id)->EnsureDict(extension_id);
}

LockScreenItemStorage* g_data_item_storage = nullptr;

LockScreenItemStorage::ValueStoreCacheFactoryCallback*
    g_test_value_store_cache_factory_callback = nullptr;
LockScreenItemStorage::ValueStoreMigratorFactoryCallback*
    g_test_value_store_migrator_factory_callback = nullptr;
LockScreenItemStorage::ItemFactoryCallback* g_test_item_factory_callback =
    nullptr;
LockScreenItemStorage::RegisteredItemsGetter*
    g_test_registered_items_getter_callback = nullptr;
LockScreenItemStorage::ItemStoreDeleter* g_test_delete_all_items_callback =
    nullptr;

std::unique_ptr<LocalValueStoreCache> CreateValueStoreCache(
    const base::FilePath& storage_root) {
  if (g_test_value_store_cache_factory_callback)
    return g_test_value_store_cache_factory_callback->Run(storage_root);
  return std::make_unique<LocalValueStoreCache>(
      new value_store::ValueStoreFactoryImpl(storage_root));
}

std::unique_ptr<LockScreenValueStoreMigrator> CreateValueStoreMigrator(
    content::BrowserContext* context,
    ValueStoreCache* deprecated_value_store_cache,
    ValueStoreCache* value_store_cache,
    base::SequencedTaskRunner* task_runner,
    const std::string& crypto_key) {
  if (g_test_value_store_migrator_factory_callback)
    return g_test_value_store_migrator_factory_callback->Run();

  return std::make_unique<LockScreenValueStoreMigratorImpl>(
      context, deprecated_value_store_cache, value_store_cache, task_runner,
      crypto_key);
}

std::unique_ptr<DataItem> CreateDataItem(const std::string& item_id,
                                         const ExtensionId& extension_id,
                                         content::BrowserContext* context,
                                         ValueStoreCache* value_store_cache,
                                         base::SequencedTaskRunner* task_runner,
                                         const std::string& crypto_key) {
  return g_test_item_factory_callback
             ? g_test_item_factory_callback->Run(item_id, extension_id,
                                                 crypto_key)
             : std::make_unique<DataItem>(item_id, extension_id, context,
                                          value_store_cache, task_runner,
                                          crypto_key);
}

void GetRegisteredItems(const ExtensionId& extension_id,
                        content::BrowserContext* context,
                        ValueStoreCache* value_store_cache,
                        base::SequencedTaskRunner* task_runner,
                        DataItem::RegisteredValuesCallback callback) {
  if (g_test_registered_items_getter_callback) {
    g_test_registered_items_getter_callback->Run(extension_id,
                                                 std::move(callback));
    return;
  }
  DataItem::GetRegisteredValuesForExtension(context, value_store_cache,
                                            task_runner, extension_id,
                                            std::move(callback));
}

void DeleteAllItems(const ExtensionId& extension_id,
                    content::BrowserContext* context,
                    ValueStoreCache* value_store_cache,
                    base::SequencedTaskRunner* task_runner,
                    base::OnceClosure callback) {
  if (g_test_delete_all_items_callback) {
    g_test_delete_all_items_callback->Run(extension_id, std::move(callback));
    return;
  }
  DataItem::DeleteAllItemsForExtension(context, value_store_cache, task_runner,
                                       extension_id, std::move(callback));
}

}  // namespace

// static
LockScreenItemStorage* LockScreenItemStorage::GetIfAllowed(
    content::BrowserContext* context) {
  if (g_data_item_storage && !g_data_item_storage->IsContextAllowed(context))
    return nullptr;
  return g_data_item_storage;
}

// static
void LockScreenItemStorage::RegisterLocalState(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kLockScreenDataPrefKey);
}

LockScreenItemStorage::LockScreenItemStorage(
    content::BrowserContext* context,
    PrefService* local_state,
    const std::string& crypto_key,
    const base::FilePath& deprecated_storage_root,
    const base::FilePath& storage_root)
    : context_(context),
      user_id_(
          ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(context)),
      crypto_key_(crypto_key),
      local_state_(local_state),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      value_store_cache_(CreateValueStoreCache(storage_root.Append(user_id_))) {
  CHECK(!user_id_.empty());
  extension_registry_observation_.Observe(ExtensionRegistry::Get(context));
  task_runner_ = GetBackendTaskRunner();

  DCHECK(!g_data_item_storage);
  g_data_item_storage = this;

  std::set<ExtensionId> extensions_to_migrate = GetExtensionsToMigrate();
  if (!extensions_to_migrate.empty()) {
    deprecated_value_store_cache_ =
        CreateValueStoreCache(deprecated_storage_root);
    storage_migrator_ = CreateValueStoreMigrator(
        context_, deprecated_value_store_cache_.get(), value_store_cache_.get(),
        task_runner_.get(), crypto_key_);
    storage_migrator_->Run(
        extensions_to_migrate,
        base::BindRepeating(&LockScreenItemStorage::OnItemsMigratedForExtension,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  ClearUninstalledAppData();
}

LockScreenItemStorage::~LockScreenItemStorage() {
  data_item_cache_.clear();
  storage_migrator_.reset();

  if (deprecated_value_store_cache_) {
    task_runner_->DeleteSoon(FROM_HERE,
                             deprecated_value_store_cache_.release());
  }
  task_runner_->DeleteSoon(FROM_HERE, value_store_cache_.release());

  DCHECK_EQ(g_data_item_storage, this);
  g_data_item_storage = nullptr;
}

// static
void LockScreenItemStorage::SetValueStoreCacheFactoryForTesting(
    ValueStoreCacheFactoryCallback* factory_callback) {
  g_test_value_store_cache_factory_callback = factory_callback;
}

// static
void LockScreenItemStorage::SetValueStoreMigratorFactoryForTesting(
    ValueStoreMigratorFactoryCallback* factory_callback) {
  g_test_value_store_migrator_factory_callback = factory_callback;
}

// static
void LockScreenItemStorage::SetItemProvidersForTesting(
    RegisteredItemsGetter* items_getter_callback,
    ItemFactoryCallback* factory_callback,
    ItemStoreDeleter* deleter_callback) {
  g_test_registered_items_getter_callback = items_getter_callback;
  g_test_item_factory_callback = factory_callback;
  g_test_delete_all_items_callback = deleter_callback;
}

void LockScreenItemStorage::SetSessionLocked(bool session_locked) {
  SessionLockedState new_state = session_locked
                                     ? SessionLockedState::kLocked
                                     : SessionLockedState::kNotLocked;
  if (new_state == session_locked_state_)
    return;

  bool was_locked = session_locked_state_ == SessionLockedState::kLocked;
  session_locked_state_ = new_state;

  if (session_locked_state_ != SessionLockedState::kNotLocked)
    return;

  EventRouter* event_router = EventRouter::Get(context_);
  std::set<std::string> extensions = GetExtensionsWithDataItems(false);
  for (const auto& id : extensions) {
    // If the session state is unlocked, dispatch Item availability events to
    // apps with available data items.
    api::lock_screen_data::DataItemsAvailableEvent event_args;
    event_args.was_locked = was_locked;

    std::unique_ptr<Event> event = std::make_unique<Event>(
        events::LOCK_SCREEN_DATA_ON_DATA_ITEMS_AVAILABLE,
        api::lock_screen_data::OnDataItemsAvailable::kEventName,
        api::lock_screen_data::OnDataItemsAvailable::Create(event_args));
    event_router->DispatchEventToExtension(id, std::move(event));
  }
}

void LockScreenItemStorage::CreateItem(const ExtensionId& extension_id,
                                       CreateCallback callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::BindOnce(&LockScreenItemStorage::CreateItemImpl,
                                   weak_ptr_factory_.GetWeakPtr(), extension_id,
                                   std::move(callback)));
}

void LockScreenItemStorage::GetAllForExtension(const ExtensionId& extension_id,
                                               DataItemListCallback callback) {
  EnsureCacheForExtensionLoaded(
      extension_id,
      base::BindOnce(&LockScreenItemStorage::GetAllForExtensionImpl,
                     weak_ptr_factory_.GetWeakPtr(), extension_id,
                     std::move(callback)));
}

void LockScreenItemStorage::SetItemContent(const ExtensionId& extension_id,
                                           const std::string& item_id,
                                           const std::vector<char>& data,
                                           WriteCallback callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::BindOnce(&LockScreenItemStorage::SetItemContentImpl,
                                   weak_ptr_factory_.GetWeakPtr(), extension_id,
                                   item_id, data, std::move(callback)));
}

void LockScreenItemStorage::GetItemContent(const ExtensionId& extension_id,
                                           const std::string& item_id,
                                           ReadCallback callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::BindOnce(&LockScreenItemStorage::GetItemContentImpl,
                                   weak_ptr_factory_.GetWeakPtr(), extension_id,
                                   item_id, std::move(callback)));
}

void LockScreenItemStorage::DeleteItem(const ExtensionId& extension_id,
                                       const std::string& item_id,
                                       WriteCallback callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::BindOnce(&LockScreenItemStorage::DeleteItemImpl,
                                   weak_ptr_factory_.GetWeakPtr(), extension_id,
                                   item_id, std::move(callback)));
}

void LockScreenItemStorage::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  ClearExtensionData(extension->id());
}

LockScreenItemStorage::CachedExtensionData::CachedExtensionData() = default;

LockScreenItemStorage::CachedExtensionData::~CachedExtensionData() = default;

bool LockScreenItemStorage::IsContextAllowed(content::BrowserContext* context) {
  switch (session_locked_state_) {
    case SessionLockedState::kUnknown:
      return false;
    case SessionLockedState::kLocked:
      return ExtensionsBrowserClient::Get()->IsLockScreenContext(context);
    case SessionLockedState::kNotLocked:
      return context_ == context;
  }
  NOTREACHED_IN_MIGRATION() << "Unknown session locked state";
  return false;
}

void LockScreenItemStorage::CreateItemImpl(const ExtensionId& extension_id,
                                           CreateCallback callback) {
  ExtensionDataMap::iterator data = data_item_cache_.find(extension_id);
  if (data == data_item_cache_.end() ||
      data->second.state != CachedExtensionData::State::kLoaded) {
    return;
  }

  std::unique_ptr<DataItem> item = CreateDataItem(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), extension_id,
      context_, value_store_cache_.get(), task_runner_.get(), crypto_key_);
  DataItem* item_ptr = item.get();
  item_ptr->Register(base::BindOnce(
      &LockScreenItemStorage::OnItemRegistered, weak_ptr_factory_.GetWeakPtr(),
      std::move(item), extension_id, tick_clock_->NowTicks(),
      std::move(callback)));
}

void LockScreenItemStorage::GetAllForExtensionImpl(
    const ExtensionId& extension_id,
    DataItemListCallback callback) {
  std::vector<const DataItem*> items;
  ExtensionDataMap::iterator extension_data =
      data_item_cache_.find(extension_id);
  if (extension_data == data_item_cache_.end()) {
    std::move(callback).Run(items);
    return;
  }

  for (const auto& item : extension_data->second.data_items) {
    if (!item.second)
      continue;
    items.push_back(item.second.get());
  }

  std::move(callback).Run(items);
}

void LockScreenItemStorage::SetItemContentImpl(const ExtensionId& extension_id,
                                               const std::string& item_id,
                                               const std::vector<char>& data,
                                               WriteCallback callback) {
  DataItem* item = FindItem(extension_id, item_id);
  if (!item) {
    std::move(callback).Run(OperationResult::kNotFound);
    return;
  }

  item->Write(data, std::move(callback));
}

void LockScreenItemStorage::GetItemContentImpl(const ExtensionId& extension_id,
                                               const std::string& item_id,
                                               ReadCallback callback) {
  DataItem* item = FindItem(extension_id, item_id);
  if (!item) {
    std::move(callback).Run(OperationResult::kNotFound, nullptr);
    return;
  }

  item->Read(std::move(callback));
}

void LockScreenItemStorage::DeleteItemImpl(const ExtensionId& extension_id,
                                           const std::string& item_id,
                                           WriteCallback callback) {
  DataItem* item = FindItem(extension_id, item_id);
  if (!item) {
    std::move(callback).Run(OperationResult::kNotFound);
    return;
  }

  item->Delete(base::BindOnce(
      &LockScreenItemStorage::OnItemDeleted, weak_ptr_factory_.GetWeakPtr(),
      extension_id, item_id, tick_clock_->NowTicks(), std::move(callback)));
}

void LockScreenItemStorage::OnItemRegistered(std::unique_ptr<DataItem> item,
                                             const ExtensionId& extension_id,
                                             const base::TimeTicks& start_time,
                                             CreateCallback callback,
                                             OperationResult result) {
  if (result != OperationResult::kSuccess) {
    std::move(callback).Run(result, nullptr);
    return;
  }

  DataItem* item_ptr = item.get();
  data_item_cache_[extension_id].data_items.emplace(item_ptr->id(),
                                                    std::move(item));

  {
    ScopedDictPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    base::Value::Dict& info =
        GetOrCreateExtensionInfoDict(user_id_, extension_id, *update);
    info.Set(
        kExtensionItemCountPrefKey,
        static_cast<int>(data_item_cache_[extension_id].data_items.size()));
  }

  std::move(callback).Run(OperationResult::kSuccess, item_ptr);
}

void LockScreenItemStorage::OnItemDeleted(const ExtensionId& extension_id,
                                          const std::string& item_id,
                                          const base::TimeTicks& start_time,
                                          WriteCallback callback,
                                          OperationResult result) {
  data_item_cache_[extension_id].data_items.erase(item_id);
  {
    ScopedDictPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    base::Value::Dict& info =
        GetOrCreateExtensionInfoDict(user_id_, extension_id, *update);
    info.Set(
        kExtensionItemCountPrefKey,
        static_cast<int>(data_item_cache_[extension_id].data_items.size()));
  }

  std::move(callback).Run(result);
}

void LockScreenItemStorage::EnsureCacheForExtensionLoaded(
    const ExtensionId& extension_id,
    base::OnceClosure callback) {
  CachedExtensionData* data = &data_item_cache_[extension_id];
  if (data->state == CachedExtensionData::State::kLoaded) {
    std::move(callback).Run();
    return;
  }

  data->load_callbacks.push_back(std::move(callback));

  if (data->state == CachedExtensionData::State::kRemoving ||
      data->state == CachedExtensionData::State::kLoading) {
    return;
  }

  data->state = CachedExtensionData::State::kLoading;

  if (storage_migrator_ &&
      storage_migrator_->IsMigratingExtensionData(extension_id)) {
    return;
  }

  GetRegisteredItems(extension_id, context_, value_store_cache_.get(),
                     task_runner_.get(),
                     base::BindOnce(&LockScreenItemStorage::OnGotExtensionItems,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    extension_id, tick_clock_->NowTicks()));
}

void LockScreenItemStorage::OnItemsMigratedForExtension(
    const ExtensionId& extension_id) {
  // Load registered data items for the extensions and update the extension's
  // local state so it correctly describes its post-migration lock screen data
  // items state.
  data_item_cache_[extension_id].state = CachedExtensionData::State::kLoading;
  GetRegisteredItems(extension_id, context_, value_store_cache_.get(),
                     task_runner_.get(),
                     base::BindOnce(&LockScreenItemStorage::OnGotExtensionItems,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    extension_id, tick_clock_->NowTicks()));
}

void LockScreenItemStorage::OnGotExtensionItems(
    const ExtensionId& extension_id,
    const base::TimeTicks& start_time,
    OperationResult result,
    base::Value::Dict items) {
  ExtensionDataMap::iterator data = data_item_cache_.find(extension_id);
  if (data == data_item_cache_.end() ||
      data->second.state != CachedExtensionData::State::kLoading) {
    return;
  }

  if (result == OperationResult::kSuccess) {
    for (const auto item : items) {
      std::unique_ptr<DataItem> data_item = CreateDataItem(
          item.first, extension_id, context_, value_store_cache_.get(),
          task_runner_.get(), crypto_key_);
      data->second.data_items.emplace(item.first, std::move(data_item));
    }
  }

  {
    ScopedDictPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    base::Value::Dict& info =
        GetOrCreateExtensionInfoDict(user_id_, extension_id, *update);
    info.Set(kExtensionItemCountPrefKey,
             static_cast<int>(data->second.data_items.size()));
    info.Set(kExtensionStorageVersionPrefKey, 2);
  }

  data->second.state = CachedExtensionData::State::kLoaded;
  RunExtensionDataLoadCallbacks(&data->second);
}

DataItem* LockScreenItemStorage::FindItem(const ExtensionId& extension_id,
                                          const std::string& item_id) {
  ExtensionDataMap::iterator extension_data =
      data_item_cache_.find(extension_id);
  if (extension_data == data_item_cache_.end())
    return nullptr;

  if (extension_data->second.state != CachedExtensionData::State::kLoaded)
    return nullptr;
  DataItemMap::iterator item_it =
      extension_data->second.data_items.find(item_id);
  if (item_it == extension_data->second.data_items.end())
    return nullptr;

  if (!item_it->second)
    return nullptr;
  return item_it->second.get();
}

std::set<std::string> LockScreenItemStorage::GetExtensionsWithDataItems(
    bool include_empty) {
  std::set<std::string> result;

  const base::Value::Dict& items =
      local_state_->GetDict(kLockScreenDataPrefKey);
  const base::Value::Dict* user_data = items.FindDictByDottedPath(user_id_);
  if (!user_data)
    return result;

  for (auto it : *user_data) {
    if (it.second.is_int() && (include_empty || it.second.GetInt() > 0)) {
      result.insert(it.first);
    } else if (it.second.is_dict()) {
      std::optional<int> count =
          it.second.GetDict().FindInt(kExtensionItemCountPrefKey);
      if (include_empty || (count && *count > 0)) {
        result.insert(it.first);
      }
    }
  }
  return result;
}

std::set<ExtensionId> LockScreenItemStorage::GetExtensionsToMigrate() {
  std::set<ExtensionId> result;

  const base::Value::Dict& items =
      local_state_->GetDict(kLockScreenDataPrefKey);

  const base::Value::Dict* user_data = items.FindDictByDottedPath(user_id_);
  if (!user_data)
    return result;

  for (auto it : *user_data) {
    if (it.second.is_int())
      result.insert(it.first);
  }
  return result;
}

void LockScreenItemStorage::ClearUninstalledAppData() {
  std::set<std::string> extensions =
      GetExtensionsWithDataItems(true /* include_empty */);
  for (const auto& id : extensions) {
    if (!ExtensionRegistry::Get(context_)->GetInstalledExtension(id))
      ClearExtensionData(id);
  }
}

void LockScreenItemStorage::ClearExtensionData(const std::string& id) {
  CachedExtensionData* data = &data_item_cache_[id];
  if (data->state == CachedExtensionData::State::kRemoving)
    return;
  data->state = CachedExtensionData::State::kRemoving;
  data->data_items.clear();
  RunExtensionDataLoadCallbacks(data);

  base::OnceClosure callback =
      base::BindOnce(&LockScreenItemStorage::RemoveExtensionFromLocalState,
                     weak_ptr_factory_.GetWeakPtr(), id);

  if (storage_migrator_ && storage_migrator_->IsMigratingExtensionData(id)) {
    storage_migrator_->ClearDataForExtension(id, std::move(callback));
  } else {
    DeleteAllItems(id, context_, value_store_cache_.get(), task_runner_.get(),
                   std::move(callback));
  }
}

void LockScreenItemStorage::RemoveExtensionFromLocalState(
    const std::string& id) {
  {
    ScopedDictPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    update->RemoveByDottedPath(base::StrCat({user_id_, ".", id}));
  }

  data_item_cache_[id].state = CachedExtensionData::State::kLoaded;
  RunExtensionDataLoadCallbacks(&data_item_cache_[id]);
}

void LockScreenItemStorage::RunExtensionDataLoadCallbacks(
    CachedExtensionData* cache_data) {
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(cache_data->load_callbacks);
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

}  // namespace lock_screen_data
}  // namespace extensions

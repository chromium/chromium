// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/api/lock_screen_data/data_item.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_value_store_migrator_impl.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/value_store/value_store.h"
#include "extensions/browser/value_store/value_store_factory_impl.h"
#include "extensions/common/api/lock_screen_data.h"

namespace extensions {

namespace lock_screen_data {

namespace {

constexpr char kLockScreenDataPrefKey[] = "lockScreenDataItems";

constexpr char kExtensionStorageVersionPrefKey[] = "storage_version";
constexpr char kExtensionItemCountPrefKey[] = "item_count";

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
      new ValueStoreFactoryImpl(storage_root));
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
                                         const std::string& extension_id,
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

void GetRegisteredItems(const std::string& extension_id,
                        content::BrowserContext* context,
                        ValueStoreCache* value_store_cache,
                        base::SequencedTaskRunner* task_runner,
                        const DataItem::RegisteredValuesCallback& callback) {
  if (g_test_registered_items_getter_callback) {
    g_test_registered_items_getter_callback->Run(extension_id, callback);
    return;
  }
  DataItem::GetRegisteredValuesForExtension(
      context, value_store_cache, task_runner, extension_id, callback);
}

void DeleteAllItems(const std::string& extension_id,
                    content::BrowserContext* context,
                    ValueStoreCache* value_store_cache,
                    base::SequencedTaskRunner* task_runner,
                    const base::Closure& callback) {
  if (g_test_delete_all_items_callback) {
    g_test_delete_all_items_callback->Run(extension_id, callback);
    return;
  }
  DataItem::DeleteAllItemsForExtension(context, value_store_cache, task_runner,
                                       extension_id, callback);
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
  extension_registry_observer_.Add(ExtensionRegistry::Get(context));
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
        base::Bind(&LockScreenItemStorage::OnItemsMigratedForExtension,
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

void LockScreenItemStorage::CreateItem(const std::string& extension_id,
                                       const CreateCallback& callback) {
  EnsureCacheForExtensionLoaded(
      extension_id,
      base::Bind(&LockScreenItemStorage::CreateItemImpl,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, callback));
}

void LockScreenItemStorage::GetAllForExtension(
    const std::string& extension_id,
    const DataItemListCallback& callback) {
  EnsureCacheForExtensionLoaded(
      extension_id,
      base::Bind(&LockScreenItemStorage::GetAllForExtensionImpl,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, callback));
}

void LockScreenItemStorage::SetItemContent(
    const std::string& extension_id,
    const std::string& item_id,
    const std::vector<char>& data,
    const LockScreenItemStorage::WriteCallback& callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::Bind(&LockScreenItemStorage::SetItemContentImpl,
                               weak_ptr_factory_.GetWeakPtr(), extension_id,
                               item_id, data, callback));
}

void LockScreenItemStorage::GetItemContent(
    const std::string& extension_id,
    const std::string& item_id,
    const LockScreenItemStorage::ReadCallback& callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::Bind(&LockScreenItemStorage::GetItemContentImpl,
                               weak_ptr_factory_.GetWeakPtr(), extension_id,
                               item_id, callback));
}

void LockScreenItemStorage::DeleteItem(const std::string& extension_id,
                                       const std::string& item_id,
                                       const WriteCallback& callback) {
  EnsureCacheForExtensionLoaded(
      extension_id, base::Bind(&LockScreenItemStorage::DeleteItemImpl,
                               weak_ptr_factory_.GetWeakPtr(), extension_id,
                               item_id, callback));
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
  NOTREACHED() << "Unknown session locked state";
  return false;
}

void LockScreenItemStorage::CreateItemImpl(const std::string& extension_id,
                                           const CreateCallback& callback) {
  ExtensionDataMap::iterator data = data_item_cache_.find(extension_id);
  if (data == data_item_cache_.end() ||
      data->second.state != CachedExtensionData::State::kLoaded) {
    return;
  }

  std::unique_ptr<DataItem> item =
      CreateDataItem(base::GenerateGUID(), extension_id, context_,
                     value_store_cache_.get(), task_runner_.get(), crypto_key_);
  DataItem* item_ptr = item.get();
  item_ptr->Register(base::Bind(&LockScreenItemStorage::OnItemRegistered,
                                weak_ptr_factory_.GetWeakPtr(),
                                base::Passed(std::move(item)), extension_id,
                                tick_clock_->NowTicks(), callback));
}

void LockScreenItemStorage::GetAllForExtensionImpl(
    const std::string& extension_id,
    const DataItemListCallback& callback) {
  std::vector<const DataItem*> items;
  ExtensionDataMap::iterator extension_data =
      data_item_cache_.find(extension_id);
  if (extension_data == data_item_cache_.end()) {
    callback.Run(items);
    return;
  }

  for (const auto& item : extension_data->second.data_items) {
    if (!item.second)
      continue;
    items.push_back(item.second.get());
  }

  callback.Run(items);
}

void LockScreenItemStorage::SetItemContentImpl(
    const std::string& extension_id,
    const std::string& item_id,
    const std::vector<char>& data,
    const LockScreenItemStorage::WriteCallback& callback) {
  DataItem* item = FindItem(extension_id, item_id);
  if (!item) {
    callback.Run(OperationResult::kNotFound);
    return;
  }

  item->Write(data, base::Bind(&LockScreenItemStorage::OnItemWritten,
                               weak_ptr_factory_.GetWeakPtr(),
                               tick_clock_->NowTicks(), callback));
}

void LockScreenItemStorage::GetItemContentImpl(const std::string& extension_id,
                                               const std::string& item_id,
                                               const ReadCallback& callback) {
  DataItem* item = FindItem(extension_id, item_id);
  if (!item) {
    callback.Run(OperationResult::kNotFound, nullptr);
    return;
  }

  item->Read(base::Bind(&LockScreenItemStorage::OnItemRead,
                        weak_ptr_factory_.GetWeakPtr(), tick_clock_->NowTicks(),
                        callback));
}

void LockScreenItemStorage::DeleteItemImpl(const std::string& extension_id,
                                           const std::string& item_id,
                                           const WriteCallback& callback) {
  DataItem* item = FindItem(extension_id, item_id);
  if (!item) {
    callback.Run(OperationResult::kNotFound);
    return;
  }

  item->Delete(base::Bind(&LockScreenItemStorage::OnItemDeleted,
                          weak_ptr_factory_.GetWeakPtr(), extension_id, item_id,
                          tick_clock_->NowTicks(), callback));
}

void LockScreenItemStorage::OnItemRegistered(std::unique_ptr<DataItem> item,
                                             const std::string& extension_id,
                                             const base::TimeTicks& start_time,
                                             const CreateCallback& callback,
                                             OperationResult result) {
  if (result == OperationResult::kSuccess) {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.OperationDuration.RegisterItem",
        tick_clock_->NowTicks() - start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.FailedOperationDuration.RegisterItem",
        tick_clock_->NowTicks() - start_time);
  }

  if (result != OperationResult::kSuccess) {
    callback.Run(result, nullptr);
    return;
  }

  DataItem* item_ptr = item.get();
  data_item_cache_[extension_id].data_items.emplace(item_ptr->id(),
                                                    std::move(item));

  {
    DictionaryPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    update->SetPath({user_id_, extension_id, kExtensionItemCountPrefKey},
                    base::Value(static_cast<int>(
                        data_item_cache_[extension_id].data_items.size())));
  }

  callback.Run(OperationResult::kSuccess, item_ptr);
}

void LockScreenItemStorage::OnItemWritten(const base::TimeTicks& start_time,
                                          const WriteCallback& callback,
                                          OperationResult result) {
  if (result == OperationResult::kSuccess) {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.OperationDuration.WriteItem",
        tick_clock_->NowTicks() - start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.FailedOperationDuration.WriteItem",
        tick_clock_->NowTicks() - start_time);
  }

  callback.Run(result);
}

void LockScreenItemStorage::OnItemRead(
    const base::TimeTicks& start_time,
    const ReadCallback& callback,
    OperationResult result,
    std::unique_ptr<std::vector<char>> data) {
  if (result == OperationResult::kSuccess) {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.OperationDuration.ReadItem",
        tick_clock_->NowTicks() - start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.FailedOperationDuration.ReadItem",
        tick_clock_->NowTicks() - start_time);
  }

  callback.Run(result, std::move(data));
}

void LockScreenItemStorage::OnItemDeleted(const std::string& extension_id,
                                          const std::string& item_id,
                                          const base::TimeTicks& start_time,
                                          const WriteCallback& callback,
                                          OperationResult result) {
  if (result == OperationResult::kSuccess) {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.OperationDuration.DeleteItem",
        tick_clock_->NowTicks() - start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.FailedOperationDuration.DeleteItem",
        tick_clock_->NowTicks() - start_time);
  }

  data_item_cache_[extension_id].data_items.erase(item_id);
  {
    DictionaryPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    update->SetPath({user_id_, extension_id, kExtensionItemCountPrefKey},
                    base::Value(static_cast<int>(
                        data_item_cache_[extension_id].data_items.size())));
  }

  callback.Run(result);
}

void LockScreenItemStorage::EnsureCacheForExtensionLoaded(
    const std::string& extension_id,
    const base::Closure& callback) {
  CachedExtensionData* data = &data_item_cache_[extension_id];
  if (data->state == CachedExtensionData::State::kLoaded) {
    callback.Run();
    return;
  }

  data->load_callbacks.push_back(callback);

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
                     base::Bind(&LockScreenItemStorage::OnGotExtensionItems,
                                weak_ptr_factory_.GetWeakPtr(), extension_id,
                                tick_clock_->NowTicks()));
}

void LockScreenItemStorage::OnItemsMigratedForExtension(
    const ExtensionId& extension_id) {
  // Load registered data items for the extensions and update the extension's
  // local state so it correctly describes its post-migration lock screen data
  // items state.
  data_item_cache_[extension_id].state = CachedExtensionData::State::kLoading;
  GetRegisteredItems(extension_id, context_, value_store_cache_.get(),
                     task_runner_.get(),
                     base::Bind(&LockScreenItemStorage::OnGotExtensionItems,
                                weak_ptr_factory_.GetWeakPtr(), extension_id,
                                tick_clock_->NowTicks()));
}

void LockScreenItemStorage::OnGotExtensionItems(
    const std::string& extension_id,
    const base::TimeTicks& start_time,
    OperationResult result,
    std::unique_ptr<base::DictionaryValue> items) {
  ExtensionDataMap::iterator data = data_item_cache_.find(extension_id);
  if (data == data_item_cache_.end() ||
      data->second.state != CachedExtensionData::State::kLoading) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "Apps.LockScreen.DataItemStorage.OperationResult.GetRegisteredItems",
      result, OperationResult::kCount);

  if (result == OperationResult::kSuccess) {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.OperationDuration.GetRegisteredItems",
        tick_clock_->NowTicks() - start_time);
  } else {
    UMA_HISTOGRAM_TIMES(
        "Apps.LockScreen.DataItemStorage.FailedOperationDuration."
        "GetRegisteredItems",
        tick_clock_->NowTicks() - start_time);
  }

  if (result == OperationResult::kSuccess) {
    for (base::DictionaryValue::Iterator item_iter(*items);
         !item_iter.IsAtEnd(); item_iter.Advance()) {
      std::unique_ptr<DataItem> item = CreateDataItem(
          item_iter.key(), extension_id, context_, value_store_cache_.get(),
          task_runner_.get(), crypto_key_);
      data->second.data_items.emplace(item_iter.key(), std::move(item));
    }

    // Record number of registered items.
    UMA_HISTOGRAM_COUNTS_100(
        "Apps.LockScreen.DataItemStorage.RegisteredItemsCount",
        data->second.data_items.size());
  }

  {
    DictionaryPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    base::Value info(base::Value::Type::DICTIONARY);
    info.SetKey(kExtensionItemCountPrefKey,
                base::Value(static_cast<int>(data->second.data_items.size())));
    info.SetKey(kExtensionStorageVersionPrefKey, base::Value(2));
    update->SetPath({user_id_, extension_id}, std::move(info));
  }

  data->second.state = CachedExtensionData::State::kLoaded;
  RunExtensionDataLoadCallbacks(&data->second);
}

DataItem* LockScreenItemStorage::FindItem(const std::string& extension_id,
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

  const base::DictionaryValue* user_data = nullptr;
  const base::DictionaryValue* items =
      local_state_->GetDictionary(kLockScreenDataPrefKey);
  if (!items || !items->GetDictionary(user_id_, &user_data) || !user_data)
    return result;

  for (base::DictionaryValue::Iterator extension_iter(*user_data);
       !extension_iter.IsAtEnd(); extension_iter.Advance()) {
    if (extension_iter.value().is_int() &&
        (include_empty || extension_iter.value().GetInt() > 0)) {
      result.insert(extension_iter.key());
    } else if (extension_iter.value().is_dict()) {
      const base::Value* count = extension_iter.value().FindKeyOfType(
          kExtensionItemCountPrefKey, base::Value::Type::INTEGER);
      if (include_empty || (count && count->GetInt() > 0)) {
        result.insert(extension_iter.key());
      }
    }
  }
  return result;
}

std::set<ExtensionId> LockScreenItemStorage::GetExtensionsToMigrate() {
  std::set<ExtensionId> result;

  const base::DictionaryValue* user_data = nullptr;
  const base::DictionaryValue* items =
      local_state_->GetDictionary(kLockScreenDataPrefKey);
  if (!items || !items->GetDictionary(user_id_, &user_data) || !user_data)
    return result;

  for (base::DictionaryValue::Iterator extension_iter(*user_data);
       !extension_iter.IsAtEnd(); extension_iter.Advance()) {
    if (extension_iter.value().is_int())
      result.insert(extension_iter.key());
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

  base::Closure callback =
      base::Bind(&LockScreenItemStorage::RemoveExtensionFromLocalState,
                 weak_ptr_factory_.GetWeakPtr(), id);

  if (storage_migrator_ && storage_migrator_->IsMigratingExtensionData(id)) {
    storage_migrator_->ClearDataForExtension(id, callback);
  } else {
    DeleteAllItems(id, context_, value_store_cache_.get(), task_runner_.get(),
                   callback);
  }
}

void LockScreenItemStorage::RemoveExtensionFromLocalState(
    const std::string& id) {
  {
    DictionaryPrefUpdate update(local_state_, kLockScreenDataPrefKey);
    update->RemovePath({user_id_, id});
  }

  data_item_cache_[id].state = CachedExtensionData::State::kLoaded;
  RunExtensionDataLoadCallbacks(&data_item_cache_[id]);
}

void LockScreenItemStorage::RunExtensionDataLoadCallbacks(
    CachedExtensionData* cache_data) {
  std::vector<base::Closure> callbacks;
  callbacks.swap(cache_data->load_callbacks);
  for (auto& callback : callbacks)
    callback.Run();
}

}  // namespace lock_screen_data
}  // namespace extensions

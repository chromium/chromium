// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/lock_screen_data/data_item.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/value_store/value_store.h"

namespace extensions {
namespace lock_screen_data {

namespace {

// Key for the dictionary in the value store containing all items registered
// for the extension.
const char kStoreKeyRegisteredItems[] = "registered_items";

constexpr int kAesInitializationVectorLength = 16;

// Encrypts |data| with AES key |raw_key|. Returns whether the encryption was
// successful, in which case |*result| will be set to the encrypted data.
bool EncryptData(const std::vector<char> data,
                 const std::string& raw_key,
                 std::string* result) {
  std::string initialization_vector(kAesInitializationVectorLength, ' ');
  std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key);
  if (!key)
    return false;

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CBC, initialization_vector))
    return false;

  return encryptor.Encrypt(std::string(data.data(), data.size()), result);
}

// Decrypts |data| content using AES key |raw_key|. Returns the operation result
// code. On success, |*result| will be set to the clear-text data.
OperationResult DecryptData(const std::string& data,
                            const std::string& raw_key,
                            std::vector<char>* result) {
  std::string initialization_vector(kAesInitializationVectorLength, ' ');
  std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, raw_key);
  if (!key)
    return OperationResult::kInvalidKey;

  crypto::Encryptor encryptor;
  if (!encryptor.Init(key.get(), crypto::Encryptor::CBC, initialization_vector))
    return OperationResult::kInvalidKey;

  std::string decrypted;
  if (!encryptor.Decrypt(data, &decrypted))
    return OperationResult::kWrongKey;

  *result =
      std::vector<char>(decrypted.data(), decrypted.data() + decrypted.size());

  return OperationResult::kSuccess;
}

// Returns whether the value store |store| contains a registered item with ID
// |item_id|.
bool IsItemRegistered(ValueStore* store, const std::string& item_id) {
  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);

  const base::DictionaryValue* registered_items = nullptr;
  return read.status().ok() &&
         read.settings().GetDictionary(kStoreKeyRegisteredItems,
                                       &registered_items) &&
         registered_items->HasKey(item_id);
}

// Gets a dictionary value that contains set of all registered data items from
// the values store |store|.
// |result| - the item fetch operation status code.
// |value| - on success, set to the dictionary containing registered data items.
//     Note that the dictionary will not contain data item content.
void GetRegisteredItems(OperationResult* result,
                        base::DictionaryValue* values,
                        ValueStore* store) {
  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);

  values->Clear();

  std::unique_ptr<base::Value> registered_items;
  if (!read.status().ok()) {
    *result = OperationResult::kFailed;
    return;
  }

  // Using remove to pass ownership of registered_item dict to
  // |registered_items| (and avoid doing a copy |read.settings()|
  // sub-dictionary).
  if (!read.settings().Remove(kStoreKeyRegisteredItems, &registered_items)) {
    // If the registered items dictionary cannot be found, assume no items have
    // yet been registered, and return empty result.
    *result = OperationResult::kSuccess;
    return;
  }

  std::unique_ptr<base::DictionaryValue> items_dict =
      base::DictionaryValue::From(std::move(registered_items));

  *result =
      items_dict.get() ? OperationResult::kSuccess : OperationResult::kFailed;
  if (items_dict)
    values->Swap(items_dict.get());
}

// Registers a data item with ID |item_id| in value store |store|.
void RegisterItem(OperationResult* result,
                  const std::string& item_id,
                  ValueStore* store) {
  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);

  std::unique_ptr<base::Value> registered_items;
  if (!read.status().ok()) {
    *result = OperationResult::kFailed;
    return;
  }
  if (!read.settings().Remove(kStoreKeyRegisteredItems, &registered_items))
    registered_items = std::make_unique<base::DictionaryValue>();

  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(registered_items));
  if (!dict) {
    *result = OperationResult::kFailed;
    return;
  }

  if (dict->HasKey(item_id)) {
    *result = OperationResult::kAlreadyRegistered;
    return;
  }

  dict->Set(item_id, std::make_unique<base::DictionaryValue>());

  ValueStore::WriteResult write =
      store->Set(ValueStore::DEFAULTS, kStoreKeyRegisteredItems, *dict);
  *result = write.status().ok() ? OperationResult::kSuccess
                                : OperationResult::kFailed;
}

// Encrypts |data| with AES key |encryption_key| and saved it as |item_id|
// content to the value store |store|. The encrypted data is saved base64
// encoded.
void WriteImpl(OperationResult* result,
               const std::string item_id,
               const std::vector<char>& data,
               const std::string& encryption_key,
               ValueStore* store) {
  if (!IsItemRegistered(store, item_id)) {
    *result = OperationResult::kNotFound;
    return;
  }

  std::string encrypted;
  if (!EncryptData(data, encryption_key, &encrypted)) {
    *result = OperationResult::kInvalidKey;
    return;
  }
  base::Base64Encode(encrypted, &encrypted);

  UMA_HISTOGRAM_COUNTS_10M("Apps.LockScreen.DataItemStorage.ClearTextItemSize",
                           data.size());

  UMA_HISTOGRAM_COUNTS_10M("Apps.LockScreen.DataItemStorage.EncryptedItemSize",
                           encrypted.size());

  ValueStore::WriteResult write = store->Set(ValueStore::DEFAULTS, item_id,
                                             base::Value(std::move(encrypted)));

  *result = write.status().ok() ? OperationResult::kSuccess
                                : OperationResult::kFailed;
}

// Gets content of the data item with ID |item_id| from value store |store|,
// and decrypts it using |decryption_key|. On success, the decrypted data is
// returned as |*data| contents. Note that this method expects the encrypted
// data content in the value store is base64 encoded.
void ReadImpl(OperationResult* result,
              std::vector<char>* data,
              const std::string& item_id,
              const std::string& decryption_key,
              ValueStore* store) {
  if (!IsItemRegistered(store, item_id)) {
    *result = OperationResult::kNotFound;
    return;
  }

  ValueStore::ReadResult read = store->Get(item_id);
  if (!read.status().ok()) {
    *result = OperationResult::kNotFound;
    return;
  }

  const base::Value* item;
  if (!read.settings().Get(item_id, &item)) {
    *result = OperationResult::kSuccess;
    *data = std::vector<char>();
    return;
  }

  std::string read_data;
  if (!item->is_string() ||
      !base::Base64Decode(item->GetString(), &read_data)) {
    *result = OperationResult::kFailed;
    return;
  }

  *result = DecryptData(read_data, decryption_key, data);
}

// Unregisters and deletes the item with |item_id| from the |valus_store|.
void DeleteImpl(OperationResult* result,
                const std::string& item_id,
                ValueStore* store) {
  ValueStore::WriteResult remove =
      store->Remove(std::vector<std::string>({item_id}));
  if (!remove.status().ok()) {
    *result = OperationResult::kFailed;
    return;
  }

  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);
  if (!read.status().ok()) {
    *result = OperationResult::kFailed;
    return;
  }

  base::DictionaryValue* registered_items = nullptr;
  if (!read.settings().GetDictionary(kStoreKeyRegisteredItems,
                                     &registered_items) ||
      !registered_items->Remove(item_id, nullptr)) {
    *result = OperationResult::kNotFound;
    return;
  }

  ValueStore::WriteResult write = store->Set(
      ValueStore::DEFAULTS, kStoreKeyRegisteredItems, *registered_items);
  *result = write.status().ok() ? OperationResult::kSuccess
                                : OperationResult::kFailed;
}

void OnGetRegisteredValues(const DataItem::RegisteredValuesCallback& callback,
                           std::unique_ptr<OperationResult> result,
                           std::unique_ptr<base::DictionaryValue> values) {
  callback.Run(*result, std::move(values));
}

}  // namespace

// static
void DataItem::GetRegisteredValuesForExtension(
    content::BrowserContext* context,
    ValueStoreCache* value_store_cache,
    base::SequencedTaskRunner* task_runner,
    const std::string& extension_id,
    const RegisteredValuesCallback& callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context)->GetExtensionById(
          extension_id, ExtensionRegistry::ENABLED);
  if (!extension) {
    callback.Run(OperationResult::kUnknownExtension, nullptr);
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();
  std::unique_ptr<base::DictionaryValue> values =
      std::make_unique<base::DictionaryValue>();
  base::DictionaryValue* values_ptr = values.get();

  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache),
                     base::Bind(&GetRegisteredItems, result_ptr, values_ptr),
                     extension),
      base::BindOnce(&OnGetRegisteredValues, callback, std::move(result),
                     std::move(values)));
}

// static
void DataItem::DeleteAllItemsForExtension(
    content::BrowserContext* context,
    ValueStoreCache* value_store_cache,
    base::SequencedTaskRunner* task_runner,
    const std::string& extension_id,
    const base::Closure& callback) {
  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::DeleteStorageSoon,
                     base::Unretained(value_store_cache), extension_id),
      callback);
}

DataItem::DataItem(const std::string& id,
                   const std::string& extension_id,
                   content::BrowserContext* context,
                   ValueStoreCache* value_store_cache,
                   base::SequencedTaskRunner* task_runner,
                   const std::string& crypto_key)
    : id_(id),
      extension_id_(extension_id),
      context_(context),
      value_store_cache_(value_store_cache),
      task_runner_(task_runner),
      crypto_key_(crypto_key) {}

DataItem::~DataItem() = default;

void DataItem::Register(const WriteCallback& callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->GetExtensionById(
          extension_id_, ExtensionRegistry::ENABLED);
  if (!extension) {
    callback.Run(OperationResult::kUnknownExtension);
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache_),
                     base::Bind(&RegisterItem, result_ptr, id()), extension),
      base::BindOnce(&DataItem::OnWriteDone, weak_ptr_factory_.GetWeakPtr(),
                     callback, std::move(result)));
}

void DataItem::Write(const std::vector<char>& data,
                     const WriteCallback& callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->GetExtensionById(
          extension_id_, ExtensionRegistry::ENABLED);
  if (!extension) {
    callback.Run(OperationResult::kUnknownExtension);
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache_),
                     base::Bind(&WriteImpl, result_ptr, id_, data, crypto_key_),
                     extension),
      base::BindOnce(&DataItem::OnWriteDone, weak_ptr_factory_.GetWeakPtr(),
                     callback, std::move(result)));
}

void DataItem::Read(const ReadCallback& callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->GetExtensionById(
          extension_id_, ExtensionRegistry::ENABLED);
  if (!extension) {
    callback.Run(OperationResult::kUnknownExtension, nullptr);
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  std::unique_ptr<std::vector<char>> data =
      std::make_unique<std::vector<char>>();
  std::vector<char>* data_ptr = data.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &ValueStoreCache::RunWithValueStoreForExtension,
          base::Unretained(value_store_cache_),
          base::Bind(&ReadImpl, result_ptr, data_ptr, id_, crypto_key_),
          extension),
      base::BindOnce(&DataItem::OnReadDone, weak_ptr_factory_.GetWeakPtr(),
                     callback, std::move(result), std::move(data)));
}

void DataItem::Delete(const WriteCallback& callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->GetExtensionById(
          extension_id_, ExtensionRegistry::ENABLED);
  if (!extension) {
    callback.Run(OperationResult::kUnknownExtension);
    return;
  }
  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache_),
                     base::Bind(&DeleteImpl, result_ptr, id_), extension),
      base::BindOnce(&DataItem::OnWriteDone, weak_ptr_factory_.GetWeakPtr(),
                     callback, std::move(result)));
}

void DataItem::OnWriteDone(const DataItem::WriteCallback& callback,
                           std::unique_ptr<OperationResult> success) {
  callback.Run(*success);
}

void DataItem::OnReadDone(const DataItem::ReadCallback& callback,
                          std::unique_ptr<OperationResult> success,
                          std::unique_ptr<std::vector<char>> data) {
  callback.Run(*success, std::move(data));
}

}  // namespace lock_screen_data
}  // namespace extensions

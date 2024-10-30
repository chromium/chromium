// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/lock_screen_data/data_item.h"

#include <utility>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/value_store/value_store.h"
#include "crypto/aes_cbc.h"
#include "extensions/browser/api/lock_screen_data/crypto.h"
#include "extensions/browser/api/lock_screen_data/operation_result.h"
#include "extensions/browser/api/storage/local_value_store_cache.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_id.h"

using value_store::ValueStore;

namespace extensions {
namespace lock_screen_data {

namespace {

// Key for the dictionary in the value store containing all items registered
// for the extension.
const char kStoreKeyRegisteredItems[] = "registered_items";

constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kFixedIv{
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

// Returns whether the value store |store| contains a registered item with ID
// |item_id|.
bool IsItemRegistered(ValueStore* store, const std::string& item_id) {
  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);
  if (!read.status().ok())
    return false;
  const base::Value::Dict* registered_items =
      read.settings().FindDict(kStoreKeyRegisteredItems);
  return registered_items && registered_items->Find(item_id);
}

// Gets a dictionary value that contains set of all registered data items from
// the values store |store|.
// |result| - the item fetch operation status code.
// |value| - on success, set to the dictionary containing registered data items.
//     Note that the dictionary will not contain data item content.
void GetRegisteredItems(OperationResult* result,
                        base::Value::Dict* dict,
                        ValueStore* store) {
  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);

  dict->clear();

  if (!read.status().ok()) {
    *result = OperationResult::kFailed;
    return;
  }

  // Using remove to pass ownership of registered_item dict to
  // |registered_items| (and avoid doing a copy |read.settings()|
  // sub-dictionary).
  std::optional<base::Value> registered_items =
      read.settings().Extract(kStoreKeyRegisteredItems);
  if (!registered_items) {
    // If the registered items dictionary cannot be found, assume no items have
    // yet been registered, and return empty result.
    *result = OperationResult::kSuccess;
    return;
  }

  if (!registered_items->is_dict()) {
    *result = OperationResult::kFailed;
    return;
  }

  *dict = std::move(registered_items->GetDict());
  *result = OperationResult::kSuccess;
}

// Registers a data item with ID |item_id| in value store |store|.
void RegisterItem(OperationResult* result,
                  const std::string& item_id,
                  ValueStore* store) {
  ValueStore::ReadResult read = store->Get(kStoreKeyRegisteredItems);

  if (!read.status().ok()) {
    *result = OperationResult::kFailed;
    return;
  }
  std::optional<base::Value> registered_items =
      read.settings().Extract(kStoreKeyRegisteredItems);
  if (!registered_items)
    registered_items = base::Value(base::Value::Type::DICT);

  if (!registered_items->is_dict()) {
    *result = OperationResult::kFailed;
    return;
  }

  base::Value::Dict dict = std::move(registered_items->GetDict());
  if (dict.Find(item_id)) {
    *result = OperationResult::kAlreadyRegistered;
    return;
  }

  dict.Set(item_id, base::Value::Dict());

  ValueStore::WriteResult write =
      store->Set(ValueStore::DEFAULTS, kStoreKeyRegisteredItems,
                 base::Value(std::move(dict)));
  *result = write.status().ok() ? OperationResult::kSuccess
                                : OperationResult::kFailed;
}

// Encrypts |data| with AES key |encryption_key| and saved it as |item_id|
// content to the value store |store|. The encrypted data is saved base64
// encoded.
void WriteImpl(OperationResult* result,
               const std::string item_id,
               const std::vector<char>& data,
               base::span<const uint8_t> key,
               ValueStore* store) {
  if (!IsItemRegistered(store, item_id)) {
    *result = OperationResult::kNotFound;
    return;
  }

  auto ciphertext =
      crypto::aes_cbc::Encrypt(key, kFixedIv, base::as_byte_span(data));
  ValueStore::WriteResult write =
      store->Set(ValueStore::DEFAULTS, item_id,
                 base::Value(base::Base64Encode(ciphertext)));

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
              base::span<const uint8_t> key,
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

  const base::Value* item = read.settings().Find(item_id);
  if (!item) {
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

  auto plaintext = crypto::aes_cbc::Decrypt(
      key, kFixedIv, base::as_byte_span(read_data) /*XXX*/);
  if (plaintext.has_value()) {
    std::copy(plaintext->begin(), plaintext->end(), std::back_inserter(*data));
    *result = OperationResult::kSuccess;
  } else {
    *result = OperationResult::kWrongKey;
  }
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

  base::Value::Dict* registered_items =
      read.settings().FindDict(kStoreKeyRegisteredItems);
  if (!registered_items || !registered_items->Remove(item_id)) {
    *result = OperationResult::kNotFound;
    return;
  }

  ValueStore::WriteResult write =
      store->Set(ValueStore::DEFAULTS, kStoreKeyRegisteredItems,
                 base::Value(std::move(*registered_items)));
  *result = write.status().ok() ? OperationResult::kSuccess
                                : OperationResult::kFailed;
}

void OnGetRegisteredValues(DataItem::RegisteredValuesCallback callback,
                           std::unique_ptr<OperationResult> result,
                           std::unique_ptr<base::Value::Dict> dict) {
  std::move(callback).Run(*result, std::move(*dict));
}

}  // namespace

// static
void DataItem::GetRegisteredValuesForExtension(
    content::BrowserContext* context,
    ValueStoreCache* value_store_cache,
    base::SequencedTaskRunner* task_runner,
    const ExtensionId& extension_id,
    RegisteredValuesCallback callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    std::move(callback).Run(OperationResult::kUnknownExtension,
                            base::Value::Dict());
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();
  std::unique_ptr<base::Value::Dict> dict =
      std::make_unique<base::Value::Dict>();
  base::Value::Dict* dict_ptr = dict.get();

  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache),
                     base::BindOnce(&GetRegisteredItems, result_ptr, dict_ptr),
                     extension),
      base::BindOnce(&OnGetRegisteredValues, std::move(callback),
                     std::move(result), std::move(dict)));
}

// static
void DataItem::DeleteAllItemsForExtension(
    content::BrowserContext* context,
    ValueStoreCache* value_store_cache,
    base::SequencedTaskRunner* task_runner,
    const ExtensionId& extension_id,
    base::OnceClosure callback) {
  task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::DeleteStorageSoon,
                     base::Unretained(value_store_cache), extension_id),
      std::move(callback));
}

DataItem::DataItem(const std::string& id,
                   const ExtensionId& extension_id,
                   content::BrowserContext* context,
                   ValueStoreCache* value_store_cache,
                   base::SequencedTaskRunner* task_runner,
                   const std::string& key)
    : id_(id),
      extension_id_(extension_id),
      context_(context),
      value_store_cache_(value_store_cache),
      task_runner_(task_runner) {
  base::span(crypto_key_).copy_from(base::as_byte_span(key));
}

DataItem::~DataItem() = default;

void DataItem::Register(WriteCallback callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->enabled_extensions().GetByID(
          extension_id_);
  if (!extension) {
    std::move(callback).Run(OperationResult::kUnknownExtension);
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache_),
                     base::BindOnce(&RegisterItem, result_ptr, id()),
                     extension),
      base::BindOnce(&DataItem::OnWriteDone, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), std::move(result)));
}

void DataItem::Write(const std::vector<char>& data, WriteCallback callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->enabled_extensions().GetByID(
          extension_id_);
  if (!extension) {
    std::move(callback).Run(OperationResult::kUnknownExtension);
    return;
  }

  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &ValueStoreCache::RunWithValueStoreForExtension,
          base::Unretained(value_store_cache_),
          base::BindOnce(&WriteImpl, result_ptr, id_, data, crypto_key_),
          extension),
      base::BindOnce(&DataItem::OnWriteDone, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), std::move(result)));
}

void DataItem::Read(ReadCallback callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->enabled_extensions().GetByID(
          extension_id_);
  if (!extension) {
    std::move(callback).Run(OperationResult::kUnknownExtension, nullptr);
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
          base::BindOnce(&ReadImpl, result_ptr, data_ptr, id_, crypto_key_),
          extension),
      base::BindOnce(&DataItem::OnReadDone, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), std::move(result), std::move(data)));
}

void DataItem::Delete(WriteCallback callback) {
  scoped_refptr<const Extension> extension =
      ExtensionRegistry::Get(context_)->enabled_extensions().GetByID(
          extension_id_);
  if (!extension) {
    std::move(callback).Run(OperationResult::kUnknownExtension);
    return;
  }
  std::unique_ptr<OperationResult> result =
      std::make_unique<OperationResult>(OperationResult::kFailed);
  OperationResult* result_ptr = result.get();

  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ValueStoreCache::RunWithValueStoreForExtension,
                     base::Unretained(value_store_cache_),
                     base::BindOnce(&DeleteImpl, result_ptr, id_), extension),
      base::BindOnce(&DataItem::OnWriteDone, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback), std::move(result)));
}

void DataItem::OnWriteDone(WriteCallback callback,
                           std::unique_ptr<OperationResult> success) {
  std::move(callback).Run(*success);
}

void DataItem::OnReadDone(ReadCallback callback,
                          std::unique_ptr<OperationResult> success,
                          std::unique_ptr<std::vector<char>> data) {
  std::move(callback).Run(*success, std::move(data));
}

}  // namespace lock_screen_data
}  // namespace extensions

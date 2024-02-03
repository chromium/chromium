// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_DATA_ITEM_H_
#define EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_DATA_ITEM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace extensions {

class ValueStoreCache;

namespace lock_screen_data {

enum class OperationResult;

// Wrapper around a per extension value store backed lock screen data item.
class DataItem {
 public:
  using WriteCallback = base::OnceCallback<void(OperationResult result)>;
  using ReadCallback =
      base::OnceCallback<void(OperationResult result,
                              std::unique_ptr<std::vector<char>> data)>;
  using RegisteredValuesCallback =
      base::OnceCallback<void(OperationResult result,
                              base::Value::Dict values)>;

  // Gets all registered data items for the extension with the provided
  // extension ID - the items are returned as a Value::Dict with keys set
  // to data item IDs.
  static void GetRegisteredValuesForExtension(
      content::BrowserContext* context,
      ValueStoreCache* value_store_cache,
      base::SequencedTaskRunner* task_runner,
      const ExtensionId& extension_id,
      RegisteredValuesCallback callback);

  // Clears data item value store for the extension with the provided extension
  // ID.
  static void DeleteAllItemsForExtension(content::BrowserContext* context,
                                         ValueStoreCache* value_store_cache,
                                         base::SequencedTaskRunner* task_runner,
                                         const ExtensionId& extension_id,
                                         base::OnceClosure callback);

  // |id| - Data item ID.
  // |extension_id| - The extension that owns the item.
  // |context| - The browser context to which the owning extension is attached.
  // |value_store_cache| - Used to retrieve value store for the extension with
  //     |extension_id| ID. Not owned - the caller should ensure the value store
  //     cache outlives the data item. Note that DataItem post tasks with
  //     unretained |value_store_cache| to |task_runner|, which means any usage
  //     of DataItem should happen before value store cache deletion is
  ///    scheduled to |task_runner|.
  // |task_runner| - The task runner on which value store should be used. Note
  //     that the Data item does not retain a reference to the task runner -
  //     the caller should ensure |task_runner| outlives the data item.
  // |crypto_key| - Symmetric AES key for encrypting/decrypting data item
  //     content.
  DataItem(const std::string& id,
           const ExtensionId& extension_id,
           content::BrowserContext* context,
           ValueStoreCache* value_store_cache,
           base::SequencedTaskRunner* task_runner,
           const std::string& crypto_key);

  DataItem(const DataItem&) = delete;
  DataItem& operator=(const DataItem&) = delete;

  virtual ~DataItem();

  // Registers the data item in the persistent data item storage.
  virtual void Register(WriteCallback callback);

  // Sets the data item content, saving it to persistent data storage.
  // This will fail if the data item is not registered.
  virtual void Write(const std::vector<char>& data, WriteCallback callback);

  // Gets the data item content from the persistent data storage.
  // This will fail is the item is not registered.
  virtual void Read(ReadCallback callback);

  // Unregisters the data item, and clears previously persisted data item
  // content.
  virtual void Delete(WriteCallback callback);

  const std::string& id() const { return id_; }

  const ExtensionId& extension_id() const { return extension_id_; }

 private:
  // Internal callback for write operations - wraps |callback| to ensure
  // |callback| is not run after |this| has been destroyed.
  void OnWriteDone(WriteCallback callback,
                   std::unique_ptr<OperationResult> result);

  // Internal callback for the read operation - wraps |callback| to ensure
  // |callback| is not run after |this| has been destroyed.
  void OnReadDone(ReadCallback callback,
                  std::unique_ptr<OperationResult> result,
                  std::unique_ptr<std::vector<char>> data);

  // The data item ID.
  std::string id_;

  // The ID of the extension that owns the data item.
  ExtensionId extension_id_;

  raw_ptr<content::BrowserContext> context_;

  // Cache used to retrieve the values store to which the data item should be
  // saved - the value stores are mapped by the extension ID.
  raw_ptr<ValueStoreCache> value_store_cache_;

  // Task runner on which value store should be accessed.
  raw_ptr<base::SequencedTaskRunner> task_runner_;

  // They symmetric AES key that should be used to encrypt data item content
  // when the content is written to the storage, and to decrypt item content
  // when reading it from the storage.
  const std::string crypto_key_;

  base::WeakPtrFactory<DataItem> weak_ptr_factory_{this};
};

}  // namespace lock_screen_data
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_LOCK_SCREEN_DATA_DATA_ITEM_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

namespace content {

class IndexedDBBackingStore;
struct IndexedDBPendingConnection;

// TODO(dmurph): Remove this interface.
class CONTENT_EXPORT IndexedDBFactory {
 public:
  IndexedDBFactory(const IndexedDBFactory&) = delete;
  IndexedDBFactory& operator=(const IndexedDBFactory&) = delete;

  virtual ~IndexedDBFactory() = default;

  virtual void GetDatabaseInfo(scoped_refptr<IndexedDBCallbacks> callbacks,
                               const blink::StorageKey& storage_key,
                               const base::FilePath& data_directory) = 0;
  virtual void Open(const std::u16string& name,
                    std::unique_ptr<IndexedDBPendingConnection> connection,
                    const blink::StorageKey& storage_key,
                    const base::FilePath& data_directory) = 0;

  virtual void DeleteDatabase(const std::u16string& name,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              const blink::StorageKey& storage_key,
                              const base::FilePath& data_directory,
                              bool force_close) = 0;

  virtual void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const blink::StorageKey& storage_key) = 0;
  virtual void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const blink::StorageKey& storage_key) = 0;

  virtual void HandleBackingStoreFailure(
      const blink::StorageKey& storage_key) = 0;
  virtual void HandleBackingStoreCorruption(
      const blink::StorageKey& storage_key,
      const IndexedDBDatabaseError& error) = 0;

  virtual std::vector<IndexedDBDatabase*> GetOpenDatabasesForStorageKey(
      const blink::StorageKey& storage_key) const = 0;

  // Close all connections to all databases within the storage key. If
  // |delete_in_memory_store| is true, references to in-memory databases will be
  // dropped thereby allowing their deletion (otherwise they are retained for
  // the lifetime of the factory).
  virtual void ForceClose(const blink::StorageKey& storage_key,
                          bool delete_in_memory_store = false) = 0;

  virtual void ForceSchemaDowngrade(const blink::StorageKey& storage_key) = 0;
  virtual V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const blink::StorageKey& storage_key) = 0;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  virtual void ContextDestroyed() = 0;

  // Called by the IndexedDBActiveBlobRegistry.
  virtual void ReportOutstandingBlobs(const blink::StorageKey& storage_key,
                                      bool blobs_outstanding) = 0;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  virtual void BlobFilesCleaned(const blink::StorageKey& storage_key) = 0;

  virtual size_t GetConnectionCount(
      const blink::StorageKey& storage_key) const = 0;

  virtual int64_t GetInMemoryDBSize(
      const blink::StorageKey& storage_key) const = 0;

  virtual base::Time GetLastModified(
      const blink::StorageKey& storage_key) const = 0;

  virtual void NotifyIndexedDBContentChanged(
      const blink::StorageKey& storage_key,
      const std::u16string& database_name,
      const std::u16string& object_store_name) = 0;

 protected:
  IndexedDBFactory() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

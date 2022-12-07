// Copyright 2013 The Chromium Authors
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

namespace storage {
struct BucketLocator;
}  // namespace storage

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
                               const storage::BucketLocator& bucket_locator,
                               const base::FilePath& data_directory) = 0;
  virtual void Open(const std::u16string& name,
                    std::unique_ptr<IndexedDBPendingConnection> connection,
                    const storage::BucketLocator& bucket_locator,
                    const base::FilePath& data_directory) = 0;

  virtual void DeleteDatabase(const std::u16string& name,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              const storage::BucketLocator& bucket_locator,
                              const base::FilePath& data_directory,
                              bool force_close) = 0;

  virtual void HandleBackingStoreFailure(
      const storage::BucketLocator& bucket_locator) = 0;
  virtual void HandleBackingStoreCorruption(
      const storage::BucketLocator& bucket_locator,
      const IndexedDBDatabaseError& error) = 0;

  virtual std::vector<IndexedDBDatabase*> GetOpenDatabasesForBucket(
      const storage::BucketLocator& bucket_locator) const = 0;

  // Close all connections to all databases within the bucket. If
  // `delete_in_memory_store` is true, references to in-memory databases will be
  // dropped thereby allowing their deletion (otherwise they are retained for
  // the lifetime of the factory).
  virtual void ForceClose(storage::BucketId bucket_id,
                          bool delete_in_memory_store = false) = 0;

  virtual void ForceSchemaDowngrade(
      const storage::BucketLocator& bucket_locator) = 0;
  virtual V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const storage::BucketLocator& bucket_locator) = 0;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  virtual void ContextDestroyed() = 0;

  // Called by the IndexedDBActiveBlobRegistry.
  virtual void ReportOutstandingBlobs(
      const storage::BucketLocator& bucket_locator,
      bool blobs_outstanding) = 0;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  virtual void BlobFilesCleaned(
      const storage::BucketLocator& bucket_locator) = 0;

  virtual size_t GetConnectionCount(storage::BucketId bucket_id) const = 0;

  virtual int64_t GetInMemoryDBSize(
      const storage::BucketLocator& bucket_locator) const = 0;

  virtual base::Time GetLastModified(
      const storage::BucketLocator& bucket_locator) const = 0;

  virtual void NotifyIndexedDBContentChanged(
      const storage::BucketLocator& bucket_locator,
      const std::u16string& database_name,
      const std::u16string& object_store_name) = 0;

 protected:
  IndexedDBFactory() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class IndexedDBBackingStore;
struct IndexedDBPendingConnection;

// TODO(dmurph): Remove this interface.
class CONTENT_EXPORT IndexedDBFactory {
 public:
  virtual ~IndexedDBFactory() = default;

  using OriginDBMap =
      base::flat_map<base::string16, std::unique_ptr<IndexedDBDatabase>>;

  virtual void GetDatabaseInfo(scoped_refptr<IndexedDBCallbacks> callbacks,
                               const url::Origin& origin,
                               const base::FilePath& data_directory) = 0;
  virtual void GetDatabaseNames(scoped_refptr<IndexedDBCallbacks> callbacks,
                                const url::Origin& origin,
                                const base::FilePath& data_directory) = 0;
  virtual void Open(
      const base::string16& name,
      std::unique_ptr<IndexedDBPendingConnection> connection,
      const url::Origin& origin,
      const base::FilePath& data_directory) = 0;

  virtual void DeleteDatabase(
      const base::string16& name,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      const url::Origin& origin,
      const base::FilePath& data_directory,
      bool force_close) = 0;

  virtual void AbortTransactionsAndCompactDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) = 0;
  virtual void AbortTransactionsForDatabase(
      base::OnceCallback<void(leveldb::Status)> callback,
      const url::Origin& origin) = 0;

  virtual void HandleBackingStoreFailure(const url::Origin& origin) = 0;
  virtual void HandleBackingStoreCorruption(
      const url::Origin& origin,
      const IndexedDBDatabaseError& error) = 0;

  virtual std::vector<IndexedDBDatabase*> GetOpenDatabasesForOrigin(
      const url::Origin& origin) const = 0;

  // Close all connections to all databases within the origin. If
  // |delete_in_memory_store| is true, references to in-memory databases will be
  // dropped thereby allowing their deletion (otherwise they are retained for
  // the lifetime of the factory).
  virtual void ForceClose(const url::Origin& origin,
                          bool delete_in_memory_store = false) = 0;

  virtual void ForceSchemaDowngrade(const url::Origin& origin) = 0;
  virtual V2SchemaCorruptionStatus HasV2SchemaCorruption(
      const url::Origin& origin) = 0;

  // Called by the IndexedDBContext destructor so the factory can do cleanup.
  virtual void ContextDestroyed() = 0;

  // Called by the IndexedDBActiveBlobRegistry.
  virtual void ReportOutstandingBlobs(const url::Origin& origin,
                                      bool blobs_outstanding) = 0;

  // Called by IndexedDBBackingStore when blob files have been cleaned.
  virtual void BlobFilesCleaned(const url::Origin& origin) = 0;

  virtual size_t GetConnectionCount(const url::Origin& origin) const = 0;

  virtual int64_t GetInMemoryDBSize(const url::Origin& origin) const = 0;

  virtual base::Time GetLastModified(const url::Origin& origin) const = 0;

  virtual void NotifyIndexedDBContentChanged(
      const url::Origin& origin,
      const base::string16& database_name,
      const base::string16& object_store_name) = 0;

 protected:
  IndexedDBFactory() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

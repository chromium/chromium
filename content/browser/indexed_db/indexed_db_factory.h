// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

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
struct IndexedDBDataLossInfo;

class CONTENT_EXPORT IndexedDBFactory
    : public base::RefCountedThreadSafe<IndexedDBFactory> {
 public:
  typedef std::multimap<url::Origin, IndexedDBDatabase*> OriginDBMap;
  typedef OriginDBMap::const_iterator OriginDBMapIterator;
  typedef std::pair<OriginDBMapIterator, OriginDBMapIterator> OriginDBs;

  virtual void ReleaseDatabase(const IndexedDBDatabase::Identifier& identifier,
                               bool forced_close) = 0;
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

  virtual OriginDBs GetOpenDatabasesForOrigin(
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

  // Called by an IndexedDBDatabase when it is actually deleted.
  virtual void DatabaseDeleted(
      const IndexedDBDatabase::Identifier& identifier) = 0;

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
  friend class base::RefCountedThreadSafe<IndexedDBFactory>;

  IndexedDBFactory() {}
  virtual ~IndexedDBFactory() {}

  virtual scoped_refptr<IndexedDBBackingStore> OpenBackingStore(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      leveldb::Status* status) = 0;

  virtual scoped_refptr<IndexedDBBackingStore> OpenBackingStoreHelper(
      const url::Origin& origin,
      const base::FilePath& data_directory,
      IndexedDBDataLossInfo* data_loss_info,
      bool* disk_full,
      bool first_time,
      leveldb::Status* status) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_FACTORY_H_

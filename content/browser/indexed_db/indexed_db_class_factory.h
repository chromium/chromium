// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/services/storage/indexed_db/scopes/scopes_lock_manager.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class IndexedDBBackingStore;
class IndexedDBConnection;
class IndexedDBFactory;
class IndexedDBTransaction;
class LevelDBFactory;
class TransactionalLevelDBFactory;

// Use this factory to create some IndexedDB objects. Exists solely to
// facilitate tests which sometimes need to inject mock objects into the system.
// TODO(dmurph): Remove this class in favor of dependency injection. This makes
// it really hard to iterate on the system.
class CONTENT_EXPORT IndexedDBClassFactory {
 public:
  typedef IndexedDBClassFactory* GetterCallback();
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback =
      base::RepeatingCallback<void(leveldb::Status, const char*)>;

  static IndexedDBClassFactory* Get();

  static void SetIndexedDBClassFactoryGetter(GetterCallback* cb);

  // Visible for testing.
  static leveldb_env::Options GetLevelDBOptions();

  virtual LevelDBFactory& leveldb_factory();
  virtual TransactionalLevelDBFactory& transactional_leveldb_factory();

  // Returns a constructed database, or a leveldb::Status error if there was a
  // problem initializing the database. |run_tasks_callback| is called when the
  // database has tasks to run.
  virtual std::pair<std::unique_ptr<IndexedDBDatabase>, leveldb::Status>
  CreateIndexedDBDatabase(
      const base::string16& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      TasksAvailableCallback tasks_available_callback,
      std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
      const IndexedDBDatabase::Identifier& unique_identifier,
      ScopesLockManager* transaction_lock_manager);

  // |tasks_available_callback| is called when the transaction has tasks to run.
  virtual std::unique_ptr<IndexedDBTransaction> CreateIndexedDBTransaction(
      int64_t id,
      IndexedDBConnection* connection,
      const std::set<int64_t>& scope,
      blink::mojom::IDBTransactionMode mode,
      TasksAvailableCallback tasks_available_callback,
      IndexedDBTransaction::TearDownCallback tear_down_callback,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  void SetLevelDBFactoryForTesting(LevelDBFactory* leveldb_factory);

 protected:
  IndexedDBClassFactory();
  IndexedDBClassFactory(
      LevelDBFactory* leveldb_factory,
      TransactionalLevelDBFactory* transactional_leveldb_factory);
  virtual ~IndexedDBClassFactory() = default;
  friend struct base::LazyInstanceTraitsBase<IndexedDBClassFactory>;

  LevelDBFactory* leveldb_factory_;
  TransactionalLevelDBFactory* transactional_leveldb_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

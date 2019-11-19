// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "components/services/storage/indexed_db/scopes/scopes_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"

namespace content {

class IndexedDBConnection;
class IndexedDBMetadataCoding;
class LevelDBDirectTransaction;
class LevelDBScope;
class LevelDBScopes;
class LevelDBSnapshot;
class TransactionalLevelDBTransaction;
class TransactionalLevelDBDatabase;

enum FailClass {
  FAIL_CLASS_NOTHING,
  FAIL_CLASS_LEVELDB_ITERATOR,
  FAIL_CLASS_LEVELDB_DIRECT_TRANSACTION,
  FAIL_CLASS_LEVELDB_TRANSACTION,
  FAIL_CLASS_LEVELDB_DATABASE,
};

enum FailMethod {
  FAIL_METHOD_NOTHING,
  FAIL_METHOD_COMMIT,
  FAIL_METHOD_COMMIT_DISK_FULL,
  FAIL_METHOD_GET,
  FAIL_METHOD_SEEK,
  FAIL_METHOD_WRITE,
};

class MockBrowserTestIndexedDBClassFactory
    : public IndexedDBClassFactory,
      public DefaultTransactionalLevelDBFactory {
 public:
  MockBrowserTestIndexedDBClassFactory();
  ~MockBrowserTestIndexedDBClassFactory() override;

  TransactionalLevelDBFactory& transactional_leveldb_factory() override;

  std::pair<std::unique_ptr<IndexedDBDatabase>, leveldb::Status>
  CreateIndexedDBDatabase(
      const base::string16& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      TasksAvailableCallback tasks_available_callback,
      std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
      const IndexedDBDatabase::Identifier& unique_identifier,
      ScopesLockManager* transaction_lock_manager) override;
  std::unique_ptr<IndexedDBTransaction> CreateIndexedDBTransaction(
      int64_t id,
      IndexedDBConnection* connection,
      const std::set<int64_t>& scope,
      blink::mojom::IDBTransactionMode mode,
      TasksAvailableCallback tasks_available_callback,
      IndexedDBTransaction::TearDownCallback tear_down_callback,
      IndexedDBBackingStore::Transaction* backing_store_transaction) override;

  std::unique_ptr<TransactionalLevelDBDatabase> CreateLevelDBDatabase(
      scoped_refptr<LevelDBState> state,
      std::unique_ptr<LevelDBScopes> scopes,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      size_t max_open_iterators) override;
  std::unique_ptr<LevelDBDirectTransaction> CreateLevelDBDirectTransaction(
      TransactionalLevelDBDatabase* db) override;
  scoped_refptr<TransactionalLevelDBTransaction> CreateLevelDBTransaction(
      TransactionalLevelDBDatabase* db,
      std::unique_ptr<LevelDBScope> scope) override;
  std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
      std::unique_ptr<leveldb::Iterator> it,
      base::WeakPtr<TransactionalLevelDBDatabase> db,
      base::WeakPtr<TransactionalLevelDBTransaction> txn,
      std::unique_ptr<LevelDBSnapshot> snapshot) override;

  void FailOperation(FailClass failure_class,
                     FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num);
  void Reset();

 private:
  FailClass failure_class_;
  FailMethod failure_method_;
  std::map<FailClass, int> instance_count_;
  std::map<FailClass, int> fail_on_instance_num_;
  std::map<FailClass, int> fail_on_call_num_;
  bool only_trace_calls_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

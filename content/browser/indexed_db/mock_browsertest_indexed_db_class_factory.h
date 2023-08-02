// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_task_helper.h"

namespace content {

class IndexedDBConnection;
class LevelDBDirectTransaction;
class LevelDBScope;
class LevelDBScopes;
class LevelDBSnapshot;
class TransactionalLevelDBTransaction;
class TransactionalLevelDBDatabase;

class MockBrowserTestIndexedDBClassFactory
    : public IndexedDBClassFactory,
      public DefaultTransactionalLevelDBFactory,
      public storage::mojom::MockFailureInjector {
 public:
  MockBrowserTestIndexedDBClassFactory();
  ~MockBrowserTestIndexedDBClassFactory() override;

  TransactionalLevelDBFactory& transactional_leveldb_factory() override;

  std::pair<std::unique_ptr<IndexedDBDatabase>, leveldb::Status>
  CreateIndexedDBDatabase(
      const std::u16string& name,
      IndexedDBBackingStore* backing_store,
      IndexedDBFactory* factory,
      TasksAvailableCallback tasks_available_callback,
      const IndexedDBDatabase::Identifier& unique_identifier,
      PartitionedLockManager* transaction_lock_manager) override;
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

  void FailOperation(storage::mojom::FailClass failure_class,
                     storage::mojom::FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num,
                     base::OnceClosure callback) override;
  void Reset();

 private:
  storage::mojom::FailClass failure_class_;
  storage::mojom::FailMethod failure_method_;
  std::map<storage::mojom::FailClass, int> instance_count_;
  std::map<storage::mojom::FailClass, int> fail_on_instance_num_;
  std::map<storage::mojom::FailClass, int> fail_on_call_num_;
  bool only_trace_calls_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

#include <map>
#include <memory>
#include <set>

#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content::indexed_db {

class LevelDBDirectTransaction;
class LevelDBScope;
class LevelDBScopes;
class LevelDBSnapshot;
class TransactionalLevelDBTransaction;
class TransactionalLevelDBDatabase;

class MockBrowserTestIndexedDBClassFactory
    : public DefaultTransactionalLevelDBFactory,
      public storage::mojom::MockFailureInjector {
 public:
  explicit MockBrowserTestIndexedDBClassFactory(
      mojo::PendingReceiver<storage::mojom::MockFailureInjector> pending);
  ~MockBrowserTestIndexedDBClassFactory() override;

  // DefaultTransactionalLevelDBFactory:
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

  // storage::mojom::MockFailureInjector:
  void FailOperation(storage::mojom::FailClass failure_class,
                     storage::mojom::FailMethod failure_method,
                     int fail_on_instance_num,
                     int fail_on_call_num) override;

 private:
  storage::mojom::FailClass failure_class_;
  storage::mojom::FailMethod failure_method_;
  std::map<storage::mojom::FailClass, int> instance_count_;
  std::map<storage::mojom::FailClass, int> fail_on_instance_num_;
  std::map<storage::mojom::FailClass, int> fail_on_call_num_;
  bool only_trace_calls_ = false;

  mojo::Receiver<storage::mojom::MockFailureInjector> receiver_{this};
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_MOCK_BROWSERTEST_INDEXED_DB_CLASS_FACTORY_H_

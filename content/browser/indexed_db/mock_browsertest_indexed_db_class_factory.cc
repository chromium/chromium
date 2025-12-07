// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_browsertest_indexed_db_class_factory.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_iterator.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_transaction.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

using storage::mojom::FailClass;
using storage::mojom::FailMethod;

namespace {

class FunctionTracer {
 public:
  FunctionTracer(const std::string& class_name,
                 const std::string& method_name,
                 int instance_num)
      : class_name_(class_name),
        method_name_(method_name),
        instance_count_(instance_num),
        current_call_num_(0) {}

  void log_call() {
    current_call_num_++;
    VLOG(0) << class_name_ << '[' << instance_count_ << "]::" << method_name_
            << "()[" << current_call_num_ << ']';
  }

 private:
  std::string class_name_;
  std::string method_name_;
  int instance_count_;
  int current_call_num_;
};

}  // namespace

namespace content::indexed_db {

class LevelDBTestDatabase : public TransactionalLevelDBDatabase {
 public:
  LevelDBTestDatabase(scoped_refptr<LevelDBState> level_db_state,
                      std::unique_ptr<LevelDBScopes> leveldb_scopes,
                      TransactionalLevelDBFactory* factory,
                      scoped_refptr<base::SequencedTaskRunner> task_runner,
                      size_t max_open_iterators,
                      FailMethod fail_method,
                      int fail_on_call_num)
      : TransactionalLevelDBDatabase(std::move(level_db_state),
                                     std::move(leveldb_scopes),
                                     factory,
                                     std::move(task_runner),
                                     max_open_iterators),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {
    DCHECK(fail_method != FailMethod::NOTHING);
    DCHECK_GT(fail_on_call_num, 0);
  }
  ~LevelDBTestDatabase() override = default;

  leveldb::Status Get(std::string_view key,
                      std::string* value,
                      bool* found) override {
    if (fail_method_ != FailMethod::GET ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBDatabase::Get(key, value, found);
    *found = false;
    return leveldb::Status::Corruption("Corrupted for the test");
  }

  leveldb::Status Write(LevelDBWriteBatch* write_batch) override {
    if ((fail_method_ != FailMethod::WRITE) ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBDatabase::Write(write_batch);
    return leveldb::Status::Corruption("Corrupted for the test");
  }

 private:
  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

class LevelDBTestDirectTransaction : public LevelDBDirectTransaction {
 public:
  LevelDBTestDirectTransaction(TransactionalLevelDBDatabase* db,
                               FailMethod fail_method,
                               int fail_on_call_num)
      : LevelDBDirectTransaction(db),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {
    DCHECK(fail_method != FailMethod::NOTHING);
    DCHECK_GT(fail_on_call_num, 0);
  }
  ~LevelDBTestDirectTransaction() override = default;

  leveldb::Status Get(std::string_view key,
                      std::string* value,
                      bool* found) override {
    if (fail_method_ != FailMethod::GET ||
        ++current_call_num_ != fail_on_call_num_)
      return LevelDBTestDirectTransaction::Get(key, value, found);

    *found = false;
    return leveldb::Status::Corruption("Corrupted for the test");
  }

 private:
  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

class LevelDBTestTransaction : public TransactionalLevelDBTransaction {
 public:
  LevelDBTestTransaction(TransactionalLevelDBDatabase* db,
                         std::unique_ptr<LevelDBScope> scope,
                         FailMethod fail_method,
                         int fail_on_call_num)
      : TransactionalLevelDBTransaction(db, std::move(scope)),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {
    DCHECK(fail_method != FailMethod::NOTHING);
    DCHECK_GT(fail_on_call_num, 0);
  }

  leveldb::Status Get(std::string_view key,
                      std::string* value,
                      bool* found) override {
    if (fail_method_ != FailMethod::GET ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBTransaction::Get(key, value, found);

    *found = false;
    return leveldb::Status::Corruption("Corrupted for the test");
  }

  leveldb::Status Commit(bool sync_on_commit) override {
    if (++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBTransaction::Commit(sync_on_commit);

    if (fail_method_ == FailMethod::COMMIT)
      return leveldb::Status::Corruption("Corrupted for the test");

    if (fail_method_ == FailMethod::COMMIT_DISK_FULL) {
      return leveldb_env::MakeIOError("dummy filename", "Disk Full",
                                      leveldb_env::kWritableFileAppend,
                                      base::File::FILE_ERROR_NO_SPACE);
    }

    if (fail_method_ == FailMethod::COMMIT_SYNC && sync_on_commit) {
      return leveldb_env::MakeIOError("dummy filename", "Sync on commit",
                                      leveldb_env::kWritableFileAppend,
                                      base::File::FILE_ERROR_FAILED);
    }

    return TransactionalLevelDBTransaction::Commit(sync_on_commit);
  }

 private:
  ~LevelDBTestTransaction() override = default;

  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

class LevelDBTraceTransaction : public TransactionalLevelDBTransaction {
 public:
  LevelDBTraceTransaction(TransactionalLevelDBDatabase* db,
                          std::unique_ptr<LevelDBScope> scope,
                          int tx_num)
      : TransactionalLevelDBTransaction(db, std::move(scope)),
        class_name_("LevelDBTransaction"),
        commit_tracer_(class_name_, "Commit", tx_num),
        get_tracer_(class_name_, "Get", tx_num) {}

  leveldb::Status Get(std::string_view key,
                      std::string* value,
                      bool* found) override {
    get_tracer_.log_call();
    return TransactionalLevelDBTransaction::Get(key, value, found);
  }

  leveldb::Status Commit(bool sync_on_commit) override {
    commit_tracer_.log_call();
    return TransactionalLevelDBTransaction::Commit(sync_on_commit);
  }

 private:
  const std::string class_name_;

  ~LevelDBTraceTransaction() override = default;

  FunctionTracer commit_tracer_;
  FunctionTracer get_tracer_;
};

class LevelDBTraceIterator : public TransactionalLevelDBIterator {
 public:
  LevelDBTraceIterator(std::unique_ptr<leveldb::Iterator> iterator,
                       base::WeakPtr<TransactionalLevelDBDatabase> db,
                       base::WeakPtr<TransactionalLevelDBTransaction> txn,
                       std::unique_ptr<LevelDBSnapshot> snapshot,
                       int inst_num)
      : TransactionalLevelDBIterator(std::move(iterator),
                                     std::move(db),
                                     std::move(txn),
                                     std::move(snapshot)),
        class_name_("LevelDBIterator"),
        is_valid_tracer_(class_name_, "IsValid", inst_num),
        seek_to_last_tracer_(class_name_, "SeekToLast", inst_num),
        seek_tracer_(class_name_, "Seek", inst_num),
        next_tracer_(class_name_, "Next", inst_num),
        prev_tracer_(class_name_, "Prev", inst_num),
        key_tracer_(class_name_, "Key", inst_num),
        value_tracer_(class_name_, "Value", inst_num) {}
  ~LevelDBTraceIterator() override = default;

 private:
  const std::string class_name_;

  bool IsValid() const override {
    is_valid_tracer_.log_call();
    return TransactionalLevelDBIterator::IsValid();
  }
  leveldb::Status SeekToLast() override {
    seek_to_last_tracer_.log_call();
    return TransactionalLevelDBIterator::SeekToLast();
  }
  leveldb::Status Seek(std::string_view target) override {
    seek_tracer_.log_call();
    return TransactionalLevelDBIterator::Seek(target);
  }
  leveldb::Status Next() override {
    next_tracer_.log_call();
    return TransactionalLevelDBIterator::Next();
  }
  leveldb::Status Prev() override {
    prev_tracer_.log_call();
    return TransactionalLevelDBIterator::Prev();
  }
  std::string_view Key() const override {
    key_tracer_.log_call();
    return TransactionalLevelDBIterator::Key();
  }
  std::string_view Value() const override {
    value_tracer_.log_call();
    return TransactionalLevelDBIterator::Value();
  }

  mutable FunctionTracer is_valid_tracer_;
  mutable FunctionTracer seek_to_last_tracer_;
  mutable FunctionTracer seek_tracer_;
  mutable FunctionTracer next_tracer_;
  mutable FunctionTracer prev_tracer_;
  mutable FunctionTracer key_tracer_;
  mutable FunctionTracer value_tracer_;
};

class LevelDBTestIterator : public TransactionalLevelDBIterator {
 public:
  LevelDBTestIterator(std::unique_ptr<leveldb::Iterator> iterator,
                      base::WeakPtr<TransactionalLevelDBDatabase> db,
                      base::WeakPtr<TransactionalLevelDBTransaction> txn,
                      std::unique_ptr<LevelDBSnapshot> snapshot,
                      FailMethod fail_method,
                      int fail_on_call_num)
      : TransactionalLevelDBIterator(std::move(iterator),
                                     std::move(db),
                                     std::move(txn),
                                     std::move(snapshot)),
        fail_method_(fail_method),
        fail_on_call_num_(fail_on_call_num),
        current_call_num_(0) {}
  ~LevelDBTestIterator() override = default;

 private:
  leveldb::Status Seek(std::string_view target) override {
    if (fail_method_ != FailMethod::SEEK ||
        ++current_call_num_ != fail_on_call_num_)
      return TransactionalLevelDBIterator::Seek(target);
    return leveldb::Status::Corruption("Corrupted for test");
  }

  FailMethod fail_method_;
  int fail_on_call_num_;
  int current_call_num_;
};

MockBrowserTestIndexedDBClassFactory::MockBrowserTestIndexedDBClassFactory(
    mojo::PendingReceiver<storage::mojom::MockFailureInjector> pending)
    : failure_class_(FailClass::NOTHING), failure_method_(FailMethod::NOTHING) {
  receiver_.Bind(std::move(pending));
}

MockBrowserTestIndexedDBClassFactory::~MockBrowserTestIndexedDBClassFactory() =
    default;

std::unique_ptr<TransactionalLevelDBDatabase>
MockBrowserTestIndexedDBClassFactory::CreateLevelDBDatabase(
    scoped_refptr<LevelDBState> state,
    std::unique_ptr<LevelDBScopes> scopes,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    size_t max_open_iterators) {
  instance_count_[FailClass::LEVELDB_DATABASE] =
      instance_count_[FailClass::LEVELDB_DATABASE] + 1;
  if (failure_class_ == FailClass::LEVELDB_DATABASE &&
      instance_count_[FailClass::LEVELDB_DATABASE] ==
          fail_on_instance_num_[FailClass::LEVELDB_DATABASE]) {
    return std::make_unique<LevelDBTestDatabase>(
        std::move(state), std::move(scopes), this, std::move(task_runner),
        max_open_iterators, failure_method_,
        fail_on_call_num_[FailClass::LEVELDB_DATABASE]);
  }
  return DefaultTransactionalLevelDBFactory::CreateLevelDBDatabase(
      std::move(state), std::move(scopes), std::move(task_runner),
      max_open_iterators);
}

std::unique_ptr<LevelDBDirectTransaction>
MockBrowserTestIndexedDBClassFactory::CreateLevelDBDirectTransaction(
    TransactionalLevelDBDatabase* db) {
  instance_count_[FailClass::LEVELDB_DIRECT_TRANSACTION] =
      instance_count_[FailClass::LEVELDB_DIRECT_TRANSACTION] + 1;
  if (failure_class_ == FailClass::LEVELDB_DIRECT_TRANSACTION &&
      instance_count_[FailClass::LEVELDB_DIRECT_TRANSACTION] ==
          fail_on_instance_num_[FailClass::LEVELDB_DIRECT_TRANSACTION]) {
    return std::make_unique<LevelDBTestDirectTransaction>(
        db, failure_method_,
        fail_on_call_num_[FailClass::LEVELDB_DIRECT_TRANSACTION]);
  }
  return DefaultTransactionalLevelDBFactory::CreateLevelDBDirectTransaction(db);
}

scoped_refptr<TransactionalLevelDBTransaction>
MockBrowserTestIndexedDBClassFactory::CreateLevelDBTransaction(
    TransactionalLevelDBDatabase* db,
    std::unique_ptr<LevelDBScope> scope) {
  instance_count_[FailClass::LEVELDB_TRANSACTION] =
      instance_count_[FailClass::LEVELDB_TRANSACTION] + 1;
  if (only_trace_calls_) {
    return base::MakeRefCounted<LevelDBTraceTransaction>(
        db, std::move(scope), instance_count_[FailClass::LEVELDB_TRANSACTION]);
  }
  if (failure_class_ == FailClass::LEVELDB_TRANSACTION &&
      instance_count_[FailClass::LEVELDB_TRANSACTION] ==
          fail_on_instance_num_[FailClass::LEVELDB_TRANSACTION]) {
    return base::MakeRefCounted<LevelDBTestTransaction>(
        db, std::move(scope), failure_method_,
        fail_on_call_num_[FailClass::LEVELDB_TRANSACTION]);
  }
  return DefaultTransactionalLevelDBFactory::CreateLevelDBTransaction(
      db, std::move(scope));
}

std::unique_ptr<TransactionalLevelDBIterator>
MockBrowserTestIndexedDBClassFactory::CreateIterator(
    std::unique_ptr<leveldb::Iterator> iterator,
    base::WeakPtr<TransactionalLevelDBDatabase> db,
    base::WeakPtr<TransactionalLevelDBTransaction> txn,
    std::unique_ptr<LevelDBSnapshot> snapshot) {
  instance_count_[FailClass::LEVELDB_ITERATOR] =
      instance_count_[FailClass::LEVELDB_ITERATOR] + 1;
  if (only_trace_calls_) {
    return std::make_unique<LevelDBTraceIterator>(
        std::move(iterator), db, std::move(txn), std::move(snapshot),
        instance_count_[FailClass::LEVELDB_ITERATOR]);
  }
  if (failure_class_ == FailClass::LEVELDB_ITERATOR &&
      instance_count_[FailClass::LEVELDB_ITERATOR] ==
          fail_on_instance_num_[FailClass::LEVELDB_ITERATOR]) {
    return std::make_unique<LevelDBTestIterator>(
        std::move(iterator), db, std::move(txn), std::move(snapshot),
        failure_method_, fail_on_call_num_[FailClass::LEVELDB_ITERATOR]);
  }
  return DefaultTransactionalLevelDBFactory::CreateIterator(
      std::move(iterator), db, std::move(txn), std::move(snapshot));
}

void MockBrowserTestIndexedDBClassFactory::FailOperation(
    storage::mojom::FailClass failure_class,
    storage::mojom::FailMethod failure_method,
    int fail_on_instance_num,
    int fail_on_call_num) {
  VLOG(0) << "FailOperation: class=" << failure_class
          << ", method=" << failure_method
          << ", instanceNum=" << fail_on_instance_num
          << ", callNum=" << fail_on_call_num;
  DCHECK(failure_class != FailClass::NOTHING);
  DCHECK(failure_method != FailMethod::NOTHING);
  failure_class_ = failure_class;
  failure_method_ = failure_method;
  fail_on_instance_num_[failure_class_] = fail_on_instance_num;
  fail_on_call_num_[failure_class_] = fail_on_call_num;
  instance_count_.clear();
}

}  // namespace content::indexed_db

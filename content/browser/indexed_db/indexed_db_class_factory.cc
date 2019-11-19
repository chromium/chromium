// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_class_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"

namespace content {
namespace {
DefaultLevelDBFactory* GetDefaultLevelDBFactory() {
  static base::NoDestructor<DefaultLevelDBFactory> leveldb_factory(
      IndexedDBClassFactory::GetLevelDBOptions(), "indexed-db");
  return leveldb_factory.get();
}
DefaultTransactionalLevelDBFactory* GetDefaultTransactionalLevelDBFactory() {
  static base::NoDestructor<DefaultTransactionalLevelDBFactory>
      transactional_leveldb_factory;
  return transactional_leveldb_factory.get();
}
}  // namespace
static IndexedDBClassFactory::GetterCallback* s_factory_getter;
static ::base::LazyInstance<IndexedDBClassFactory>::Leaky s_factory =
    LAZY_INSTANCE_INITIALIZER;

void IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetterCallback* cb) {
  s_factory_getter = cb;
}

// static
IndexedDBClassFactory* IndexedDBClassFactory::Get() {
  if (s_factory_getter)
    return (*s_factory_getter)();
  else
    return s_factory.Pointer();
}

// static
leveldb_env::Options IndexedDBClassFactory::GetLevelDBOptions() {
  static const leveldb::FilterPolicy* kIDBFilterPolicy =
      leveldb::NewBloomFilterPolicy(10);
  leveldb_env::Options options;
  options.comparator = indexed_db::GetDefaultLevelDBComparator();
  options.paranoid_checks = true;
  options.filter_policy = kIDBFilterPolicy;
  options.compression = leveldb::kSnappyCompression;
  // For info about the troubles we've run into with this parameter, see:
  // https://crbug.com/227313#c11
  options.max_open_files = 80;
  options.env = IndexedDBLevelDBEnv::Get();
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();
  options.on_get_error = base::BindRepeating(
      indexed_db::ReportLevelDBError, "WebCore.IndexedDB.LevelDBReadErrors");
  options.on_write_error = base::BindRepeating(
      indexed_db::ReportLevelDBError, "WebCore.IndexedDB.LevelDBWriteErrors");
  return options;
}

IndexedDBClassFactory::IndexedDBClassFactory()
    : IndexedDBClassFactory(GetDefaultLevelDBFactory(),
                            GetDefaultTransactionalLevelDBFactory()) {}

IndexedDBClassFactory::IndexedDBClassFactory(
    LevelDBFactory* leveldb_factory,
    TransactionalLevelDBFactory* transactional_leveldb_factory)
    : leveldb_factory_(leveldb_factory),
      transactional_leveldb_factory_(transactional_leveldb_factory) {}

LevelDBFactory& IndexedDBClassFactory::leveldb_factory() {
  return *leveldb_factory_;
}
TransactionalLevelDBFactory&
IndexedDBClassFactory::transactional_leveldb_factory() {
  return *transactional_leveldb_factory_;
}

std::pair<std::unique_ptr<IndexedDBDatabase>, leveldb::Status>
IndexedDBClassFactory::CreateIndexedDBDatabase(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    TasksAvailableCallback tasks_available_callback,
    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
    const IndexedDBDatabase::Identifier& unique_identifier,
    ScopesLockManager* transaction_lock_manager) {
  DCHECK(backing_store);
  DCHECK(factory);
  std::unique_ptr<IndexedDBDatabase> database =
      base::WrapUnique(new IndexedDBDatabase(
          name, backing_store, factory, this,
          std::move(tasks_available_callback), std::move(metadata_coding),
          unique_identifier, transaction_lock_manager));
  leveldb::Status s = database->OpenInternal();
  if (!s.ok())
    database = nullptr;
  return {std::move(database), s};
}

std::unique_ptr<IndexedDBTransaction>
IndexedDBClassFactory::CreateIndexedDBTransaction(
    int64_t id,
    IndexedDBConnection* connection,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    TasksAvailableCallback tasks_available_callback,
    IndexedDBTransaction::TearDownCallback tear_down_callback,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  return base::WrapUnique(new IndexedDBTransaction(
      id, connection, scope, mode, std::move(tasks_available_callback),
      std::move(tear_down_callback), backing_store_transaction));
}

void IndexedDBClassFactory::SetLevelDBFactoryForTesting(
    LevelDBFactory* leveldb_factory) {
  if (leveldb_factory)
    leveldb_factory_ = leveldb_factory;
  else
    leveldb_factory_ = GetDefaultLevelDBFactory();
}

}  // namespace content

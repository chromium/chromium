// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_class_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/services/storage/filesystem_proxy_factory.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_factory.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_factory.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/leveldatabase/env_chromium.h"
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

static ::base::LazyInstance<IndexedDBClassFactory>::Leaky s_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
IndexedDBClassFactory* IndexedDBClassFactory::Get() {
  return s_factory.Pointer();
}

IndexedDBClassFactory::IndexedDBClassFactory()
    : leveldb_factory_(GetDefaultLevelDBFactory()),
      transactional_leveldb_factory_(GetDefaultTransactionalLevelDBFactory()) {}

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
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();
  options.on_get_error = base::BindRepeating(
      indexed_db::ReportLevelDBError, "WebCore.IndexedDB.LevelDBReadErrors");
  options.on_write_error = base::BindRepeating(
      indexed_db::ReportLevelDBError, "WebCore.IndexedDB.LevelDBWriteErrors");

  static base::NoDestructor<leveldb_env::ChromiumEnv> g_leveldb_env(
      "LevelDBEnv.IDB");
  options.env = g_leveldb_env.get();

  return options;
}

LevelDBFactory& IndexedDBClassFactory::leveldb_factory() {
  return *leveldb_factory_;
}
TransactionalLevelDBFactory&
IndexedDBClassFactory::transactional_leveldb_factory() {
  return *transactional_leveldb_factory_;
}

void IndexedDBClassFactory::SetTransactionalLevelDBFactoryForTesting(
    TransactionalLevelDBFactory* factory) {
  if (factory) {
    transactional_leveldb_factory_ = factory;
  } else {
    transactional_leveldb_factory_ = GetDefaultTransactionalLevelDBFactory();
  }
}

void IndexedDBClassFactory::SetLevelDBFactoryForTesting(
    LevelDBFactory* leveldb_factory) {
  if (leveldb_factory)
    leveldb_factory_ = leveldb_factory;
  else
    leveldb_factory_ = GetDefaultLevelDBFactory();
}

}  // namespace content

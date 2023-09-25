// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {
class LevelDBFactory;
class TransactionalLevelDBFactory;

// This singleton holds onto LevelDB factories which can be replaced with
// testing versions to facilitate mocking out of LevelDB objects and operations.
// TODO(estade): all LevelDB dependencies should be tucked away behind
// `IndexedDBBackingStore`.
class CONTENT_EXPORT IndexedDBClassFactory {
 public:
  static IndexedDBClassFactory* Get();

  // Visible for testing.
  static leveldb_env::Options GetLevelDBOptions();

  LevelDBFactory& leveldb_factory();
  TransactionalLevelDBFactory& transactional_leveldb_factory();

  void SetTransactionalLevelDBFactoryForTesting(
      TransactionalLevelDBFactory* factory);
  void SetLevelDBFactoryForTesting(LevelDBFactory* leveldb_factory);

 protected:
  IndexedDBClassFactory();
  ~IndexedDBClassFactory() = default;
  friend struct base::LazyInstanceTraitsBase<IndexedDBClassFactory>;

  raw_ptr<LevelDBFactory> leveldb_factory_;
  raw_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

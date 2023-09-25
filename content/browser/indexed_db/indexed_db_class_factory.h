// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class LevelDBFactory;
class TransactionalLevelDBFactory;

// Use this factory to create some IndexedDB objects. Exists solely to
// facilitate tests which sometimes need to inject mock objects into the system.
// TODO(dmurph): Remove this class in favor of dependency injection. This makes
// it really hard to iterate on the system.
class CONTENT_EXPORT IndexedDBClassFactory {
 public:
  using GetterCallback = base::RepeatingCallback<IndexedDBClassFactory*(void)>;

  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback =
      base::RepeatingCallback<void(leveldb::Status, const char*)>;

  static IndexedDBClassFactory* Get();

  static void SetIndexedDBClassFactoryGetter(GetterCallback cb);

  // Visible for testing.
  static leveldb_env::Options GetLevelDBOptions();

  LevelDBFactory& leveldb_factory();
  virtual TransactionalLevelDBFactory& transactional_leveldb_factory();

  void SetLevelDBFactoryForTesting(LevelDBFactory* leveldb_factory);

 protected:
  IndexedDBClassFactory();
  IndexedDBClassFactory(
      LevelDBFactory* leveldb_factory,
      TransactionalLevelDBFactory* transactional_leveldb_factory);
  virtual ~IndexedDBClassFactory() = default;
  friend struct base::LazyInstanceTraitsBase<IndexedDBClassFactory>;

  raw_ptr<LevelDBFactory> leveldb_factory_;
  raw_ptr<TransactionalLevelDBFactory> transactional_leveldb_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLASS_FACTORY_H_

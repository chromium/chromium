// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_env.h"

#include "base/no_destructor.h"
#include "components/services/storage/filesystem_proxy_factory.h"

namespace content {

IndexedDBLevelDBEnv::IndexedDBLevelDBEnv()
    : ChromiumEnv("LevelDBEnv.IDB", storage::CreateFilesystemProxy()) {}

IndexedDBLevelDBEnv* IndexedDBLevelDBEnv::Get() {
  static base::NoDestructor<IndexedDBLevelDBEnv> g_leveldb_env;
  return g_leveldb_env.get();
}

}  // namespace content

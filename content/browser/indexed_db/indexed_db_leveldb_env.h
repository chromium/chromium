// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_ENV_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_ENV_H_

#include <memory>
#include <tuple>

#include "base/no_destructor.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace content {

// The leveldb::Env used by the Indexed DB backend.
class IndexedDBLevelDBEnv : public leveldb_env::ChromiumEnv {
 public:
  CONTENT_EXPORT static IndexedDBLevelDBEnv* Get();

 private:
  friend class base::NoDestructor<IndexedDBLevelDBEnv>;
  IndexedDBLevelDBEnv();
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_ENV_H_

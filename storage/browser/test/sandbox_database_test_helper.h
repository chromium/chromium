// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_SANDBOX_DATABASE_TEST_HELPER_H_
#define STORAGE_BROWSER_TEST_SANDBOX_DATABASE_TEST_HELPER_H_

#include <stddef.h>

#include "third_party/leveldatabase/src/db/filename.h"

namespace base {
class FilePath;
}

namespace storage {

void CorruptDatabase(const base::FilePath& db_path,
                     leveldb::FileType type,
                     ptrdiff_t offset,
                     size_t size);

void DeleteDatabaseFile(const base::FilePath& db_path,
                        leveldb::FileType type);

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_SANDBOX_DATABASE_TEST_HELPER_H_

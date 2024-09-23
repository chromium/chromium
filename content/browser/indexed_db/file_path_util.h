// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_FILE_PATH_UTIL_H_
#define CONTENT_BROWSER_INDEXED_DB_FILE_PATH_UTIL_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace storage {
struct BucketLocator;
}

namespace content::indexed_db {

extern CONTENT_EXPORT const base::FilePath::CharType kIndexedDBExtension[];
extern const base::FilePath::CharType kIndexedDBFile[];
extern CONTENT_EXPORT const base::FilePath::CharType kLevelDBExtension[];

// Returns whether the legacy (first-party/default-bucket) path should be used
// for storing IDB files for the given bucket.
bool ShouldUseLegacyFilePath(const storage::BucketLocator& bucket_locator);

base::FilePath GetBlobStoreFileName(
    const storage::BucketLocator& bucket_locator);

base::FilePath GetLevelDBFileName(const storage::BucketLocator& bucket_locator);

base::FilePath GetBlobDirectoryName(const base::FilePath& path_base,
                                    int64_t database_id);

base::FilePath GetBlobDirectoryNameForKey(const base::FilePath& path_base,
                                          int64_t database_id,
                                          int64_t blob_number);

base::FilePath GetBlobFileNameForKey(const base::FilePath& path_base,
                                     int64_t database_id,
                                     int64_t blob_number);

// Returns if the given file path is too long for the current operating system's
// file system.
bool IsPathTooLong(const base::FilePath& leveldb_dir);

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_FILE_PATH_UTIL_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/file_path_util.h"

#include <inttypes.h>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content::indexed_db {

namespace {
constexpr base::FilePath::CharType kBlobExtension[] =
    FILE_PATH_LITERAL(".blob");
}  // namespace

const base::FilePath::CharType kLevelDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");
const base::FilePath::CharType kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".indexeddb");
const base::FilePath::CharType kIndexedDBFile[] =
    FILE_PATH_LITERAL("indexeddb");

bool ShouldUseLegacyFilePath(const storage::BucketLocator& bucket_locator) {
  return bucket_locator.storage_key.IsFirstPartyContext() &&
         bucket_locator.is_default;
}

base::FilePath GetBlobStoreFileName(
    const storage::BucketLocator& bucket_locator) {
  if (ShouldUseLegacyFilePath(bucket_locator)) {
    // First-party blob files, for legacy reasons, are stored at:
    // {{first_party_data_path}}/{{serialized_origin}}.indexeddb.blob
    return base::FilePath()
        .AppendASCII(storage::GetIdentifierFromOrigin(
            bucket_locator.storage_key.origin()))
        .AddExtension(kIndexedDBExtension)
        .AddExtension(kBlobExtension);
  }

  // Third-party blob files are stored at:
  // {{third_party_data_path}}/{{bucket_id}}/IndexedDB/indexeddb.blob
  return base::FilePath(kIndexedDBFile).AddExtension(kBlobExtension);
}

base::FilePath GetLevelDBFileName(
    const storage::BucketLocator& bucket_locator) {
  if (ShouldUseLegacyFilePath(bucket_locator)) {
    // First-party leveldb files, for legacy reasons, are stored at:
    // {{first_party_data_path}}/{{serialized_origin}}.indexeddb.leveldb
    // TODO(crbug.com/40855748): Migrate all first party buckets to the new
    // path.
    return base::FilePath()
        .AppendASCII(storage::GetIdentifierFromOrigin(
            bucket_locator.storage_key.origin()))
        .AddExtension(kIndexedDBExtension)
        .AddExtension(kLevelDBExtension);
  }

  // Third-party leveldb files are stored at:
  // {{third_party_data_path}}/{{bucket_id}}/IndexedDB/indexeddb.leveldb
  return base::FilePath(kIndexedDBFile).AddExtension(kLevelDBExtension);
}

base::FilePath GetBlobDirectoryName(const base::FilePath& path_base,
                                    int64_t database_id) {
  return path_base.AppendASCII(base::StringPrintf("%" PRIx64, database_id));
}

base::FilePath GetBlobDirectoryNameForKey(const base::FilePath& path_base,
                                          int64_t database_id,
                                          int64_t blob_number) {
  base::FilePath path = GetBlobDirectoryName(path_base, database_id);
  path = path.AppendASCII(base::StringPrintf(
      "%02x", static_cast<int>(blob_number & 0x000000000000ff00) >> 8));
  return path;
}

base::FilePath GetBlobFileNameForKey(const base::FilePath& path_base,
                                     int64_t database_id,
                                     int64_t blob_number) {
  base::FilePath path =
      GetBlobDirectoryNameForKey(path_base, database_id, blob_number);
  path = path.AppendASCII(base::StringPrintf("%" PRIx64, blob_number));
  return path;
}

bool IsPathTooLong(const base::FilePath& leveldb_dir) {
  int limit = base::GetMaximumPathComponentLength(leveldb_dir.DirName());
  if (limit < 0) {
    DLOG(WARNING) << "GetMaximumPathComponentLength returned -1";
// In limited testing, ChromeOS returns 143, other OSes 255.
#if BUILDFLAG(IS_CHROMEOS)
    limit = 143;
#else
    limit = 255;
#endif
  }
  size_t component_length = leveldb_dir.BaseName().value().length();
  if (component_length > static_cast<uint32_t>(limit)) {
    DLOG(WARNING) << "Path component length (" << component_length
                  << ") exceeds maximum (" << limit
                  << ") allowed by this filesystem.";
    const int min = 140;
    const int max = 300;
    const int num_buckets = 12;
    base::UmaHistogramCustomCounts(
        "WebCore.IndexedDB.BackingStore.OverlyLargeOriginLength",
        component_length, min, max, num_buckets);
    return true;
  }
  return false;
}

}  // namespace content::indexed_db

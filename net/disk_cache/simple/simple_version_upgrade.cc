// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_version_upgrade.h"

#include <cstring>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "net/disk_cache/cache_file.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_backend_version.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "third_party/zlib/zlib.h"

namespace {

// It is not possible to upgrade cache structures on disk that are of version
// below this, the entire cache should be dropped for them.
constexpr uint32_t kMinVersionAbleToUpgrade = 8;

constexpr char kFakeIndexFileName[] = "index";
constexpr char kIndexDirName[] = "index-dir";
constexpr char kIndexFileName[] = "the-real-index";

void LogMessageFailedUpgradeFromVersion(int version) {
  LOG(ERROR) << "Failed to upgrade Simple Cache from version: " << version;
}

bool WriteFakeIndexFile(disk_cache::BackendFileOperations* file_operations,
                        const base::FilePath& file_name) {
  std::unique_ptr<disk_cache::CacheFile> file = file_operations->OpenFile(
      file_name, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file->IsValid()) {
    return false;
  }

  disk_cache::FakeIndexData file_contents;
  file_contents.initial_magic_number = disk_cache::kSimpleInitialMagicNumber;
  file_contents.version = disk_cache::kSimpleVersion;
  file_contents.zero = 0;
  file_contents.zero2 = 0;
  file_contents.encryption_status = file_operations->IsEncrypted() ? 1 : 0;

  if (!file->WriteAndCheck(0, base::byte_span_from_ref(file_contents))) {
    LOG(ERROR) << "Failed to write fake index file: "
               << file_name.LossyDisplayName();
    return false;
  }
  return true;
}

}  // namespace

namespace disk_cache {

FakeIndexData::FakeIndexData() {
  // We don't want unset holes in types stored to disk.
  static_assert(std::has_unique_object_representations_v<FakeIndexData>,
                "FakeIndexData should have no implicit padding bytes");
}

// Some points about the Upgrade process are still not clear:
// 1. if the upgrade path requires dropping cache it would be faster to just
//    return an initialization error here and proceed with asynchronous cache
//    cleanup in CacheCreator. Should this hack be considered valid? Some smart
//    tests may fail.
// 2. Because Android process management allows for killing a process at any
//    time, the upgrade process may need to deal with a partially completed
//    previous upgrade. For example, while upgrading A -> A + 2 we are the
//    process gets killed and some parts are remaining at version A + 1. There
//    are currently no generic mechanisms to resolve this situation, co the
//    upgrade codes need to ensure they can continue after being stopped in the
//    middle. It also means that the "fake index" must be flushed in between the
//    upgrade steps. Atomicity of this is an interesting research topic. The
//    intermediate fake index flushing must be added as soon as we add more
//    upgrade steps.
// 3. This upgrade logic only upgrades the fake index file and not other files
//    (entry cache file nor sparse entry file) on Version 9.
SimpleCacheConsistencyResult UpgradeSimpleCacheOnDisk(
    BackendFileOperations* file_operations,
    const base::FilePath& path) {
  // There is a convention among disk cache backends: looking at the magic in
  // the file "index" it should be sufficient to determine if the cache belongs
  // to the currently running backend. The Simple Backend stores its index in
  // the file "the-real-index" (see simple_index_file.cc) and the file "index"
  // only signifies presence of the implementation's magic and version. There
  // are two reasons for that:
  // 1. Absence of the index is itself not a fatal error in the Simple Backend
  // 2. The Simple Backend has pickled file format for the index making it hacky
  //    to have the magic in the right place.
  const base::FilePath fake_index = path.AppendASCII(kFakeIndexFileName);
  std::unique_ptr<CacheFile> fake_index_file = file_operations->OpenFile(
      fake_index, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!fake_index_file->IsValid()) {
    if (fake_index_file->error_details() == base::File::FILE_ERROR_NOT_FOUND) {
      if (!WriteFakeIndexFile(file_operations, fake_index)) {
        file_operations->DeleteFile(fake_index);
        LOG(ERROR) << "Failed to write a new fake index.";
        return SimpleCacheConsistencyResult::kWriteFakeIndexFileFailed;
      }
      return SimpleCacheConsistencyResult::kOK;
    }
    return SimpleCacheConsistencyResult::kBadFakeIndexFile;
  }

  FakeIndexData file_header;
  if (!fake_index_file->ReadAndCheck(0,
                                     base::byte_span_from_ref(file_header))) {
    LOG(ERROR) << "Disk cache backend fake index file has wrong size.";
    return SimpleCacheConsistencyResult::kBadFakeIndexReadSize;
  }
  if (file_header.initial_magic_number != kSimpleInitialMagicNumber) {
    LOG(ERROR) << "Disk cache backend fake index file has wrong magic number.";
    return SimpleCacheConsistencyResult::kBadInitialMagicNumber;
  }
  fake_index_file.reset();

  uint32_t version_from = file_header.version;
  if (version_from < kMinVersionAbleToUpgrade) {
    LOG(ERROR) << "Version " << version_from << " is too old.";
    return SimpleCacheConsistencyResult::kVersionTooOld;
  }

  if (version_from > kSimpleVersion) {
    LOG(ERROR) << "Version " << version_from << " is from the future.";
    return SimpleCacheConsistencyResult::kVersionFromTheFuture;
  }

  if (file_header.zero != 0 && file_header.zero2 != 0) {
    LOG(WARNING) << "Rebuilding cache due to experiment change";
    return SimpleCacheConsistencyResult::kBadZeroCheck;
  }

  if (file_header.encryption_status != file_operations->IsEncrypted()) {
    LOG(WARNING) << "Rebuilding cache due to encryption status change";
    return SimpleCacheConsistencyResult::kEncryptionStatusMismatch;
  }

  bool new_fake_index_needed = (version_from != kSimpleVersion);

  // There should be one upgrade routine here for each incremental upgrade
  // starting at kMinVersionAbleToUpgrade.
  static_assert(kMinVersionAbleToUpgrade == 8, "upgrade routines don't match");
  DCHECK_LE(8U, version_from);
  if (version_from == 8) {
    // Likewise, V8 -> V9 is handled entirely by the index reader.
    version_from++;
  }

  DCHECK_EQ(kSimpleIndexFileVersion, version_from);

  if (!new_fake_index_needed)
    return SimpleCacheConsistencyResult::kOK;

  const base::FilePath temp_fake_index = path.AppendASCII("upgrade-index");
  if (!WriteFakeIndexFile(file_operations, temp_fake_index)) {
    file_operations->DeleteFile(temp_fake_index);
    LOG(ERROR) << "Failed to write a new fake index.";
    LogMessageFailedUpgradeFromVersion(file_header.version);
    return SimpleCacheConsistencyResult::kWriteFakeIndexFileFailed;
  }
  if (!file_operations->ReplaceFile(temp_fake_index, fake_index, nullptr)) {
    LOG(ERROR) << "Failed to replace the fake index.";
    LogMessageFailedUpgradeFromVersion(file_header.version);
    return SimpleCacheConsistencyResult::kReplaceFileFailed;
  }
  return SimpleCacheConsistencyResult::kOK;
}

bool DeleteIndexFilesIfCacheIsEmpty(const base::FilePath& path) {
  const base::FilePath fake_index = path.AppendASCII(kFakeIndexFileName);
  const base::FilePath index_dir = path.AppendASCII(kIndexDirName);
  // The newer schema versions have the real index in the index directory.
  // Older versions, however, had a real index file in the same directory.
  const base::FilePath legacy_index_file = path.AppendASCII(kIndexFileName);
  base::FileEnumerator e(
      path, /* recursive = */ false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    if (name == fake_index || name == index_dir || name == legacy_index_file)
      continue;
    return false;
  }
  bool deleted_fake_index = base::DeleteFile(fake_index);
  bool deleted_index_dir = base::DeletePathRecursively(index_dir);
  bool deleted_legacy_index_file = base::DeleteFile(legacy_index_file);
  return deleted_fake_index || deleted_index_dir || deleted_legacy_index_file;
}

}  // namespace disk_cache

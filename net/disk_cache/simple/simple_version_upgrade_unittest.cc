// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_version_upgrade.h"

#include <stdint.h>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/simple/simple_backend_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Same as |disk_cache::kSimpleInitialMagicNumber|.
const uint64_t kSimpleInitialMagicNumber = UINT64_C(0xfcfb6d1ba7725c30);

// The "fake index" file that cache backends use to distinguish whether the
// cache belongs to one backend or another.
const char kFakeIndexFileName[] = "index";

// Same as |SimpleIndexFile::kIndexDirectory|.
const char kIndexDirName[] = "index-dir";

// Same as |SimpleIndexFile::kIndexFileName|.
const char kIndexFileName[] = "the-real-index";

bool WriteFakeIndexFileV8(const base::FilePath& cache_path) {
  disk_cache::FakeIndexData data;
  data.version = 8;
  data.initial_magic_number = kSimpleInitialMagicNumber;
  data.zero = 0;
  data.zero2 = 0;
  const base::FilePath file_name = cache_path.AppendASCII("index");
  return base::WriteFile(file_name, base::byte_span_from_ref(data));
}

bool WriteFakeIndexFile(const base::FilePath& cache_path,
                        uint32_t version,
                        int encryption_status) {
  disk_cache::FakeIndexData data;
  data.version = version;
  data.initial_magic_number = kSimpleInitialMagicNumber;
  data.zero = 0;
  data.zero2 = 0;
  data.encryption_status = encryption_status;
  const base::FilePath file_name = cache_path.AppendASCII(kFakeIndexFileName);
  return base::WriteFile(file_name, base::byte_span_from_ref(data));
}

// Mock `BackendFileOperations` to expose and change the encryption field for
// testing.
class TestFileOperations : public disk_cache::TrivialFileOperations {
 public:
  explicit TestFileOperations(bool is_encrypted)
      : is_encrypted_(is_encrypted) {}

  bool IsEncrypted() const override { return is_encrypted_; }

 private:
  const bool is_encrypted_;
};

TEST(SimpleVersionUpgradeTest, FailsToMigrateBackwards) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  disk_cache::FakeIndexData data;
  data.version = 100500;
  data.initial_magic_number = kSimpleInitialMagicNumber;
  data.zero = 0;
  data.zero2 = 0;
  const base::FilePath file_name = cache_path.AppendASCII(kFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(file_name, base::byte_span_from_ref(data)));
  disk_cache::TrivialFileOperations file_operations;
  EXPECT_EQ(disk_cache::SimpleCacheConsistencyResult::kVersionFromTheFuture,
            disk_cache::UpgradeSimpleCacheOnDisk(&file_operations,
                                                 cache_dir.GetPath()));
}

TEST(SimpleVersionUpgradeTest, ExperimentBacktoDefault) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  disk_cache::FakeIndexData data;
  data.version = disk_cache::kSimpleVersion;
  data.initial_magic_number = kSimpleInitialMagicNumber;
  data.zero = 2;
  data.zero2 = 4;
  const base::FilePath file_name = cache_path.AppendASCII(kFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(file_name, base::byte_span_from_ref(data)));

  disk_cache::TrivialFileOperations file_operations;
  // The cache needs to transition from a deprecated experiment back to not
  // having one.
  EXPECT_EQ(disk_cache::SimpleCacheConsistencyResult::kBadZeroCheck,
            disk_cache::UpgradeSimpleCacheOnDisk(&file_operations,
                                                 cache_dir.GetPath()));
}

TEST(SimpleVersionUpgradeTest, WritesEncryptionStatusToFakeIndexFile) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();
  const base::FilePath fake_index_path =
      cache_path.AppendASCII(kFakeIndexFileName);

  TestFileOperations file_operations(true);
  ASSERT_EQ(disk_cache::SimpleCacheConsistencyResult::kOK,
            disk_cache::UpgradeSimpleCacheOnDisk(&file_operations, cache_path));

  disk_cache::FakeIndexData data;
  ASSERT_TRUE(base::ReadFile(fake_index_path, base::byte_span_from_ref(data)));
  EXPECT_EQ(1, data.encryption_status);
  EXPECT_EQ(disk_cache::kSimpleVersion, data.version);
}

TEST(SimpleVersionUpgradeTest, EncryptionStatusMismatch) {
  auto run_and_check = [](int cache_encrypted_status,
                          bool backend_is_encrypted) {
    base::ScopedTempDir cache_dir;
    ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
    const base::FilePath cache_path = cache_dir.GetPath();

    // Create a fake index file with the specified encryption status.
    ASSERT_TRUE(WriteFakeIndexFile(cache_path, disk_cache::kSimpleVersion,
                                   cache_encrypted_status));

    // Simulate opening the cache with a backend of a different encryption
    // status.
    TestFileOperations file_operations(backend_is_encrypted);
    EXPECT_EQ(
        disk_cache::SimpleCacheConsistencyResult::kEncryptionStatusMismatch,
        disk_cache::UpgradeSimpleCacheOnDisk(&file_operations,
                                             cache_dir.GetPath()));
  };

  run_and_check(0, true);
  run_and_check(1, false);
}

TEST(SimpleVersionUpgradeTest, FakeIndexVersionGetsUpdated) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  ASSERT_TRUE(WriteFakeIndexFileV8(cache_path));

  const std::string file_contents("incorrectly serialized data");
  const base::FilePath index_file = cache_path.AppendASCII(kIndexFileName);
  ASSERT_TRUE(base::WriteFile(index_file, file_contents));

  disk_cache::TrivialFileOperations file_operations;
  // Upgrade.
  ASSERT_EQ(disk_cache::SimpleCacheConsistencyResult::kOK,
            disk_cache::UpgradeSimpleCacheOnDisk(&file_operations, cache_path));

  // Check that the version in the fake index file is updated.
  std::string new_fake_index_contents;
  ASSERT_TRUE(base::ReadFileToString(cache_path.AppendASCII(kFakeIndexFileName),
                                     &new_fake_index_contents));
  const disk_cache::FakeIndexData* fake_index_header;
  EXPECT_EQ(sizeof(*fake_index_header), new_fake_index_contents.size());
  // TODO(crbug.com/428945428): Fix unsafe uses of std::string::data().
  fake_index_header =
      UNSAFE_TODO(reinterpret_cast<const disk_cache::FakeIndexData*>(
          new_fake_index_contents.data()));
  EXPECT_EQ(disk_cache::kSimpleVersion, fake_index_header->version);
  EXPECT_EQ(kSimpleInitialMagicNumber, fake_index_header->initial_magic_number);
}

TEST(SimpleVersionUpgradeTest, DeleteAllIndexFilesWhenCacheIsEmpty) {
  const std::string kCorruptData("corrupt");

  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  const base::FilePath fake_index = cache_path.AppendASCII(kFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(fake_index, kCorruptData));

  const base::FilePath index_path = cache_path.AppendASCII(kIndexDirName);
  ASSERT_TRUE(base::CreateDirectory(index_path));

  const base::FilePath index = index_path.AppendASCII(kIndexFileName);
  ASSERT_TRUE(base::WriteFile(index, kCorruptData));

  EXPECT_TRUE(disk_cache::DeleteIndexFilesIfCacheIsEmpty(cache_path));
  EXPECT_TRUE(base::PathExists(cache_path));
  EXPECT_TRUE(base::IsDirectoryEmpty(cache_path));
}

TEST(SimpleVersionUpgradeTest, DoesNotDeleteIndexFilesWhenCacheIsNotEmpty) {
  const std::string kCorruptData("corrupt");

  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  const base::FilePath fake_index = cache_path.AppendASCII(kFakeIndexFileName);
  ASSERT_TRUE(base::WriteFile(fake_index, kCorruptData));

  const base::FilePath index_path = cache_path.AppendASCII(kIndexDirName);
  ASSERT_TRUE(base::CreateDirectory(index_path));

  const base::FilePath index = index_path.AppendASCII(kIndexFileName);
  ASSERT_TRUE(base::WriteFile(index, kCorruptData));

  const base::FilePath entry_file = cache_path.AppendASCII("01234567_0");
  ASSERT_TRUE(base::WriteFile(entry_file, kCorruptData));

  EXPECT_FALSE(disk_cache::DeleteIndexFilesIfCacheIsEmpty(cache_path));
  EXPECT_TRUE(base::PathExists(cache_path));
  EXPECT_FALSE(base::IsDirectoryEmpty(cache_path));
  EXPECT_TRUE(base::PathExists(fake_index));
  EXPECT_TRUE(base::PathExists(index_path));
  EXPECT_TRUE(base::PathExists(index));
  EXPECT_TRUE(base::PathExists(entry_file));
}

}  // namespace

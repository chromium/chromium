// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "net/disk_cache/simple/simple_backend_version.h"
#include "net/disk_cache/simple/simple_entry_format_history.h"
#include "testing/gtest/include/gtest/gtest.h"

// The migration process relies on ability to rename newly created files, which
// could be problematic on Windows XP.
#if defined(OS_POSIX)

namespace {

// Same as |disk_cache::kSimpleInitialMagicNumber|.
const uint64_t kSimpleInitialMagicNumber = UINT64_C(0xfcfb6d1ba7725c30);

// The "fake index" file that cache backends use to distinguish whether the
// cache belongs to one backend or another.
const char kFakeIndexFileName[] = "index";

// Same as |SimpleIndexFile::kIndexFileName|.
const char kIndexFileName[] = "the-real-index";

bool WriteFakeIndexFileV5(const base::FilePath& cache_path) {
  disk_cache::FakeIndexData data;
  data.version = 5;
  data.initial_magic_number = kSimpleInitialMagicNumber;
  data.zero = 0;
  data.zero2 = 0;
  const base::FilePath file_name = cache_path.AppendASCII("index");
  return sizeof(data) ==
         base::WriteFile(
             file_name, reinterpret_cast<const char*>(&data), sizeof(data));
}

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
  ASSERT_EQ(static_cast<int>(sizeof(data)),
            base::WriteFile(file_name, reinterpret_cast<const char*>(&data),
                            sizeof(data)));
  EXPECT_EQ(disk_cache::SimpleCacheConsistencyResult::kVersionFromTheFuture,
            disk_cache::UpgradeSimpleCacheOnDisk(cache_dir.GetPath()));
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
  ASSERT_EQ(static_cast<int>(sizeof(data)),
            base::WriteFile(file_name, reinterpret_cast<const char*>(&data),
                            sizeof(data)));

  // The cache needs to transition from a deprecated experiment back to not
  // having one.
  EXPECT_EQ(disk_cache::SimpleCacheConsistencyResult::kBadZeroCheck,
            disk_cache::UpgradeSimpleCacheOnDisk(cache_dir.GetPath()));
}

TEST(SimpleVersionUpgradeTest, FakeIndexVersionGetsUpdated) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  WriteFakeIndexFileV5(cache_path);
  const std::string file_contents("incorrectly serialized data");
  const base::FilePath index_file = cache_path.AppendASCII(kIndexFileName);
  ASSERT_EQ(
      static_cast<int>(file_contents.size()),
      base::WriteFile(index_file, file_contents.data(), file_contents.size()));

  // Upgrade.
  ASSERT_EQ(disk_cache::SimpleCacheConsistencyResult::kOK,
            disk_cache::UpgradeSimpleCacheOnDisk(cache_path));

  // Check that the version in the fake index file is updated.
  std::string new_fake_index_contents;
  ASSERT_TRUE(base::ReadFileToString(cache_path.AppendASCII(kFakeIndexFileName),
                                     &new_fake_index_contents));
  const disk_cache::FakeIndexData* fake_index_header;
  EXPECT_EQ(sizeof(*fake_index_header), new_fake_index_contents.size());
  fake_index_header = reinterpret_cast<const disk_cache::FakeIndexData*>(
      new_fake_index_contents.data());
  EXPECT_EQ(disk_cache::kSimpleVersion, fake_index_header->version);
  EXPECT_EQ(kSimpleInitialMagicNumber, fake_index_header->initial_magic_number);
}

TEST(SimpleVersionUpgradeTest, UpgradeV5V6IndexMustDisappear) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  const base::FilePath cache_path = cache_dir.GetPath();

  WriteFakeIndexFileV5(cache_path);
  const std::string file_contents("incorrectly serialized data");
  const base::FilePath index_file = cache_path.AppendASCII(kIndexFileName);
  ASSERT_EQ(
      static_cast<int>(file_contents.size()),
      base::WriteFile(index_file, file_contents.data(), file_contents.size()));

  // Create a few entry-like files.
  const uint64_t kEntries = 5;
  for (uint64_t entry_hash = 0; entry_hash < kEntries; ++entry_hash) {
    for (int index = 0; index < 3; ++index) {
      std::string file_name =
          base::StringPrintf("%016" PRIx64 "_%1d", entry_hash, index);
      std::string entry_contents =
          file_contents +
          base::StringPrintf(" %" PRIx64, static_cast<uint64_t>(entry_hash));
      ASSERT_EQ(static_cast<int>(entry_contents.size()),
                base::WriteFile(cache_path.AppendASCII(file_name),
                                entry_contents.data(), entry_contents.size()));
    }
  }

  // Upgrade.
  ASSERT_TRUE(disk_cache::UpgradeIndexV5V6(cache_path));

  // Check that the old index disappeared but the files remain unchanged.
  EXPECT_FALSE(base::PathExists(index_file));
  for (uint64_t entry_hash = 0; entry_hash < kEntries; ++entry_hash) {
    for (int index = 0; index < 3; ++index) {
      std::string file_name =
          base::StringPrintf("%016" PRIx64 "_%1d", entry_hash, index);
      std::string expected_contents =
          file_contents +
          base::StringPrintf(" %" PRIx64, static_cast<uint64_t>(entry_hash));
      std::string real_contents;
      EXPECT_TRUE(base::ReadFileToString(cache_path.AppendASCII(file_name),
                                         &real_contents));
      EXPECT_EQ(expected_contents, real_contents);
    }
  }
}

}  // namespace

#endif  // defined(OS_POSIX)

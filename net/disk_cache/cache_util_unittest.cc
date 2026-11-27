// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <stdio.h>

#include <map>
#include <optional>
#include <string>

#include "base/byte_size.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace disk_cache {

class CacheUtilTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    cache_dir_ = tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("Cache"));
    file1_ = base::FilePath(cache_dir_.Append(FILE_PATH_LITERAL("file01")));
    file2_ = base::FilePath(cache_dir_.Append(FILE_PATH_LITERAL(".file02")));
    dir1_ = base::FilePath(cache_dir_.Append(FILE_PATH_LITERAL("dir01")));
    file3_ = base::FilePath(dir1_.Append(FILE_PATH_LITERAL("file03")));
    ASSERT_TRUE(base::CreateDirectory(cache_dir_));
    FILE* fp = base::OpenFile(file1_, "w");
    ASSERT_TRUE(fp != nullptr);
    base::CloseFile(fp);
    fp = base::OpenFile(file2_, "w");
    ASSERT_TRUE(fp != nullptr);
    base::CloseFile(fp);
    ASSERT_TRUE(base::CreateDirectory(dir1_));
    fp = base::OpenFile(file3_, "w");
    ASSERT_TRUE(fp != nullptr);
    base::CloseFile(fp);
    dest_dir_ = tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("old_Cache_001"));
    dest_file1_ = base::FilePath(dest_dir_.Append(FILE_PATH_LITERAL("file01")));
    dest_file2_ =
        base::FilePath(dest_dir_.Append(FILE_PATH_LITERAL(".file02")));
    dest_dir1_ = base::FilePath(dest_dir_.Append(FILE_PATH_LITERAL("dir01")));
  }

 protected:
  base::ScopedTempDir tmp_dir_;
  base::FilePath cache_dir_;
  base::FilePath file1_;
  base::FilePath file2_;
  base::FilePath dir1_;
  base::FilePath file3_;
  base::FilePath dest_dir_;
  base::FilePath dest_file1_;
  base::FilePath dest_file2_;
  base::FilePath dest_dir1_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(CacheUtilTest, MoveCache) {
  EXPECT_TRUE(disk_cache::MoveCache(cache_dir_, dest_dir_));
  EXPECT_TRUE(base::PathExists(dest_dir_));
  EXPECT_TRUE(base::PathExists(dest_file1_));
  EXPECT_TRUE(base::PathExists(dest_file2_));
  EXPECT_TRUE(base::PathExists(dest_dir1_));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(base::PathExists(cache_dir_));  // old cache dir stays
#else
  EXPECT_FALSE(base::PathExists(cache_dir_));  // old cache is gone
#endif
  EXPECT_FALSE(base::PathExists(file1_));
  EXPECT_FALSE(base::PathExists(file2_));
  EXPECT_FALSE(base::PathExists(dir1_));
}

TEST_F(CacheUtilTest, DeleteCache) {
  EXPECT_TRUE(disk_cache::DeleteCache(cache_dir_, false));
  EXPECT_TRUE(base::PathExists(cache_dir_));  // cache dir stays
  EXPECT_FALSE(base::PathExists(dir1_));
  EXPECT_FALSE(base::PathExists(file1_));
  EXPECT_FALSE(base::PathExists(file2_));
  EXPECT_FALSE(base::PathExists(file3_));
}

TEST_F(CacheUtilTest, DeleteCacheAndDir) {
  EXPECT_TRUE(disk_cache::DeleteCache(cache_dir_, true));
  EXPECT_FALSE(base::PathExists(cache_dir_));  // cache dir is gone
  EXPECT_FALSE(base::PathExists(dir1_));
  EXPECT_FALSE(base::PathExists(file1_));
  EXPECT_FALSE(base::PathExists(file2_));
  EXPECT_FALSE(base::PathExists(file3_));
}

TEST_F(CacheUtilTest, CleanupDirectory) {
  base::RunLoop run_loop;
  disk_cache::CleanupDirectory(cache_dir_,
                               base::BindLambdaForTesting([&](bool result) {
                                 EXPECT_TRUE(result);
                                 run_loop.Quit();
                               }));
  run_loop.Run();

  while (true) {
    base::FileEnumerator enumerator(tmp_dir_.GetPath(), /*recursive=*/false,
                                    /*file_type=*/base::FileEnumerator::FILES |
                                        base::FileEnumerator::DIRECTORIES);
    bool found = false;
    while (true) {
      base::FilePath path = enumerator.Next();
      if (path.empty()) {
        break;
      }
      // We're not sure if we see an entry in the directory because it depends
      // on timing, but if we do, it must be "old_Cache_000".
      // Caveat: On ChromeOS, we leave the top-level directory ("Cache") so
      // it must be "Cache" or "old_Cache_000".
      const base::FilePath dirname = path.DirName();
      std::optional<base::SafeBaseName> basename =
          base::SafeBaseName::Create(path);
      ASSERT_EQ(dirname, tmp_dir_.GetPath());
      ASSERT_TRUE(basename.has_value());
#if BUILDFLAG(IS_CHROMEOS)
      if (basename->path().value() == FILE_PATH_LITERAL("Cache")) {
        // See the comment above.
        ASSERT_TRUE(base::IsDirectoryEmpty(dirname.Append(*basename)));
        continue;
      }
#endif
      ASSERT_EQ(basename->path().value(), FILE_PATH_LITERAL("old_Cache_000"));
      found = true;
    }
    if (!found) {
      break;
    }

    base::PlatformThread::Sleep(base::Milliseconds(10));
  }
}

#if BUILDFLAG(IS_POSIX)
TEST_F(CacheUtilTest, CleanupDirectoryFailsWhenParentDirectoryIsInaccessible) {
  base::RunLoop run_loop;

  ASSERT_TRUE(base::SetPosixFilePermissions(tmp_dir_.GetPath(), /*mode=*/0));
  disk_cache::CleanupDirectory(cache_dir_,
                               base::BindLambdaForTesting([&](bool result) {
                                 EXPECT_FALSE(result);
                                 run_loop.Quit();
                               }));
  run_loop.Run();
}

TEST_F(CacheUtilTest,
       CleanupDirectorySucceedsWhenTargetDirectoryIsInaccessible) {
  base::RunLoop run_loop;

  // Make a child directory inaccessible so asynchronous recursive deletion
  // cannot remove it, while renaming `cache_dir_` itself still succeeds on all
  // POSIX platforms (on macOS < 26, `rename(2)` returns EACCES if `cache_dir_`
  // itself has mode 0).
  ASSERT_TRUE(base::SetPosixFilePermissions(dir1_, /*mode=*/0));
#if !BUILDFLAG(IS_MAC)
  ASSERT_TRUE(base::SetPosixFilePermissions(cache_dir_, /*mode=*/0));
#endif
  disk_cache::CleanupDirectory(cache_dir_,
                               base::BindLambdaForTesting([&](bool result) {
                                 EXPECT_TRUE(result);
                                 run_loop.Quit();
                               }));
  run_loop.Run();
#if !BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(base::PathExists(cache_dir_));
#endif
}

TEST_F(CacheUtilTest, MoveCacheFailsForSymlinkAndRenameFailure) {
  const base::FilePath external_dir =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("external_dir"));
  ASSERT_TRUE(base::CreateDirectory(external_dir));
  const base::FilePath sample_file =
      external_dir.Append(FILE_PATH_LITERAL("sample_data.txt"));
  ASSERT_TRUE(base::WriteFile(sample_file, "sample_data"));

  ASSERT_TRUE(base::DeletePathRecursively(cache_dir_));
  ASSERT_TRUE(base::CreateSymbolicLink(external_dir, cache_dir_));

  EXPECT_FALSE(disk_cache::MoveCache(cache_dir_, dest_dir_));
  EXPECT_FALSE(
      disk_cache::MoveCache(cache_dir_.AsEndingWithSeparator(), dest_dir_));
  EXPECT_FALSE(base::PathExists(dest_dir_));

  // Pre-existing destination directory should fail without copying.
  ASSERT_TRUE(base::CreateDirectory(dest_dir_));
  EXPECT_FALSE(disk_cache::MoveCache(cache_dir_, dest_dir_));
  EXPECT_TRUE(base::IsDirectoryEmpty(dest_dir_));
  ASSERT_TRUE(base::DeletePathRecursively(dest_dir_));

  // Read-only parent directory causes rename to fail; MoveCache should return
  // false without falling back to copying.
  const base::FilePath ro_parent =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("ro_parent"));
  ASSERT_TRUE(base::CreateDirectory(ro_parent));
  const base::FilePath ro_dir = ro_parent.Append(FILE_PATH_LITERAL("CacheDir"));
  ASSERT_TRUE(base::CreateDirectory(ro_dir));
  ASSERT_TRUE(
      base::WriteFile(ro_dir.Append(FILE_PATH_LITERAL("entry01")), "data"));
#if BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(base::SetPosixFilePermissions(
      ro_dir, base::FILE_PERMISSION_READ_BY_USER |
                  base::FILE_PERMISSION_EXECUTE_BY_USER));
#endif
  ASSERT_TRUE(base::SetPosixFilePermissions(
      ro_parent, base::FILE_PERMISSION_READ_BY_USER |
                     base::FILE_PERMISSION_EXECUTE_BY_USER));
  base::ScopedClosureRunner restore_perms(base::BindOnce(
      [](const base::FilePath& parent, const base::FilePath& child) {
        base::SetPosixFilePermissions(parent, base::FILE_PERMISSION_USER_MASK);
        base::SetPosixFilePermissions(child, base::FILE_PERMISSION_USER_MASK);
      },
      ro_parent, ro_dir));
  EXPECT_FALSE(disk_cache::MoveCache(ro_dir, dest_dir_));
  EXPECT_FALSE(
      base::PathExists(dest_dir_.Append(FILE_PATH_LITERAL("entry01"))));
  base::DeletePathRecursively(dest_dir_);

  // Destination symlink should also be rejected.
  const base::FilePath real_source =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("RealCache"));
  ASSERT_TRUE(base::CreateDirectory(real_source));
  ASSERT_TRUE(base::CreateSymbolicLink(
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("missing_target")),
      dest_dir_));
  EXPECT_FALSE(disk_cache::MoveCache(real_source, dest_dir_));

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(sample_file, &contents));
  EXPECT_EQ("sample_data", contents);
}

TEST_F(CacheUtilTest, CleanupDirectorySkipsSymlinkSlots) {
  const base::FilePath external_dir =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("external_dir"));
  ASSERT_TRUE(base::CreateDirectory(external_dir));
  const base::FilePath sample_file =
      external_dir.Append(FILE_PATH_LITERAL("sample_data.txt"));
  ASSERT_TRUE(base::WriteFile(sample_file, "sample_data"));

  ASSERT_TRUE(base::DeletePathRecursively(cache_dir_));
  ASSERT_TRUE(base::CreateSymbolicLink(external_dir, cache_dir_));

  {
    base::RunLoop run_loop;
    disk_cache::CleanupDirectory(cache_dir_,
                                 base::BindLambdaForTesting([&](bool result) {
                                   EXPECT_FALSE(result);
                                   run_loop.Quit();
                                 }));
    run_loop.Run();
  }
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::PathExists(
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("old_Cache_000"))));

  // Restore a real cache directory while old_Cache_000 is a dangling symlink.
  ASSERT_TRUE(base::DeletePathRecursively(cache_dir_));
  ASSERT_TRUE(base::CreateDirectory(cache_dir_));
  ASSERT_TRUE(base::WriteFile(file1_, "cached_entry"));
  const base::FilePath broken_temp_link =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("old_Cache_000"));
  ASSERT_TRUE(base::CreateSymbolicLink(
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("nonexistent")),
      broken_temp_link));

  base::RunLoop run_loop;
  disk_cache::CleanupDirectory(cache_dir_,
                               base::BindLambdaForTesting([&](bool result) {
                                 EXPECT_TRUE(result);
                                 run_loop.Quit();
                               }));
  run_loop.Run();
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(base::IsLink(broken_temp_link));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(sample_file, &contents));
  EXPECT_EQ("sample_data", contents);
}

TEST_F(CacheUtilTest, DeleteCacheHandlesSymlinkPaths) {
  const base::FilePath external_dir =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("external_dir"));
  ASSERT_TRUE(base::CreateDirectory(external_dir));
  const base::FilePath sample_file =
      external_dir.Append(FILE_PATH_LITERAL("sample_data.txt"));
  ASSERT_TRUE(base::WriteFile(sample_file, "sample_data"));

  ASSERT_TRUE(base::DeletePathRecursively(cache_dir_));
  ASSERT_TRUE(base::CreateSymbolicLink(external_dir, cache_dir_));

  EXPECT_FALSE(disk_cache::DeleteCache(cache_dir_, /*remove_folder=*/false));
  EXPECT_FALSE(disk_cache::DeleteCache(cache_dir_.AsEndingWithSeparator(),
                                       /*remove_folder=*/false));
  EXPECT_TRUE(base::IsLink(cache_dir_));
  EXPECT_TRUE(base::PathExists(sample_file));

  EXPECT_TRUE(disk_cache::DeleteCache(cache_dir_.AsEndingWithSeparator(),
                                      /*remove_folder=*/true));
  EXPECT_FALSE(base::IsLink(cache_dir_));
  EXPECT_TRUE(base::PathExists(sample_file));

  // Deleting contents of a real directory containing a child symlink and nested
  // subdirectory unlinks the link and removes the subdirectory without
  // affecting the link target.
  ASSERT_TRUE(base::CreateDirectory(cache_dir_));
  const base::FilePath child_link =
      cache_dir_.Append(FILE_PATH_LITERAL("child_link"));
  ASSERT_TRUE(base::CreateSymbolicLink(external_dir, child_link));
  const base::FilePath nested_dir =
      cache_dir_.Append(FILE_PATH_LITERAL("nested_dir"));
  ASSERT_TRUE(base::CreateDirectory(nested_dir));
  const base::FilePath nested_file =
      nested_dir.Append(FILE_PATH_LITERAL("nested_file"));
  ASSERT_TRUE(base::WriteFile(nested_file, "nested"));
  EXPECT_TRUE(disk_cache::DeleteCache(cache_dir_, /*remove_folder=*/false));
  EXPECT_TRUE(base::DirectoryExists(cache_dir_));
  EXPECT_FALSE(base::IsLink(child_link));
  EXPECT_FALSE(base::PathExists(nested_dir));
  EXPECT_TRUE(base::PathExists(sample_file));

  // Deleting contents of a dangling symlink or regular file returns false.
  const base::FilePath dangling_link =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("dangling_link"));
  ASSERT_TRUE(base::CreateSymbolicLink(
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("missing_target")),
      dangling_link));
  EXPECT_FALSE(disk_cache::DeleteCache(dangling_link, /*remove_folder=*/false));
  EXPECT_TRUE(base::IsLink(dangling_link));
  EXPECT_FALSE(disk_cache::DeleteCache(sample_file, /*remove_folder=*/false));
  EXPECT_TRUE(base::PathExists(sample_file));

  // Deleting contents of a non-existent directory succeeds as a no-op.
  EXPECT_TRUE(disk_cache::DeleteCache(
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("missing_cache")),
      /*remove_folder=*/false));
}

TEST_F(CacheUtilTest, AncestorSymlinkAllowed) {
  const base::FilePath real_parent =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("real_parent"));
  const base::FilePath symlink_parent =
      tmp_dir_.GetPath().Append(FILE_PATH_LITERAL("symlink_parent"));
  ASSERT_TRUE(base::CreateDirectory(real_parent));
  ASSERT_TRUE(base::CreateSymbolicLink(real_parent, symlink_parent));

  const base::FilePath cache_under_link =
      symlink_parent.Append(FILE_PATH_LITERAL("SubCache"));
  ASSERT_TRUE(base::CreateDirectory(cache_under_link));
  const base::FilePath entry_file =
      cache_under_link.Append(FILE_PATH_LITERAL("entry01"));
  ASSERT_TRUE(base::WriteFile(entry_file, "entry_data"));

  EXPECT_TRUE(
      disk_cache::DeleteCache(cache_under_link, /*remove_folder=*/false));
  EXPECT_TRUE(base::DirectoryExists(cache_under_link));
  EXPECT_FALSE(base::PathExists(entry_file));

  ASSERT_TRUE(base::WriteFile(entry_file, "entry_data"));
  const base::FilePath moved_cache =
      symlink_parent.Append(FILE_PATH_LITERAL("MovedSubCache"));
  EXPECT_TRUE(disk_cache::MoveCache(cache_under_link, moved_cache));
  EXPECT_TRUE(
      base::PathExists(moved_cache.Append(FILE_PATH_LITERAL("entry01"))));
}
#endif

TEST_F(CacheUtilTest, PreferredCacheSize) {
  // The size of the HTTP cache is multiplied by 4 by default on non-Windows.
  constexpr bool kHTTPCacheSizeIsIncreased =
#if BUILDFLAG(IS_WIN)
      false;
#else
      true;
#endif

  const struct TestCase {
    std::optional<base::ByteSize> available;
    base::ByteSize expected_100percent;
    base::ByteSize expected_200percent;
    base::ByteSize expected_250percent;
    base::ByteSize expected_300percent;
    base::ByteSize expected_400percent;
  } kTestCases[] = {
      {std::nullopt, base::MiB(80), base::MiB(160), base::MiB(200),
       base::MiB(240), base::MiB(320)},

      // 0 produces 0.
      {base::ByteSize(0), base::ByteSize(0), base::ByteSize(0),
       base::ByteSize(0), base::ByteSize(0), base::ByteSize(0)},

      // Cache is 80% of available space, when default cache size is larger than
      // 80% of available space..
      {base::MiB(50), base::MiB(40), base::MiB(40), base::MiB(40),
       base::MiB(40), base::MiB(40)},

      // Cache is default size, when default size is 10% to 80% of available
      // space.
      {base::MiB(100), base::MiB(80), base::MiB(80), base::MiB(80),
       base::MiB(80), base::MiB(80)},
      {base::MiB(200), base::MiB(80), base::MiB(80), base::MiB(80),
       base::MiB(80), base::MiB(80)},

      // Cache is 10% of available space if 2.5 * default size is more than 10%
      // of available space.
      {base::MiB(1000), base::MiB(100), base::MiB(200), base::MiB(200),
       base::MiB(200), base::MiB(200)},
      {base::MiB(2000), base::MiB(200), base::MiB(400), base::MiB(400),
       base::MiB(400), base::MiB(400)},

      // Cache is 2.5 * kDefaultCacheSize if 2.5 * kDefaultCacheSize uses from
      // 1% to 10% of available space.
      {base::MiB(10000), base::MiB(200), base::MiB(400), base::MiB(500),
       base::MiB(600), base::MiB(800)},

      // Otherwise, cache is 1% of available space.
      {base::MiB(20000), base::MiB(200), base::MiB(400), base::MiB(500),
       base::MiB(600), base::MiB(800)},

      // Until it runs into the cache size cap.
      {base::MiB(32000), base::MiB(320), base::MiB(640), base::MiB(800),
       base::MiB(960), base::MiB(1280)},
      {base::MiB(50000), base::MiB(320), base::MiB(640), base::MiB(800),
       base::MiB(960), base::MiB(1280)},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "test_case.available = " << test_case.available);

    const base::ByteSize expected = kHTTPCacheSizeIsIncreased
                                        ? test_case.expected_400percent
                                        : test_case.expected_100percent;
    EXPECT_EQ(expected, PreferredCacheSize(test_case.available));

    // Preferred size for WebUI code cache is the same as without trial, but
    // should never be more than 5 MB.
    base::ByteSize expected_webui_code_cache_size =
        std::min(base::MiB(5), expected);
    EXPECT_EQ(expected_webui_code_cache_size,
              PreferredCacheSize(test_case.available,
                                 net::GENERATED_WEBUI_BYTE_CODE_CACHE));
  }

  // Check that the cache size cap is 50% higher for native code caches.
  EXPECT_EQ(
      (base::MiB(320) / 2) * 3,
      PreferredCacheSize(base::MiB(50000), net::GENERATED_NATIVE_CODE_CACHE));

  for (int cache_size_experiment : {100, 200, 250, 300, 400}) {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["percent_relative_size"] =
        base::NumberToString(cache_size_experiment);
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        disk_cache::kChangeGeneratedCodeCacheSizeExperiment,
        field_trial_params);

    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(testing::Message()
                   << "test_case.available = " << test_case.available);

      base::ByteSize expected;
      switch (cache_size_experiment) {
        case 100:
          expected = test_case.expected_100percent;
          break;
        case 200:
          expected = test_case.expected_200percent;
          break;
        case 250:
          expected = test_case.expected_250percent;
          break;
        case 300:
          expected = test_case.expected_300percent;
          break;
        case 400:
          expected = test_case.expected_400percent;
          break;
        default:
          NOTREACHED();
      }

      // Generate byte code cache size is appropriately scaled.
      EXPECT_EQ(expected, PreferredCacheSize(test_case.available,
                                             net::GENERATED_BYTE_CODE_CACHE));

      // For caches other than generated code cache, the size is not scaled
      // through `kChangeGeneratedCodeCacheSizeExperiment`.
      base::ByteSize default_expected = kHTTPCacheSizeIsIncreased
                                            ? test_case.expected_400percent
                                            : test_case.expected_100percent;
      EXPECT_EQ(default_expected, PreferredCacheSize(test_case.available));

      // Preferred size for WebUI code cache is not scaled by the trial, and
      // should never be more than 5 MB.
      base::ByteSize expected_webui_code_cache_size =
          std::min(base::MiB(5), test_case.expected_100percent);
      EXPECT_EQ(expected_webui_code_cache_size,
                PreferredCacheSize(test_case.available,
                                   net::GENERATED_WEBUI_BYTE_CODE_CACHE));
    }

    // Check that the cache size cap is 50% higher for native code caches but is
    // not scaled for the experiment.
    EXPECT_EQ(
        (base::MiB(320) / 2) * 3,
        PreferredCacheSize(base::MiB(50000), net::GENERATED_NATIVE_CODE_CACHE));
  }

  // Check no explicit "percent_relative_size" matches default behavior.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        disk_cache::kChangeGeneratedCodeCacheSizeExperiment);
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(test_case.expected_400percent,
                PreferredCacheSize(test_case.available,
                                   net::GENERATED_BYTE_CODE_CACHE));
    }
    // Check that the cache size cap is 50% higher for native code caches.
    EXPECT_EQ(
        (base::MiB(320) / 2) * 3,
        PreferredCacheSize(base::MiB(50000), net::GENERATED_NATIVE_CODE_CACHE));
  }
}

}  // namespace disk_cache

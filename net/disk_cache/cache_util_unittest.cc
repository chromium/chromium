// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include <stdio.h>

#include <map>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/files/scoped_temp_dir.h"
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
    FILE *fp = base::OpenFile(file1_, "w");
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
  EXPECT_TRUE(base::PathExists(cache_dir_)); // old cache dir stays
#else
  EXPECT_FALSE(base::PathExists(cache_dir_)); // old cache is gone
#endif
  EXPECT_FALSE(base::PathExists(file1_));
  EXPECT_FALSE(base::PathExists(file2_));
  EXPECT_FALSE(base::PathExists(dir1_));
}

TEST_F(CacheUtilTest, DeleteCache) {
  disk_cache::DeleteCache(cache_dir_, false);
  EXPECT_TRUE(base::PathExists(cache_dir_)); // cache dir stays
  EXPECT_FALSE(base::PathExists(dir1_));
  EXPECT_FALSE(base::PathExists(file1_));
  EXPECT_FALSE(base::PathExists(file2_));
  EXPECT_FALSE(base::PathExists(file3_));
}

TEST_F(CacheUtilTest, DeleteCacheAndDir) {
  disk_cache::DeleteCache(cache_dir_, true);
  EXPECT_FALSE(base::PathExists(cache_dir_)); // cache dir is gone
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

  ASSERT_TRUE(base::SetPosixFilePermissions(cache_dir_, /*mode=*/0));
  disk_cache::CleanupDirectory(cache_dir_,
                               base::BindLambdaForTesting([&](bool result) {
                                 EXPECT_TRUE(result);
                                 run_loop.Quit();
                               }));
  run_loop.Run();
}
#endif

TEST_F(CacheUtilTest, PreferredCacheSize) {
  constexpr int64_t kMiBInBytes = 1024 * 1024;

  // The size of the HTTP cache is multiplied by 4 by default on non-Windows.
  constexpr bool kHTTPCacheSizeIsIncreased =
#if BUILDFLAG(IS_WIN)
      false;
#else
      true;
#endif

  const struct TestCase {
    int64_t available;
    int64_t expected_100percent;
    int64_t expected_200percent;
    int64_t expected_250percent;
    int64_t expected_300percent;
    int64_t expected_400percent;
  } kTestCases[] = {
      // Weird negative value for available --- return the "default"
      {-1000LL, 80 * kMiBInBytes, 160 * kMiBInBytes, 200 * kMiBInBytes,
       240 * kMiBInBytes, 320 * kMiBInBytes},
      {-1LL, 80 * kMiBInBytes, 160 * kMiBInBytes, 200 * kMiBInBytes,
       240 * kMiBInBytes, 320 * kMiBInBytes},

      // 0 produces 0.
      {0LL, 0, 0, 0, 0, 0},

      // Cache is 80% of available space, when default cache size is larger than
      // 80% of available space..
      {50 * kMiBInBytes, 40 * kMiBInBytes, 40 * kMiBInBytes, 40 * kMiBInBytes,
       40 * kMiBInBytes, 40 * kMiBInBytes},
      // Cache is default size, when default size is 10% to 80% of available
      // space.
      {100 * kMiBInBytes, 80 * kMiBInBytes, 80 * kMiBInBytes, 80 * kMiBInBytes,
       80 * kMiBInBytes, 80 * kMiBInBytes},
      {200 * kMiBInBytes, 80 * kMiBInBytes, 80 * kMiBInBytes, 80 * kMiBInBytes,
       80 * kMiBInBytes, 80 * kMiBInBytes},
      // Cache is 10% of available space if 2.5 * default size is more than 10%
      // of available space.
      {1000 * kMiBInBytes, 100 * kMiBInBytes, 200 * kMiBInBytes,
       200 * kMiBInBytes, 200 * kMiBInBytes, 200 * kMiBInBytes},
      {2000 * kMiBInBytes, 200 * kMiBInBytes, 400 * kMiBInBytes,
       400 * kMiBInBytes, 400 * kMiBInBytes, 400 * kMiBInBytes},
      // Cache is 2.5 * kDefaultCacheSize if 2.5 * kDefaultCacheSize uses from
      // 1% to 10% of available space.
      {10000 * kMiBInBytes, 200 * kMiBInBytes, 400 * kMiBInBytes,
       500 * kMiBInBytes, 600 * kMiBInBytes, 800 * kMiBInBytes},
      // Otherwise, cache is 1% of available space.
      {20000 * kMiBInBytes, 200 * kMiBInBytes, 400 * kMiBInBytes,
       500 * kMiBInBytes, 600 * kMiBInBytes, 800 * kMiBInBytes},
      // Until it runs into the cache size cap.
      {32000 * kMiBInBytes, 320 * kMiBInBytes, 640 * kMiBInBytes,
       800 * kMiBInBytes, 960 * kMiBInBytes, 1280 * kMiBInBytes},
      {50000 * kMiBInBytes, 320 * kMiBInBytes, 640 * kMiBInBytes,
       800 * kMiBInBytes, 960 * kMiBInBytes, 1280 * kMiBInBytes},
  };

  for (const auto& test_case : kTestCases) {
    const int64_t expected = kHTTPCacheSizeIsIncreased
                                 ? test_case.expected_400percent
                                 : test_case.expected_100percent;
    EXPECT_EQ(expected, PreferredCacheSize(test_case.available))
        << test_case.available;

    // Preferred size for WebUI code cache is the same as without trial, but
    // should never be more than 5 MB.
    int expected_webui_code_cache_size = std::min(5 * kMiBInBytes, expected);
    EXPECT_EQ(expected_webui_code_cache_size,
              PreferredCacheSize(test_case.available,
                                 net::GENERATED_WEBUI_BYTE_CODE_CACHE))
        << test_case.available;
  }

  // Check that the cache size cap is 50% higher for native code caches.
  EXPECT_EQ(((320 * kMiBInBytes) / 2) * 3,
            PreferredCacheSize(50000 * kMiBInBytes,
                               net::GENERATED_NATIVE_CODE_CACHE));

  for (int cache_size_exeriment : {100, 200, 250, 300, 400}) {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["percent_relative_size"] =
        base::NumberToString(cache_size_exeriment);
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        disk_cache::kChangeGeneratedCodeCacheSizeExperiment,
        field_trial_params);

    for (const auto& test_case : kTestCases) {
      int expected = 0;
      switch (cache_size_exeriment) {
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
      int64_t default_expected = kHTTPCacheSizeIsIncreased
                                     ? test_case.expected_400percent
                                     : test_case.expected_100percent;
      EXPECT_EQ(default_expected, PreferredCacheSize(test_case.available));

      // Preferred size for WebUI code cache is not scaled by the trial, and
      // should never be more than 5 MB.
      int expected_webui_code_cache_size =
          std::min(5 * kMiBInBytes, test_case.expected_100percent);
      EXPECT_EQ(expected_webui_code_cache_size,
                PreferredCacheSize(test_case.available,
                                   net::GENERATED_WEBUI_BYTE_CODE_CACHE))
          << test_case.available;
    }

    // Check that the cache size cap is 50% higher for native code caches but is
    // not scaled for the experiment.
    EXPECT_EQ(((320 * kMiBInBytes) / 2) * 3,
              PreferredCacheSize(50000 * kMiBInBytes,
                                 net::GENERATED_NATIVE_CODE_CACHE));
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
    EXPECT_EQ(((320 * kMiBInBytes) / 2) * 3,
              PreferredCacheSize(50000 * kMiBInBytes,
                                 net::GENERATED_NATIVE_CODE_CACHE));
  }
}

}  // namespace disk_cache

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <map>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "net/disk_cache/cache_util.h"
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
};

TEST_F(CacheUtilTest, MoveCache) {
  EXPECT_TRUE(disk_cache::MoveCache(cache_dir_, dest_dir_));
  EXPECT_TRUE(base::PathExists(dest_dir_));
  EXPECT_TRUE(base::PathExists(dest_file1_));
  EXPECT_TRUE(base::PathExists(dest_file2_));
  EXPECT_TRUE(base::PathExists(dest_dir1_));
#if defined(OS_CHROMEOS)
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

TEST_F(CacheUtilTest, DeleteCacheFile) {
  EXPECT_TRUE(disk_cache::DeleteCacheFile(file1_));
  EXPECT_FALSE(base::PathExists(file1_));
  EXPECT_TRUE(base::PathExists(cache_dir_)); // cache dir stays
  EXPECT_TRUE(base::PathExists(dir1_));
  EXPECT_TRUE(base::PathExists(file3_));
}

TEST_F(CacheUtilTest, PreferredCacheSize) {
  const struct TestCase {
    int64_t available;
    int expected_without_trial;
    int expected_with_200_trial;
  } kTestCases[] = {
      // Cache is 80% of available space, when default cache size is larger than
      // 80% of available space..
      {50 * 1024 * 1024LL, 40 * 1024 * 1024, 40 * 1024 * 1024},
      // Cache is default size, when default size is 10% to 80% of available
      // space.
      {100 * 1024 * 1024LL, 80 * 1024 * 1024, 80 * 1024 * 1024},
      {200 * 1024 * 1024LL, 80 * 1024 * 1024, 80 * 1024 * 1024},
      // Same case as above, but the size is now less than 20% of available
      // space, so the trial increases cache size, though not yet doubling it.
      {500 * 1024 * 1024LL, 80 * 1024 * 1024, 100 * 1024 * 1024},
      // Cache is 10% of available space if 2.5 * default size is more than 10%
      // of available space.
      {1000 * 1024 * 1024LL, 100 * 1024 * 1024, 200 * 1024 * 1024},
      {2000 * 1024 * 1024LL, 200 * 1024 * 1024, 400 * 1024 * 1024},
      // Cache is 2.5 * kDefaultCacheSize if 2.5 * kDefaultCacheSize uses from
      // 1% to 10% of available space.
      {10000 * 1024 * 1024LL, 200 * 1024 * 1024, 400 * 1024 * 1024},
      // Otherwise, cache is 1% of available space.
      {20000 * 1024 * 1024LL, 200 * 1024 * 1024, 400 * 1024 * 1024},
      // Until it runs into the cache size cap.
      {32000 * 1024 * 1024LL, 320 * 1024 * 1024, 640 * 1024 * 1024},
      {50000 * 1024 * 1024LL, 320 * 1024 * 1024, 640 * 1024 * 1024},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expected_without_trial,
              PreferredCacheSize(test_case.available));
  }

  // Check 100 "percent_relative_size" matches default behavior.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["percent_relative_size"] = "100";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        disk_cache::kChangeDiskCacheSizeExperiment, field_trial_params);
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(test_case.expected_without_trial,
                PreferredCacheSize(test_case.available));
    }
  }

  // Check 200 "percent_relative_size".
  {
    base::test::ScopedFeatureList scoped_feature_list;
    std::map<std::string, std::string> field_trial_params;
    field_trial_params["percent_relative_size"] = "200";
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        disk_cache::kChangeDiskCacheSizeExperiment, field_trial_params);
    for (const auto& test_case : kTestCases) {
      EXPECT_EQ(test_case.expected_with_200_trial,
                PreferredCacheSize(test_case.available));
    }
  }
}

}  // namespace disk_cache

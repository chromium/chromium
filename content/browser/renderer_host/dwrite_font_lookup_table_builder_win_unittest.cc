// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_lookup_table_builder_win.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"

namespace content {

namespace {

struct FontExpectation {
  const char font_name[64];
  uint16_t ttc_index;
};

constexpr FontExpectation kExpectedTestFonts[] = {{u8"CambriaMath", 1},
                                                  {u8"Ming-Lt-HKSCS-ExtB", 2},
                                                  {u8"NSimSun", 1},
                                                  {u8"calibri-bolditalic", 0}};

constexpr base::TimeDelta kTestingTimeout = base::TimeDelta::FromSeconds(10);

class DWriteFontLookupTableBuilderTest : public testing::Test {
 public:
  DWriteFontLookupTableBuilderTest() {
    feature_list_.InitAndEnableFeature(features::kFontSrcLocalMatching);
  }

  void SetUp() override {
    font_lookup_table_builder_ = DWriteFontLookupTableBuilder::GetInstance();
    font_lookup_table_builder_->OverrideDWriteVersionChecksForTesting();
    font_lookup_table_builder_->ResetLookupTableForTesting();
    bool temp_dir_created = scoped_temp_dir_.CreateUniqueTempDir();
    ASSERT_TRUE(temp_dir_created);
    font_lookup_table_builder_->SetCacheDirectoryForTesting(
        scoped_temp_dir_.GetPath());
  }

  void TearDown() override {
    font_lookup_table_builder_->ResetStateForTesting();
  }

  void TestMatchFonts() {
    base::ReadOnlySharedMemoryRegion font_table_memory =
        font_lookup_table_builder_->DuplicateMemoryRegion();
    blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

    for (auto& test_font_name_index : kExpectedTestFonts) {
      base::Optional<blink::FontTableMatcher::MatchResult> match_result =
          font_table_matcher.MatchName(test_font_name_index.font_name);
      ASSERT_TRUE(match_result) << "No font matched for font name: "
                                << test_font_name_index.font_name;
      base::File unique_font_file(
          base::FilePath::FromUTF8Unsafe(match_result->font_path),
          base::File::FLAG_OPEN | base::File::FLAG_READ);
      ASSERT_TRUE(unique_font_file.IsValid());
      ASSERT_GT(unique_font_file.GetLength(), 0);
      ASSERT_EQ(test_font_name_index.ttc_index, match_result->ttc_index);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  DWriteFontLookupTableBuilder* font_lookup_table_builder_;
  base::ScopedTempDir scoped_temp_dir_;
};

class DWriteFontLookupTableBuilderTimeoutTest
    : public DWriteFontLookupTableBuilderTest,
      public ::testing::WithParamInterface<
          DWriteFontLookupTableBuilder::SlowDownMode> {};

}  // namespace

// Run a test similar to DWriteFontProxyImplUnitTest, TestFindUniqueFont but
// without going through Mojo and running it on the DWRiteFontLookupTableBuilder
// class directly.
TEST_F(DWriteFontLookupTableBuilderTest, TestFindUniqueFontDirect) {
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTableIfNeeded();
  bool test_callback_executed = false;
  font_lookup_table_builder_->QueueShareMemoryRegionWhenReady(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindLambdaForTesting(
          [this, &test_callback_executed](base::ReadOnlySharedMemoryRegion) {
            TestMatchFonts();
            test_callback_executed = true;
          }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(test_callback_executed);
}

TEST_P(DWriteFontLookupTableBuilderTimeoutTest, TestTimeout) {
  font_lookup_table_builder_->SetSlowDownIndexingForTestingWithTimeout(
      GetParam(), kTestingTimeout);
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTableIfNeeded();
  bool test_callback_executed = false;
  font_lookup_table_builder_->QueueShareMemoryRegionWhenReady(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindLambdaForTesting([this, &test_callback_executed](
                                     base::ReadOnlySharedMemoryRegion
                                         font_table_memory) {
        blink::FontTableMatcher font_table_matcher(font_table_memory.Map());

        for (auto& test_font_name_index : kExpectedTestFonts) {
          base::Optional<blink::FontTableMatcher::MatchResult> match_result =
              font_table_matcher.MatchName(test_font_name_index.font_name);
          ASSERT_TRUE(!match_result);
        }
        if (GetParam() ==
            DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask)
          font_lookup_table_builder_->ResumeFromHangForTesting();
        test_callback_executed = true;
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(test_callback_executed);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DWriteFontLookupTableBuilderTimeoutTest,
    ::testing::Values(
        DWriteFontLookupTableBuilder::SlowDownMode::kDelayEachTask,
        DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask));

TEST_F(DWriteFontLookupTableBuilderTest, TestReadyEarly) {
  font_lookup_table_builder_->SetSlowDownIndexingForTestingWithTimeout(
      DWriteFontLookupTableBuilder::SlowDownMode::kHangOneTask,
      kTestingTimeout);

  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTableIfNeeded();
  bool test_callback_executed = false;
  font_lookup_table_builder_->QueueShareMemoryRegionWhenReady(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindLambdaForTesting(
          [this, &test_callback_executed](base::ReadOnlySharedMemoryRegion) {
            ASSERT_TRUE(font_lookup_table_builder_->FontUniqueNameTableReady());
            test_callback_executed = true;
          }));
  ASSERT_FALSE(font_lookup_table_builder_->FontUniqueNameTableReady());
  font_lookup_table_builder_->ResumeFromHangForTesting();
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(test_callback_executed);
}

TEST_F(DWriteFontLookupTableBuilderTest, RepeatedScheduling) {
  for (unsigned i = 0; i < 3; ++i) {
    font_lookup_table_builder_->ResetLookupTableForTesting();
    font_lookup_table_builder_->SetCachingEnabledForTesting(false);
    font_lookup_table_builder_->SchedulePrepareFontUniqueNameTableIfNeeded();
    bool test_callback_executed = false;
    font_lookup_table_builder_->QueueShareMemoryRegionWhenReady(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindLambdaForTesting(
            [&test_callback_executed](base::ReadOnlySharedMemoryRegion) {
              test_callback_executed = true;
            }));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(test_callback_executed);
  }
}

TEST_F(DWriteFontLookupTableBuilderTest, FontsHash) {
  ASSERT_GT(font_lookup_table_builder_->ComputePersistenceHash().size(), 0u);
}

TEST_F(DWriteFontLookupTableBuilderTest, HandleCorruptCacheFile) {
  // Cycle once to build cache file.
  font_lookup_table_builder_->ResetLookupTableForTesting();
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTableIfNeeded();

  bool test_callback_executed = false;
  base::File cache_file;
  font_lookup_table_builder_->QueueShareMemoryRegionWhenReady(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindLambdaForTesting([this, &cache_file, &test_callback_executed](
                                     base::ReadOnlySharedMemoryRegion) {
        ASSERT_TRUE(font_lookup_table_builder_->FontUniqueNameTableReady());
        // Truncate table for testing
        base::FilePath cache_file_path = scoped_temp_dir_.GetPath().Append(
            FILE_PATH_LITERAL("font_unique_name_table.pb"));
        // Use FLAG_EXCLUSIVE_WRITE to block file and make persisting the
        // cache fail as well, use FLAG_OPEN to ensure it got created by the
        // table builder implementation.
        cache_file = base::File(cache_file_path,
                                base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WRITE |
                                    base::File::FLAG_EXCLUSIVE_WRITE);
        // Ensure the cache file was created in the empty scoped_temp_dir_
        // and has a non-zero length.
        ASSERT_TRUE(cache_file.IsValid());
        ASSERT_TRUE(cache_file.GetLength() > 0);
        ASSERT_TRUE(cache_file.SetLength(cache_file.GetLength() / 2));
        ASSERT_TRUE(cache_file.SetLength(cache_file.GetLength() * 2));
        test_callback_executed = true;
      }));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(test_callback_executed);

  // Reload the cache file.
  font_lookup_table_builder_->ResetLookupTableForTesting();
  font_lookup_table_builder_->SchedulePrepareFontUniqueNameTableIfNeeded();

  test_callback_executed = false;
  font_lookup_table_builder_->QueueShareMemoryRegionWhenReady(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindLambdaForTesting(
          [this, &test_callback_executed](base::ReadOnlySharedMemoryRegion) {
            TestMatchFonts();
            test_callback_executed = true;
          }));

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(test_callback_executed);

  // Ensure that the table is still valid even though persisting has failed
  // due to the exclusive write lock on the file.
  ASSERT_TRUE(font_lookup_table_builder_->FontUniqueNameTableReady());
}

}  // namespace content

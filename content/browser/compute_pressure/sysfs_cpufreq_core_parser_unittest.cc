// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/sysfs_cpufreq_core_parser.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_piece_forward.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

enum class FrequencyType {
  kMax = 0,
  kMin = 1,
  kBase = 2,
  kCurrent = 3,
};

class SysfsCpufreqCoreParserTest : public testing::Test {
 public:
  ~SysfsCpufreqCoreParserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_core_path_ = temp_dir_.GetPath();
    parser_ = std::make_unique<SysfsCpufreqCoreParser>(fake_core_path_);
  }

  [[nodiscard]] bool WriteFakeFile(base::StringPiece file_name,
                                   base::StringPiece contents) {
    base::FilePath file_path = fake_core_path_.AppendASCII(file_name);
    return base::WriteFile(file_path, contents);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath fake_core_path_;
  std::unique_ptr<SysfsCpufreqCoreParser> parser_;
};

TEST_F(SysfsCpufreqCoreParserTest, CorePath) {
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("/cpu/cpu0/cpufreq")),
            SysfsCpufreqCoreParser::CorePath(0, FILE_PATH_LITERAL("/cpu/cpu")));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("/cpu/cpu5/cpufreq")),
            SysfsCpufreqCoreParser::CorePath(5, FILE_PATH_LITERAL("/cpu/cpu")));
  EXPECT_EQ(
      base::FilePath(FILE_PATH_LITERAL("/cpu/cpu999/cpufreq")),
      SysfsCpufreqCoreParser::CorePath(999, FILE_PATH_LITERAL("/cpu/cpu")));
  EXPECT_EQ(base::FilePath(FILE_PATH_LITERAL("/cpu/cpu2147483647/cpufreq")),
            SysfsCpufreqCoreParser::CorePath(2147483647, "/cpu/cpu"));
}

TEST_F(SysfsCpufreqCoreParserTest, NoFile) {
  EXPECT_EQ(-1, parser_->ReadMaxFrequency());
  EXPECT_EQ(-1, parser_->ReadMinFrequency());
  EXPECT_EQ(-1, parser_->ReadBaseFrequency());
  EXPECT_EQ(-1, parser_->ReadCurrentFrequency());
}

TEST_F(SysfsCpufreqCoreParserTest, GovernorFiles) {
  ASSERT_TRUE(WriteFakeFile("scaling_max_freq", "1234000\n"));
  ASSERT_TRUE(WriteFakeFile("scaling_min_freq", "5678000\n"));
  ASSERT_TRUE(WriteFakeFile("scaling_cur_freq", "9876000\n"));
  ASSERT_TRUE(WriteFakeFile("base_frequency", "5432000\n"));
  EXPECT_EQ(1'234'000'000, parser_->ReadMaxFrequency());
  EXPECT_EQ(5'678'000'000, parser_->ReadMinFrequency());
  EXPECT_EQ(9'876'000'000, parser_->ReadCurrentFrequency());
  EXPECT_EQ(5'432'000'000, parser_->ReadBaseFrequency());
}

TEST_F(SysfsCpufreqCoreParserTest, FirmwareFiles) {
  ASSERT_TRUE(WriteFakeFile("cpuinfo_max_freq", "1234000\n"));
  ASSERT_TRUE(WriteFakeFile("cpuinfo_min_freq", "5678000\n"));
  ASSERT_TRUE(WriteFakeFile("cpuinfo_cur_freq", "9876000\n"));
  EXPECT_EQ(1'234'000'000, parser_->ReadMaxFrequency());
  EXPECT_EQ(5'678'000'000, parser_->ReadMinFrequency());
  EXPECT_EQ(9'876'000'000, parser_->ReadCurrentFrequency());
  EXPECT_EQ(-1, parser_->ReadBaseFrequency());
}

TEST_F(SysfsCpufreqCoreParserTest, ProductionData_NoCrash) {
  base::FilePath production_path = SysfsCpufreqCoreParser::CorePath(
      0, SysfsCpufreqCoreParser::kSysfsCpuPath);
  SysfsCpufreqCoreParser parser(production_path);

  // CPUFreq does not appear to be enabled on bots. This test has different
  // assertions, depending on whether CPUFreq exists or not.
  bool sysfs_cpufreq_exists = base::DirectoryExists(production_path);

  SCOPED_TRACE(testing::Message()
               << "Path: " << production_path << " exists: "
               << sysfs_cpufreq_exists << " min: " << parser.ReadMinFrequency()
               << " max: " << parser.ReadMaxFrequency()
               << " base: " << parser.ReadBaseFrequency()
               << " current: " << parser.ReadCurrentFrequency());

  if (sysfs_cpufreq_exists) {
    EXPECT_GE(parser.ReadMaxFrequency(), 0);
    EXPECT_GE(parser.ReadMinFrequency(), 0);
    EXPECT_GE(parser.ReadCurrentFrequency(), 0);
  } else {
    EXPECT_EQ(parser.ReadMaxFrequency(), -1);
    EXPECT_EQ(parser.ReadMinFrequency(), -1);
    EXPECT_EQ(parser.ReadCurrentFrequency(), -1);
  }

  // The base frequency is not reported on many systems. So, we only expect that
  // the reader follows its API (and doesn't crash).
  EXPECT_GE(parser.ReadBaseFrequency(), -1);
}

class SysfsCpufreqCoreParserRoutingTest
    : public SysfsCpufreqCoreParserTest,
      public testing::WithParamInterface<
          std::tuple<FrequencyType, bool, bool>> {
 public:
  FrequencyType frequency_type() { return std::get<0>(GetParam()); }
  bool firmware_file_is_present() { return std::get<1>(GetParam()); }
  bool governor_file_is_present() { return std::get<1>(GetParam()); }

  bool WriteFakeFiles(base::StringPiece firmware_contents,
                      base::StringPiece governor_contents) {
    switch (frequency_type()) {
      case FrequencyType::kMax:
        if (firmware_file_is_present()) {
          if (!WriteFakeFile("cpuinfo_max_freq", firmware_contents))
            return false;
        }
        if (governor_file_is_present()) {
          if (!WriteFakeFile("scaling_max_freq", governor_contents))
            return false;
        }
        return true;

      case FrequencyType::kMin:
        if (firmware_file_is_present()) {
          if (!WriteFakeFile("cpuinfo_min_freq", firmware_contents))
            return false;
        }
        if (governor_file_is_present()) {
          if (!WriteFakeFile("scaling_min_freq", governor_contents))
            return false;
        }
        return true;

      case FrequencyType::kBase:
        if (firmware_file_is_present()) {
          if (!WriteFakeFile("base_frequency", firmware_contents))
            return false;
        } else if (governor_file_is_present()) {
          if (!WriteFakeFile("base_frequency", governor_contents))
            return false;
        }
        return true;

      case FrequencyType::kCurrent:
        if (firmware_file_is_present()) {
          if (!WriteFakeFile("cpuinfo_cur_freq", firmware_contents))
            return false;
        }
        if (governor_file_is_present()) {
          if (!WriteFakeFile("scaling_cur_freq", governor_contents))
            return false;
        }
        return true;
    }
  }
};

TEST_P(SysfsCpufreqCoreParserRoutingTest, Read) {
  WriteFakeFiles("1234000\n", "56780000\n");

  int64_t expected_value =
      firmware_file_is_present()
          ? 1'234'000'000
          : (governor_file_is_present() ? 5'678'000'000 : -1);

  EXPECT_EQ(frequency_type() == FrequencyType::kMax ? expected_value : -1,
            parser_->ReadMaxFrequency());
  EXPECT_EQ(frequency_type() == FrequencyType::kMin ? expected_value : -1,
            parser_->ReadMinFrequency());
  EXPECT_EQ(frequency_type() == FrequencyType::kBase ? expected_value : -1,
            parser_->ReadBaseFrequency());
  EXPECT_EQ(frequency_type() == FrequencyType::kCurrent ? expected_value : -1,
            parser_->ReadCurrentFrequency());
}

TEST_P(SysfsCpufreqCoreParserRoutingTest, Read_InvalidFirmwareFile) {
  std::vector<const char*> invalid_numbers = {
      "-1",
      "-100",
      "-",
      "a",
      "1a",
      "1234a",
      u8"\U0001f41b",
      u8"1\U0001f41b",
      u8"\U0001f41b1",
  };

  for (const char* invalid_number : invalid_numbers) {
    WriteFakeFiles(invalid_number, "5678000\n");

    int64_t expected_value = governor_file_is_present() ? 5'678'000'000 : -1;

    EXPECT_EQ(frequency_type() == FrequencyType::kMax ? expected_value : -1,
              parser_->ReadMaxFrequency());
    EXPECT_EQ(frequency_type() == FrequencyType::kMin ? expected_value : -1,
              parser_->ReadMinFrequency());
    EXPECT_EQ(frequency_type() == FrequencyType::kCurrent ? expected_value : -1,
              parser_->ReadCurrentFrequency());

    // There is only one base frequency file. The test fixture contains the
    // firmware string. So, it will always be invalid in this test.
    EXPECT_EQ(-1, parser_->ReadBaseFrequency());
  }
}

TEST_P(SysfsCpufreqCoreParserRoutingTest, Read_InvalidGovernorFile) {
  std::vector<const char*> invalid_numbers = {
      "-1",
      "-100",
      "-",
      "a",
      "1a",
      "1234a",
      u8"\U0001f41b",
      u8"1\U0001f41b",
      u8"\U0001f41b1",
  };

  for (const char* invalid_number : invalid_numbers) {
    WriteFakeFiles("1234000\n", invalid_number);

    int64_t expected_value = firmware_file_is_present() ? 1'234'000'000 : -1;

    EXPECT_EQ(frequency_type() == FrequencyType::kMax ? expected_value : -1,
              parser_->ReadMaxFrequency());
    EXPECT_EQ(frequency_type() == FrequencyType::kMin ? expected_value : -1,
              parser_->ReadMinFrequency());
    EXPECT_EQ(frequency_type() == FrequencyType::kBase ? expected_value : -1,
              parser_->ReadBaseFrequency());
    EXPECT_EQ(frequency_type() == FrequencyType::kCurrent ? expected_value : -1,
              parser_->ReadCurrentFrequency());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SysfsCpufreqCoreParserRoutingTest,
    testing::Combine(testing::Values(FrequencyType::kMax,
                                     FrequencyType::kMin,
                                     FrequencyType::kBase,
                                     FrequencyType::kCurrent),
                     testing::Bool(),
                     testing::Bool()));

}  // namespace

}  // namespace content

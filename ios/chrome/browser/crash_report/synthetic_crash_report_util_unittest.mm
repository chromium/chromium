// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/synthetic_crash_report_util.h"

#import <Foundation/Foundation.h>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#import "ios/chrome/browser/metrics/previous_session_info_private.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SyntheticCrashReportUtilTest : public PlatformTest {
 protected:
  ~SyntheticCrashReportUtilTest() override {
    [PreviousSessionInfo resetSharedInstanceForTesting];
  }
};

// Tests that CreateSyntheticCrashReportForUte correctly generates config and
// minidump files.
TEST_F(SyntheticCrashReportUtilTest, CreateSyntheticCrashReportForUte) {
  [PreviousSessionInfo resetSharedInstanceForTesting];
  const NSInteger kAvailableStorage = 256;
  PreviousSessionInfo* previous_session = [PreviousSessionInfo sharedInstance];
  previous_session.availableDeviceStorage = kAvailableStorage;
  previous_session.didSeeMemoryWarningShortlyBeforeTerminating = YES;

  // Create crash report.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string product_display = std::string(255, 'a') + std::string(1, 'b');
  CreateSyntheticCrashReportForUte(temp_dir.GetPath(), product_display,
                                   "Product", "Version", "URL");
  // CreateSyntheticCrashReportForUte creates config and empty minidump file.
  // locate both files and ensure there are no other files in the directory.
  base::FileEnumerator traversal(temp_dir.GetPath(), /*recursive=*/false,
                                 base::FileEnumerator::FILES);
  base::FilePath new_file_path = traversal.Next();
  base::FilePath config_file_path;
  base::FilePath minidump_file_path;
  if (new_file_path.Extension() == ".dmp") {
    // First file is minidump. Next one should be config.
    minidump_file_path = new_file_path;
    config_file_path = traversal.Next();
  } else {
    // First file is confix. Next one should be minidump.
    config_file_path = new_file_path;
    minidump_file_path = traversal.Next();
  }
  ASSERT_EQ("", traversal.Next().value());

  // Config file name is "Config-<6 random characters>" (f.e. Config-S0Zl1r).
  std::vector<std::string> config_file_path_components;
  config_file_path.GetComponents(&config_file_path_components);
  ASSERT_FALSE(config_file_path_components.empty());
  std::string config_file_name = config_file_path_components.back();
  ASSERT_EQ(13U, config_file_name.size()) << config_file_name;
  EXPECT_EQ(0U, config_file_name.find("Config-"));

  // Minidump file name is "<UUID>.dmp" (f.e.
  // f83dfc0a-771e-4a99-8540-e430ab995307.dmp).
  std::vector<std::string> minidump_file_path_components;
  minidump_file_path.GetComponents(&minidump_file_path_components);
  ASSERT_FALSE(minidump_file_path_components.empty());
  std::string minidump_file_name = minidump_file_path_components.back();
  ASSERT_EQ(40U, minidump_file_name.size()) << minidump_file_name;

  // Read config file
  base::File config_file(config_file_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(config_file.IsValid());
  ASSERT_GE(config_file.GetLength(), 0U);

  std::vector<uint8_t> data;
  data.resize(config_file.GetLength());
  ASSERT_TRUE(config_file.ReadAndCheck(/*offset=*/0, data));
  std::string config_content(data.begin(), data.end());
  std::vector<std::string> config_lines = base::SplitString(
      config_content, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Verify config file content. Config file has the following format:
  // <Key1>\n<Value1Length>\n<Value1>\n...<KeyN>\n<ValueNLength>\n<ValueN>
  ASSERT_EQ(33U, config_lines.size())
      << "<content>" << config_content << "</content>";

  EXPECT_EQ("MinidumpDir", config_lines[0]);
  EXPECT_EQ(base::NumberToString(temp_dir.GetPath().value().size()),
            config_lines[1]);
  EXPECT_EQ(temp_dir.GetPath().value(), config_lines[2]);

  // Verify that MinidumpID is a proper UUID and used in minidump file name.
  EXPECT_EQ("MinidumpID", config_lines[3]);
  EXPECT_EQ("36", config_lines[4]);
  EXPECT_TRUE([[NSUUID alloc]
      initWithUUIDString:base::SysUTF8ToNSString(config_lines[5])]);
  EXPECT_EQ(0U, minidump_file_name.find(config_lines[5]));

  // BreakpadProductDisplay value is too long and split into 2 chunks.
  EXPECT_EQ("BreakpadProductDisplay__1", config_lines[6]);
  EXPECT_EQ("255", config_lines[7]);
  EXPECT_EQ(product_display, config_lines[8] + config_lines[11]);
  EXPECT_EQ("BreakpadProductDisplay__2", config_lines[9]);
  EXPECT_EQ("1", config_lines[10]);

  EXPECT_EQ("BreakpadProduct", config_lines[12]);
  EXPECT_EQ("7", config_lines[13]);
  EXPECT_EQ("Product", config_lines[14]);

  EXPECT_EQ("BreakpadVersion", config_lines[15]);
  EXPECT_EQ("7", config_lines[16]);
  EXPECT_EQ("Version", config_lines[17]);

  EXPECT_EQ("BreakpadURL", config_lines[18]);
  EXPECT_EQ("3", config_lines[19]);
  EXPECT_EQ("URL", config_lines[20]);

  EXPECT_EQ("BreakpadMinidumpLocation", config_lines[21]);
  EXPECT_EQ(base::NumberToString(temp_dir.GetPath().value().size()),
            config_lines[22]);
  EXPECT_EQ(temp_dir.GetPath().value(), config_lines[23]);

  EXPECT_EQ("BreakpadServerParameterPrefix_free_disk_in_kb", config_lines[24]);
  EXPECT_EQ(
      base::NumberToString(base::NumberToString(kAvailableStorage).size()),
      config_lines[25]);
  EXPECT_EQ(base::NumberToString(kAvailableStorage), config_lines[26]);

  EXPECT_EQ("BreakpadServerParameterPrefix_memory_warning_in_progress",
            config_lines[27]);
  const char kYesString[] = "yes";
  EXPECT_EQ(base::NumberToString(strlen(kYesString)), config_lines[28]);
  EXPECT_EQ(kYesString, config_lines[29]);

  EXPECT_EQ("BreakpadServerParameterPrefix_platform", config_lines[30]);
  EXPECT_EQ(base::NumberToString(base::SysInfo::HardwareModelName().size()),
            config_lines[31]);
  EXPECT_EQ(base::SysInfo::HardwareModelName(), config_lines[32]);

  // Read minidump file. It must be empty as there is no stack trace, but
  // Breakpad will not upload config without minidump file.
  base::File minidump_file(minidump_file_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(minidump_file.IsValid());
  EXPECT_EQ(0U, minidump_file.GetLength());
}

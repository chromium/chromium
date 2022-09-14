// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/synthetic_crash_report_util.h"

#import <Foundation/Foundation.h>

#import "base/files/file.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "base/path_service.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "base/system/sys_info.h"
#import "components/previous_session_info/previous_session_info_private.h"
#import "testing/platform_test.h"

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
  NSString* const kOSVersion = @"OSVersion";
  previous_session.OSVersion = kOSVersion;
  previous_session.terminatedDuringSessionRestoration = YES;
  previous_session.applicationWillTerminateWasReceived = YES;
  NSString* const kURL = @"URL";
  previous_session.reportParameters = @{@"url" : kURL};
  previous_session.sessionStartTime = [NSDate date];
  const NSTimeInterval kUptimeMs = 5000;
  previous_session.sessionEndTime = [previous_session.sessionStartTime
      dateByAddingTimeInterval:kUptimeMs / 1000];
  const NSInteger kMemoryFootprint = 1278759;
  previous_session.memoryFootprint = kMemoryFootprint;

  // Create crash report.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  std::string product_display = std::string(255, 'a') + std::string(1, 'b');
  const char kBreadcrumb1[] = "52:43 Tab1 Zoom";
  const char kLastEvent[] = "Tab1 Scroll 1";
  std::string kBreadcrumb2 = std::string("52:46 ") + kLastEvent;

  CreateSyntheticCrashReportForUte(temp_dir.GetPath(), product_display,
                                   "Product", "Version", "URL",
                                   {kBreadcrumb1, kBreadcrumb2});
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
  std::vector<std::string> config_file_path_components =
      config_file_path.GetComponents();
  ASSERT_FALSE(config_file_path_components.empty());
  std::string config_file_name = config_file_path_components.back();
  ASSERT_EQ(13U, config_file_name.size()) << config_file_name;
  EXPECT_EQ(0U, config_file_name.find("Config-"));

  // Minidump file name is "<UUID>.dmp" (f.e.
  // f83dfc0a-771e-4a99-8540-e430ab995307.dmp).
  std::vector<std::string> minidump_file_path_components =
      minidump_file_path.GetComponents();
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
  ASSERT_EQ(61U, config_lines.size())
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

  EXPECT_EQ("BreakpadServerParameterPrefix_crashed_during_session_restore",
            config_lines[30]);
  EXPECT_EQ(base::NumberToString(strlen(kYesString)), config_lines[31]);
  EXPECT_EQ(kYesString, config_lines[32]);

  EXPECT_EQ("BreakpadServerParameterPrefix_osVersion", config_lines[33]);
  EXPECT_EQ(base::NumberToString(kOSVersion.length), config_lines[34]);
  EXPECT_EQ(base::SysNSStringToUTF8(kOSVersion), config_lines[35]);

  EXPECT_EQ("BreakpadServerParameterPrefix_osName", config_lines[36]);
  EXPECT_EQ("3", config_lines[37]);
  EXPECT_EQ("iOS", config_lines[38]);

  EXPECT_EQ("BreakpadServerParameterPrefix_platform", config_lines[39]);
  EXPECT_EQ(base::NumberToString(base::SysInfo::HardwareModelName().size()),
            config_lines[40]);
  EXPECT_EQ(base::SysInfo::HardwareModelName(), config_lines[41]);

  EXPECT_EQ("BreakpadServerParameterPrefix_breadcrumbs", config_lines[42]);
  EXPECT_EQ(
      base::NumberToString(strlen(kBreadcrumb1) + kBreadcrumb2.size() + 1),
      config_lines[43]);
  EXPECT_EQ(kBreadcrumb1, config_lines[44]);
  EXPECT_EQ(kBreadcrumb2, config_lines[45]);

  EXPECT_EQ("BreakpadServerParameterPrefix_signature", config_lines[46]);
  EXPECT_EQ(base::NumberToString(strlen(kLastEvent)), config_lines[47]);
  EXPECT_EQ(kLastEvent, config_lines[48]);

  EXPECT_EQ("BreakpadServerParameterPrefix_url", config_lines[49]);
  EXPECT_EQ(base::NumberToString(kURL.length), config_lines[50]);
  EXPECT_EQ(base::SysNSStringToUTF8(kURL), config_lines[51]);

  EXPECT_EQ("BreakpadProcessUpTime", config_lines[52]);
  EXPECT_EQ(base::NumberToString(base::NumberToString(kUptimeMs).size()),
            config_lines[53]);
  EXPECT_EQ(base::NumberToString(kUptimeMs), config_lines[54]);

  EXPECT_EQ("BreakpadServerParameterPrefix_memory_footprint", config_lines[55]);
  EXPECT_EQ(base::NumberToString(base::NumberToString(kMemoryFootprint).size()),
            config_lines[56]);
  EXPECT_EQ(base::NumberToString(kMemoryFootprint), config_lines[57]);

  EXPECT_EQ("BreakpadServerParameterPrefix_crashed_after_app_will_terminate",
            config_lines[58]);
  EXPECT_EQ(base::NumberToString(strlen(kYesString)), config_lines[59]);
  EXPECT_EQ(kYesString, config_lines[60]);

  // Read minidump file. It must be empty as there is no stack trace, but
  // Breakpad will not upload config without minidump file.
  base::File minidump_file(minidump_file_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(minidump_file.IsValid());
  EXPECT_EQ(0U, minidump_file.GetLength());
}

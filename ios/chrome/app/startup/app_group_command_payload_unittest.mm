// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/app_group_command_payload.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Histogram recording the delay of App Group commands.
const char kDelayHistogram[] = "Startup.ApplicationGroupCommandDelay";

// Lowest sample of the overflow bucket of the delay histogram.
const int kDelayHistogramOverflowSample = 100;

// Test fixture for `ConsumeAppGroupCommandPayload`.
class ConsumeAppGroupCommandPayloadTest : public PlatformTest {
 protected:
  ConsumeAppGroupCommandPayloadTest() { RemoveCommand(); }
  ~ConsumeAppGroupCommandPayloadTest() override { RemoveCommand(); }

  NSString* CommandPreference() {
    return base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);
  }

  // Writes an App Group command, issued at `command_time`, to the shared
  // defaults. Required fields set to nil are omitted.
  void WriteCommand(NSString* caller_app,
                    NSString* command,
                    NSDate* command_time) {
    NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
    dictionary[base::SysUTF8ToNSString(
        app_group::kChromeAppGroupCommandAppPreference)] = caller_app;
    dictionary[base::SysUTF8ToNSString(
        app_group::kChromeAppGroupCommandCommandPreference)] = command;
    dictionary[base::SysUTF8ToNSString(
        app_group::kChromeAppGroupCommandTimePreference)] = command_time;
    dictionary[base::SysUTF8ToNSString(
        app_group::kChromeAppGroupCommandTextPreference)] = kText;
    [app_group::GetGroupUserDefaults() setObject:dictionary
                                          forKey:CommandPreference()];
  }

  void WriteValidCommand() {
    WriteCommand(app_group::kOpenCommandSourceShareExtension, OpenURLCommand(),
                 [NSDate date]);
  }

  NSString* OpenURLCommand() {
    return base::SysUTF8ToNSString(app_group::kChromeAppGroupOpenURLCommand);
  }

  bool HasCommand() {
    return [app_group::GetGroupUserDefaults()
               objectForKey:CommandPreference()] != nil;
  }

  void RemoveCommand() {
    [app_group::GetGroupUserDefaults() removeObjectForKey:CommandPreference()];
  }

  NSString* const kText = @"https://www.example.com";
  base::HistogramTester histogram_tester_;
};

// Tests that an empty payload is returned when there is no command.
TEST_F(ConsumeAppGroupCommandPayloadTest, NoCommand) {
  AppGroupCommandPayload payload = ConsumeAppGroupCommandPayload();
  EXPECT_FALSE(payload.is_valid);
  EXPECT_FALSE(payload.command_time.has_value());
  EXPECT_FALSE(payload.command);
  histogram_tester_.ExpectTotalCount(kDelayHistogram, 0);
}

// Tests that a recent and complete command is valid, parsed and its delay
// recorded.
TEST_F(ConsumeAppGroupCommandPayloadTest, ValidCommand) {
  WriteValidCommand();

  AppGroupCommandPayload payload = ConsumeAppGroupCommandPayload();
  EXPECT_TRUE(payload.is_valid);
  EXPECT_TRUE(payload.command_time.has_value());
  histogram_tester_.ExpectUniqueSample(kDelayHistogram, 0, 1);
  EXPECT_NSEQ(OpenURLCommand(), payload.command);
  EXPECT_NSEQ(app_group::kOpenCommandSourceShareExtension, payload.caller_app);
  EXPECT_NSEQ(kText, payload.external_text);
}

// Tests that the command is removed from the shared defaults, so that it can
// only be consumed once.
TEST_F(ConsumeAppGroupCommandPayloadTest, CommandIsConsumed) {
  WriteValidCommand();

  EXPECT_TRUE(ConsumeAppGroupCommandPayload().is_valid);
  EXPECT_FALSE(HasCommand());
  EXPECT_FALSE(ConsumeAppGroupCommandPayload().is_valid);
}

// Tests that an expired command is invalid, but still reports its delay.
TEST_F(ConsumeAppGroupCommandPayloadTest, ExpiredCommand) {
  WriteCommand(
      app_group::kOpenCommandSourceShareExtension, OpenURLCommand(),
      [NSDate dateWithTimeIntervalSinceNow:-2 *
                                           app_group::kAppGroupCommandTimeout]);

  AppGroupCommandPayload payload = ConsumeAppGroupCommandPayload();
  EXPECT_FALSE(payload.is_valid);
  EXPECT_FALSE(HasCommand());
  histogram_tester_.ExpectTotalCount(kDelayHistogram, 1);
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(kDelayHistogram, 0));
}

// Tests that a command without a time is invalid and its delay recorded as
// expired.
TEST_F(ConsumeAppGroupCommandPayloadTest, MissingTime) {
  WriteCommand(app_group::kOpenCommandSourceShareExtension, OpenURLCommand(),
               nil);

  AppGroupCommandPayload payload = ConsumeAppGroupCommandPayload();
  EXPECT_FALSE(payload.is_valid);
  EXPECT_FALSE(payload.command_time.has_value());
  histogram_tester_.ExpectUniqueSample(kDelayHistogram,
                                       kDelayHistogramOverflowSample, 1);
}

// Tests that a command without a caller app is invalid and its delay not
// recorded.
TEST_F(ConsumeAppGroupCommandPayloadTest, MissingCallerApp) {
  WriteCommand(nil, OpenURLCommand(), [NSDate date]);

  EXPECT_FALSE(ConsumeAppGroupCommandPayload().is_valid);
  histogram_tester_.ExpectTotalCount(kDelayHistogram, 0);
}

// Tests that a command without a command name is invalid and its delay not
// recorded.
TEST_F(ConsumeAppGroupCommandPayloadTest, MissingCommandName) {
  WriteCommand(app_group::kOpenCommandSourceShareExtension, nil, [NSDate date]);

  EXPECT_FALSE(ConsumeAppGroupCommandPayload().is_valid);
  histogram_tester_.ExpectTotalCount(kDelayHistogram, 0);
}

}  // namespace

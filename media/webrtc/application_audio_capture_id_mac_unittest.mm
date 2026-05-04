// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/application_audio_capture_id_mac.h"

#import <AppKit/AppKit.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
NSRunningApplication* g_mock_app = nil;
NSArray<NSRunningApplication*>* g_mock_apps = nil;
}  // namespace

@interface FakeNSRunningApplication : NSObject
@property(nonatomic, copy) NSString* bundleIdentifier;
@property(nonatomic, assign) pid_t processIdentifier;
@end

@implementation FakeNSRunningApplication
@synthesize bundleIdentifier = _bundleIdentifier;
@synthesize processIdentifier = _processIdentifier;
@end

@interface MockNSRunningApplication : NSObject
+ (nullable NSRunningApplication*)runningApplicationWithProcessIdentifier:
    (pid_t)pid;
+ (NSArray<NSRunningApplication*>*)runningApplicationsWithBundleIdentifier:
    (NSString*)bundleIdentifier;
@end

@implementation MockNSRunningApplication
+ (nullable NSRunningApplication*)runningApplicationWithProcessIdentifier:
    (pid_t)pid {
  return g_mock_app;
}
+ (NSArray<NSRunningApplication*>*)runningApplicationsWithBundleIdentifier:
    (NSString*)bundleIdentifier {
  return g_mock_apps;
}
@end

namespace media {

TEST(ApplicationAudioCaptureIdMacTest,
     GetApplicationAudioCaptureIdForProcess_NotFound) {
  g_mock_app = nil;
  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  EXPECT_FALSE(identifier.has_value());
}

TEST(ApplicationAudioCaptureIdMacTest,
     GetApplicationAudioCaptureIdForProcess_NonChromium) {
  FakeNSRunningApplication* fake_app = [[FakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"com.example.app";
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  ASSERT_TRUE(identifier.has_value());
  EXPECT_EQ(identifier->bundle_id, "com.example.app");
  EXPECT_FALSE(identifier->pid.has_value());
}

struct BundleIdTestParams {
  NSString* input_bundle_id;
  std::string expected_truncated_id;
};

class ApplicationAudioCaptureIdMacBrowserTest
    : public testing::TestWithParam<BundleIdTestParams> {};

TEST_P(ApplicationAudioCaptureIdMacBrowserTest, TruncatesCorrectly) {
  const BundleIdTestParams& params = GetParam();

  FakeNSRunningApplication* fake_app = [[FakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = params.input_bundle_id;
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  ASSERT_TRUE(identifier.has_value());
  EXPECT_EQ(identifier->bundle_id, params.expected_truncated_id);
  ASSERT_TRUE(identifier->pid.has_value());
  EXPECT_EQ(identifier->pid.value(), 123);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ApplicationAudioCaptureIdMacBrowserTest,
    testing::Values(
        // Standard browsers
        BundleIdTestParams{@"com.google.Chrome", "com.google.Chrome"},
        BundleIdTestParams{@"org.chromium.Chromium", "org.chromium.Chromium"},
        BundleIdTestParams{@"com.microsoft.edgemac", "com.microsoft.edgemac"},
        BundleIdTestParams{@"com.operasoftware.Opera",
                           "com.operasoftware.Opera"},

        // Variants
        BundleIdTestParams{@"com.google.Chrome.beta", "com.google.Chrome"},
        BundleIdTestParams{@"com.google.Chrome.canary", "com.google.Chrome"},
        BundleIdTestParams{@"com.google.Chrome.dev", "com.google.Chrome"},
        BundleIdTestParams{@"com.microsoft.edgemac.beta",
                           "com.microsoft.edgemac"}));

struct PwaTestParams {
  NSString* pwa_bundle_id;
  NSString* browser_bundle_id;
  std::string expected_id;
};

class ApplicationAudioCaptureIdMacPwaTest
    : public testing::TestWithParam<PwaTestParams> {};

TEST_P(ApplicationAudioCaptureIdMacPwaTest, ResolvesToBrowser) {
  const PwaTestParams& params = GetParam();

  FakeNSRunningApplication* fake_app = [[FakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = params.pwa_bundle_id;
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  FakeNSRunningApplication* fake_browser =
      [[FakeNSRunningApplication alloc] init];
  fake_browser.bundleIdentifier = params.browser_bundle_id;
  fake_browser.processIdentifier = 456;
  g_mock_apps = @[ (NSRunningApplication*)fake_browser ];

  base::apple::ScopedObjCClassSwizzler swizzler_pid(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));
  base::apple::ScopedObjCClassSwizzler swizzler_bundle(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationsWithBundleIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  ASSERT_TRUE(identifier.has_value());
  EXPECT_EQ(identifier->bundle_id, params.expected_id);
  ASSERT_TRUE(identifier->pid.has_value());
  EXPECT_EQ(identifier->pid.value(), 456);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ApplicationAudioCaptureIdMacPwaTest,
    testing::Values(
        PwaTestParams{@"org.chromium.Chromium.app.a1b2c3",
                      @"org.chromium.Chromium", "org.chromium.Chromium"},
        PwaTestParams{@"com.google.Chrome.beta.app.d4e5f6",
                      @"com.google.Chrome.beta", "com.google.Chrome"},
        PwaTestParams{@"com.microsoft.edgemac.app.g7h8i9",
                      @"com.microsoft.edgemac", "com.microsoft.edgemac"}));

TEST(ApplicationAudioCaptureIdMacTest,
     GetApplicationAudioCaptureIdForProcess_PWA_BrowserNotFound) {
  FakeNSRunningApplication* fake_app = [[FakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"org.chromium.Chromium.app.a1b2c3";
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  g_mock_apps = @[];  // Empty list

  base::apple::ScopedObjCClassSwizzler swizzler_pid(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));
  base::apple::ScopedObjCClassSwizzler swizzler_bundle(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationsWithBundleIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  EXPECT_FALSE(identifier.has_value());
}

TEST(ApplicationAudioCaptureIdMacTest,
     GetApplicationAudioCaptureIdForProcess_PWA_MultipleBrowsers) {
  FakeNSRunningApplication* fake_app = [[FakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"org.chromium.Chromium.app.a1b2c3";
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  FakeNSRunningApplication* fake_browser =
      [[FakeNSRunningApplication alloc] init];
  fake_browser.bundleIdentifier = @"org.chromium.Chromium";
  fake_browser.processIdentifier = 456;
  g_mock_apps = @[
    (NSRunningApplication*)fake_browser, (NSRunningApplication*)fake_browser
  ];

  base::apple::ScopedObjCClassSwizzler swizzler_pid(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));
  base::apple::ScopedObjCClassSwizzler swizzler_bundle(
      [NSRunningApplication class], [MockNSRunningApplication class],
      @selector(runningApplicationsWithBundleIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  EXPECT_FALSE(identifier.has_value());
}

}  // namespace media

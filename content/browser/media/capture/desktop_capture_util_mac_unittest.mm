// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_util_mac.h"

#import <AppKit/AppKit.h>

#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_provider.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
NSRunningApplication* g_mock_app = nil;
NSArray<NSRunningApplication*>* g_mock_apps = nil;
}  // namespace

@interface DesktopCaptureUtilFakeNSRunningApplication : NSObject
@property(nonatomic, copy) NSString* bundleIdentifier;
@property(nonatomic, assign) pid_t processIdentifier;
@end

@implementation DesktopCaptureUtilFakeNSRunningApplication
@synthesize bundleIdentifier = _bundleIdentifier;
@synthesize processIdentifier = _processIdentifier;
@end

@interface DesktopCaptureUtilMockNSRunningApplication : NSObject
+ (nullable NSRunningApplication*)runningApplicationWithProcessIdentifier:
    (pid_t)pid;
+ (NSArray<NSRunningApplication*>*)runningApplicationsWithBundleIdentifier:
    (NSString*)bundleIdentifier;
@end

@implementation DesktopCaptureUtilMockNSRunningApplication
+ (nullable NSRunningApplication*)runningApplicationWithProcessIdentifier:
    (pid_t)pid {
  return g_mock_app;
}
+ (NSArray<NSRunningApplication*>*)runningApplicationsWithBundleIdentifier:
    (NSString*)bundleIdentifier {
  return g_mock_apps;
}
@end

namespace content {

namespace {

class FakeVideoCaptureProvider : public VideoCaptureProvider {
 public:
  FakeVideoCaptureProvider() = default;
  ~FakeVideoCaptureProvider() override = default;

  // VideoCaptureProvider implementation:
  void GetDeviceInfosAsync(GetDeviceInfosCallback result_callback) override {}
  std::unique_ptr<VideoCaptureDeviceLauncher> CreateDeviceLauncher() override {
    return nullptr;
  }
  void OpenNativeScreenCapturePicker(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(webrtc::DesktopCapturer::Source)> picker_callback,
      base::OnceCallback<void()> cancel_callback,
      base::OnceCallback<void()> error_callback,
      base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback)
      override {}
  void CloseNativeScreenCapturePicker(DesktopMediaID device_id) override {}
  void GetApplicationAudioCaptureId(
      DesktopMediaID::Id session_id,
      base::OnceCallback<void(
          const std::optional<desktop_capture::ApplicationAudioCaptureId>&)>
          callback) override {
    // Return a fake ID for testing.
    std::move(callback).Run(desktop_capture::ApplicationAudioCaptureId{
        .bundle_id = "com.example.FakeApp",
        .pid = 1234,
    });
  }
};

}  // namespace

class DesktopCaptureUtilTest : public testing::Test {
 public:
  DesktopCaptureUtilTest() {
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::TestAudioThread>());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    auto fake_provider = std::make_unique<FakeVideoCaptureProvider>();
    media_stream_manager_ = std::make_unique<MediaStreamManager>(
        audio_system_.get(), std::move(fake_provider));
  }

  ~DesktopCaptureUtilTest() override {
    if (audio_manager_) {
      audio_manager_->Shutdown();
    }
  }

 protected:
  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(DesktopCaptureUtilTest, GetApplicationAudioCaptureId_PlatformNative) {
  DesktopCaptureUtilFakeNSRunningApplication* fake_app =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"com.example.PlatformNativeApp";
  fake_app.processIdentifier = 5678;
  base::AutoReset<NSRunningApplication*> reset_mock_app(
      &g_mock_app, (NSRunningApplication*)fake_app);

  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));

  DesktopMediaID media_id(DesktopMediaID::TYPE_WINDOW, 1);
  media_id.id_type = DesktopMediaID::IdType::kPlatformNative;

  base::test::TestFuture<
      std::optional<desktop_capture::ApplicationAudioCaptureId>>
      future;
  GetApplicationAudioCaptureIdInternal(
      media_id,
      future.GetCallback<
          const std::optional<desktop_capture::ApplicationAudioCaptureId>&>());

  std::optional<desktop_capture::ApplicationAudioCaptureId> expected =
      desktop_capture::ApplicationAudioCaptureId{
          .bundle_id = "com.example.PlatformNativeApp",
          .pid = std::nullopt,
      };
  EXPECT_EQ(expected, future.Get());
}

TEST_F(DesktopCaptureUtilTest,
       GetApplicationAudioCaptureId_NativePickerSession) {
  DesktopMediaID media_id(DesktopMediaID::TYPE_WINDOW, 1);
  media_id.id_type = DesktopMediaID::IdType::kNativePickerSession;

  base::test::TestFuture<
      std::optional<desktop_capture::ApplicationAudioCaptureId>>
      future;
  GetApplicationAudioCaptureIdInternal(
      media_id,
      future.GetCallback<
          const std::optional<desktop_capture::ApplicationAudioCaptureId>&>());

  std::optional<desktop_capture::ApplicationAudioCaptureId> expected =
      desktop_capture::ApplicationAudioCaptureId{
          .bundle_id = "com.example.FakeApp",
          .pid = 1234,
      };
  EXPECT_EQ(expected, future.Get());
}

TEST_F(DesktopCaptureUtilTest,
       GetApplicationAudioCaptureIdForProcess_NotFound) {
  g_mock_app = nil;
  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  EXPECT_FALSE(identifier.has_value());
}

TEST_F(DesktopCaptureUtilTest,
       GetApplicationAudioCaptureIdForProcess_NonChromium) {
  DesktopCaptureUtilFakeNSRunningApplication* fake_app =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"com.example.app";
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
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

class DesktopCaptureUtilBrowserTest
    : public testing::TestWithParam<BundleIdTestParams> {};

TEST_P(DesktopCaptureUtilBrowserTest, TruncatesCorrectly) {
  const BundleIdTestParams& params = GetParam();

  DesktopCaptureUtilFakeNSRunningApplication* fake_app =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = params.input_bundle_id;
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  base::apple::ScopedObjCClassSwizzler swizzler(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  ASSERT_TRUE(identifier.has_value());
  EXPECT_EQ(identifier->bundle_id, params.expected_truncated_id);
  ASSERT_TRUE(identifier->pid.has_value());
  EXPECT_EQ(identifier->pid.value(), 123);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DesktopCaptureUtilBrowserTest,
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

class DesktopCaptureUtilPwaTest : public testing::TestWithParam<PwaTestParams> {
};

TEST_P(DesktopCaptureUtilPwaTest, ResolvesToBrowser) {
  const PwaTestParams& params = GetParam();

  DesktopCaptureUtilFakeNSRunningApplication* fake_app =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = params.pwa_bundle_id;
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  DesktopCaptureUtilFakeNSRunningApplication* fake_browser =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_browser.bundleIdentifier = params.browser_bundle_id;
  fake_browser.processIdentifier = 456;
  g_mock_apps = @[ (NSRunningApplication*)fake_browser ];

  base::apple::ScopedObjCClassSwizzler swizzler_pid(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));
  base::apple::ScopedObjCClassSwizzler swizzler_bundle(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationsWithBundleIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  ASSERT_TRUE(identifier.has_value());
  EXPECT_EQ(identifier->bundle_id, params.expected_id);
  ASSERT_TRUE(identifier->pid.has_value());
  EXPECT_EQ(identifier->pid.value(), 456);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DesktopCaptureUtilPwaTest,
    testing::Values(
        PwaTestParams{@"org.chromium.Chromium.app.a1b2c3",
                      @"org.chromium.Chromium", "org.chromium.Chromium"},
        PwaTestParams{@"com.google.Chrome.beta.app.d4e5f6",
                      @"com.google.Chrome.beta", "com.google.Chrome"},
        PwaTestParams{@"com.microsoft.edgemac.app.g7h8i9",
                      @"com.microsoft.edgemac", "com.microsoft.edgemac"}));

TEST_F(DesktopCaptureUtilTest,
       GetApplicationAudioCaptureIdForProcess_PWA_BrowserNotFound) {
  DesktopCaptureUtilFakeNSRunningApplication* fake_app =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"org.chromium.Chromium.app.a1b2c3";
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  g_mock_apps = @[];  // Empty list

  base::apple::ScopedObjCClassSwizzler swizzler_pid(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));
  base::apple::ScopedObjCClassSwizzler swizzler_bundle(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationsWithBundleIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  EXPECT_FALSE(identifier.has_value());
}

TEST_F(DesktopCaptureUtilTest,
       GetApplicationAudioCaptureIdForProcess_PWA_MultipleBrowsers) {
  DesktopCaptureUtilFakeNSRunningApplication* fake_app =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = @"org.chromium.Chromium.app.a1b2c3";
  fake_app.processIdentifier = 123;
  g_mock_app = (NSRunningApplication*)fake_app;

  DesktopCaptureUtilFakeNSRunningApplication* fake_browser =
      [[DesktopCaptureUtilFakeNSRunningApplication alloc] init];
  fake_browser.bundleIdentifier = @"org.chromium.Chromium";
  fake_browser.processIdentifier = 456;
  g_mock_apps = @[
    (NSRunningApplication*)fake_browser, (NSRunningApplication*)fake_browser
  ];

  base::apple::ScopedObjCClassSwizzler swizzler_pid(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationWithProcessIdentifier:));
  base::apple::ScopedObjCClassSwizzler swizzler_bundle(
      [NSRunningApplication class],
      [DesktopCaptureUtilMockNSRunningApplication class],
      @selector(runningApplicationsWithBundleIdentifier:));

  auto identifier = GetApplicationAudioCaptureIdForProcess(123);
  EXPECT_FALSE(identifier.has_value());
}

}  // namespace content

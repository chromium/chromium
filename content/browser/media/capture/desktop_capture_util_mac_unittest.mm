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
@end

@implementation DesktopCaptureUtilMockNSRunningApplication
+ (nullable NSRunningApplication*)runningApplicationWithProcessIdentifier:
    (pid_t)pid {
  return g_mock_app;
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
          const std::optional<media::ApplicationAudioCaptureId>&)> callback)
      override {
    // Return a fake ID for testing.
    std::move(callback).Run(media::ApplicationAudioCaptureId{
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

}  // namespace content

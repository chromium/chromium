// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"

#import <Cocoa/Cocoa.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "content/browser/media/capture/fake_screen_capture_picker_mac.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

class ScreenCaptureKitRealCaptureTest
    : public ContentBrowserTest,
      public testing::WithParamInterface<DesktopMediaID::Type> {
 public:
  ScreenCaptureKitRealCaptureTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            "use-gpu-in-tests")) {
      GTEST_SKIP() << "Skipping test on non-GPU bots";
    }

    if (@available(macOS 14.4, *)) {
      // Create a window to ensure we have something to capture.
      // Use a distinct title so we pick this window rather than the default
      // Content Shell window.
      browser_window_title_ = "ScreenCaptureKitRealCaptureTest_Window";

      window_ = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 400, 400)
                                            styleMask:NSWindowStyleMaskTitled |
                                                      NSWindowStyleMaskResizable
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
      [window_ setReleasedWhenClosed:NO];
      [window_ makeKeyAndOrderFront:nil];
      [window_ orderFrontRegardless];
      [window_ setTitle:base::SysUTF8ToNSString(browser_window_title_)];
      [window_ center];
      [window_ setBackgroundColor:[NSColor blueColor]];
    } else {
      GTEST_SKIP() << "Skipping tests on macOS < 14.4";
    }
  }

  void TearDownOnMainThread() override {
    if (device_ && mock_client_ptr_) {
      base::RunLoop stop_loop;
      stop_run_loop_ = &stop_loop;
      device_->StopAndDeAllocate();
      stop_loop.Run();
      stop_run_loop_ = nullptr;
      mock_client_ptr_ = nullptr;
    }
    if (window_) {
      [window_ close];
      window_ = nil;
    }
    device_.reset();
    ContentBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<media::MockVideoCaptureDeviceClient> CreateClient() {
    return std::make_unique<media::MockVideoCaptureDeviceClient>();
  }

  void RunPickerAndCreateDevice() {
    ASSERT_FALSE(browser_window_title_.empty());
    std::unique_ptr<NativeScreenCapturePicker> picker =
        CreateFakeScreenCapturePickerMac(browser_window_title_,
                                         base::BindLambdaForTesting([this]() {
                                           if (stop_run_loop_) {
                                             stop_run_loop_->Quit();
                                           }
                                         }));
    ASSERT_TRUE(picker);

    base::RunLoop run_loop;
    DesktopMediaID::Id captured_source_id = DesktopMediaID::kNullId;
    // We strictly use the picker to get the source ID.
    // The picker itself holds the content filter internally.
    picker->Open(
        GetParam(), base::DoNothing(),
        base::BindLambdaForTesting([&](webrtc::DesktopCapturer::Source source) {
          captured_source_id = source.id;
          run_loop.Quit();
        }),
        base::BindLambdaForTesting([&]() {
          run_loop.Quit();
          FAIL() << "Picker cancelled";
        }),
        base::BindLambdaForTesting([&]() {
          run_loop.Quit();
          FAIL() << "Picker error";
        }),
        base::DoNothing());
    run_loop.Run();

    ASSERT_NE(captured_source_id, DesktopMediaID::kNullId);

    DesktopMediaID source(GetParam(), captured_source_id);
    device_ = picker->CreateDevice(source);
    ASSERT_TRUE(device_);
  }

 protected:
  std::unique_ptr<media::VideoCaptureDevice> device_;
  raw_ptr<media::MockVideoCaptureDeviceClient> mock_client_ptr_ = nullptr;
  std::string browser_window_title_;
  NSWindow* __strong window_;
  raw_ptr<base::RunLoop> stop_run_loop_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(ScreenCaptureKitRealCaptureTest, CreateCapturer) {
  ASSERT_NO_FATAL_FAILURE(RunPickerAndCreateDevice());
}

IN_PROC_BROWSER_TEST_P(ScreenCaptureKitRealCaptureTest, CaptureFrame) {
  ASSERT_NO_FATAL_FAILURE(RunPickerAndCreateDevice());
  auto mock_client = CreateClient();
  mock_client_ptr_ = mock_client.get();

  base::RunLoop wait_for_frame_loop;
  int events_needed = 2;
  auto check_done = base::BindLambdaForTesting([&]() {
    events_needed--;
    if (events_needed == 0) {
      wait_for_frame_loop.Quit();
    }
  });

  // Setup Expectations.
  ON_CALL(*mock_client_ptr_, OnError(_, _, _))
      .WillByDefault([&](media::VideoCaptureError error,
                         const base::Location& from_here,
                         const std::string& reason) {
        ADD_FAILURE() << "OnError called: " << static_cast<int>(error) << " - "
                      << reason;
        wait_for_frame_loop.Quit();
      });

  EXPECT_CALL(*mock_client_ptr_, OnStarted())
      .WillOnce(base::test::RunClosure(check_done));

  // We expect at least one frame with a buffer.
  EXPECT_CALL(*mock_client_ptr_,
              OnIncomingCapturedExternalBuffer(_, _, _, _, _, _, _))
      .WillOnce(base::test::RunClosure(check_done))
      .WillRepeatedly(testing::Return());

  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      gfx::Size(640, 480), 30.0f, media::PIXEL_FORMAT_NV12);

  // Start Capture and wait for frame.
  device_->AllocateAndStart(params, std::move(mock_client));
  wait_for_frame_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ScreenCaptureKitRealCaptureTest,
    testing::Values(DesktopMediaID::Type::TYPE_SCREEN,
                    DesktopMediaID::Type::TYPE_WINDOW),
    [](const testing::TestParamInfo<ScreenCaptureKitRealCaptureTest::ParamType>&
           info) {
      switch (info.param) {
        case DesktopMediaID::Type::TYPE_SCREEN:
          return "Screen";
        case DesktopMediaID::Type::TYPE_WINDOW:
          return "Window";
        default:
          return "Unknown";
      }
    });

}  // namespace content

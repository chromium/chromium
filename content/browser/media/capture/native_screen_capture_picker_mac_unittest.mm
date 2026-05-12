// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <map>
#include <optional>
#include <string>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/test/browser_task_environment.h"
#include "media/capture/video/mac/test/screen_capture_kit_test_helper.h"
#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Source = webrtc::DesktopCapturer::Source;

// --- Mocking helpers for GetApplicationAudioCaptureIdForNativeWindowId
// --------------------

static std::map<content::DesktopMediaID::Id, std::string>& GetBundleMap() {
  static base::NoDestructor<std::map<content::DesktopMediaID::Id, std::string>>
      bundle_map;
  return *bundle_map;
}

static pid_t GetWindowOwnerPidFake(content::DesktopMediaID::Id window_id) {
  // Use window_id as PID for simplicity.
  return static_cast<pid_t>(window_id);
}

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
@end

@implementation MockNSRunningApplication
+ (nullable NSRunningApplication*)runningApplicationWithProcessIdentifier:
    (pid_t)pid {
  auto it = GetBundleMap().find(static_cast<content::DesktopMediaID::Id>(pid));
  if (it == GetBundleMap().end()) {
    return nil;
  }
  FakeNSRunningApplication* fake_app = [[FakeNSRunningApplication alloc] init];
  fake_app.bundleIdentifier = base::SysUTF8ToNSString(it->second);
  fake_app.processIdentifier = pid;
  return (NSRunningApplication*)fake_app;
}
@end

@class FakeSCContentSharingPicker;
// Globals used for swizzling.
API_AVAILABLE(macos(14.0))
static FakeSCContentSharingPicker* g_fake_picker = nil;

// --- FakeSCContentSharingPicker ----------------------------------------------

API_AVAILABLE(macos(14.0))
@interface FakeSCContentSharingPicker : NSObject
@property(assign, readwrite) BOOL active;
@property(copy, readwrite)
    SCContentSharingPickerConfiguration* defaultConfiguration;
@property(copy, readwrite) NSNumber* maximumStreamCount;
@property(weak, readonly) id<SCContentSharingPickerObserver> observer;
@property(assign, readonly) SCShareableContentStyle lastPresentedContentStyle;
- (void)addObserver:(id<SCContentSharingPickerObserver>)observer;
- (void)presentPickerUsingContentStyle:(SCShareableContentStyle)contentStyle;
@end

@implementation FakeSCContentSharingPicker
@synthesize active = _active;
@synthesize defaultConfiguration = _defaultConfiguration;
@synthesize maximumStreamCount = _maximumStreamCount;
@synthesize observer = _observer;
@synthesize lastPresentedContentStyle = _lastPresentedContentStyle;

- (instancetype)init {
  if (self = [super init]) {
    _defaultConfiguration = [[SCContentSharingPickerConfiguration alloc] init];
  }
  return self;
}

- (void)addObserver:(id<SCContentSharingPickerObserver>)observer {
  _observer = observer;
}

- (void)presentPickerUsingContentStyle:(SCShareableContentStyle)contentStyle {
  _lastPresentedContentStyle = contentStyle;
}
@end

// --- Swizzling Categories ----------------------------------------------------

API_AVAILABLE(macos(14.0))
@interface SCContentSharingPicker (NativeScreenCapturePickerMacTest)
+ (SCContentSharingPicker*)fakeSharedPicker;
@end

@implementation SCContentSharingPicker (NativeScreenCapturePickerMacTest)
+ (SCContentSharingPicker*)fakeSharedPicker {
  return (SCContentSharingPicker*)g_fake_picker;
}
@end

@interface FakeSCContentFilter : NSObject
@property(strong) NSArray* includedWindows;
@end

@implementation FakeSCContentFilter
@synthesize includedWindows = _includedWindows;
@end

namespace content {

class NativeScreenCapturePickerMacTest : public testing::Test {
 public:
  NativeScreenCapturePickerMacTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    GetBundleMap().clear();
    if (@available(macOS 14.0, *)) {
      g_fake_picker = [[FakeSCContentSharingPicker alloc] init];
      picker_swizzler_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
          [SCContentSharingPicker class], @selector(sharedPicker),
          @selector(fakeSharedPicker));
      ns_running_app_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [NSRunningApplication class], [MockNSRunningApplication class],
              @selector(runningApplicationWithProcessIdentifier:));
      NativeScreenCapturePickerMac::SetGetWindowOwnerPidForTesting(
          base::BindRepeating(&GetWindowOwnerPidFake));
      picker_ = CreateNativeScreenCapturePickerMac();
    } else {
      GTEST_SKIP() << "Skipping tests on macOS < 14.0";
    }
  }

  void TearDown() override {
    picker_.reset();
    picker_swizzler_.reset();
    ns_running_app_swizzler_.reset();
    if (@available(macOS 14.0, *)) {
      g_fake_picker = nil;
      NativeScreenCapturePickerMac::SetGetWindowOwnerPidForTesting(
          base::NullCallback());
    }
  }

  // Helper to create a fake window.
  FakeSCWindow* CreateFakeWindow(CGWindowID window_id) {
    return [[FakeSCWindow alloc] initWithID:window_id
                                      title:@""
                          owningApplication:nil
                                windowLayer:0
                                      frame:CGRectZero
                                   onScreen:YES];
  }

  // Triggers a picker observer update. If |window_id| is provided, the filter
  // will include that specific window (macOS 15.2+).
  void TriggerUpdate(std::optional<CGWindowID> window_id = std::nullopt) {
    if (@available(macOS 14.0, *)) {
      FakeSCContentFilter* filter = [[FakeSCContentFilter alloc] init];
      if (window_id.has_value()) {
        if (@available(macOS 15.2, *)) {
          filter.includedWindows = @[ CreateFakeWindow(*window_id) ];
        }
      }
      [g_fake_picker.observer
          contentSharingPicker:(SCContentSharingPicker*)g_fake_picker
           didUpdateWithFilter:(SCContentFilter*)filter
                     forStream:nil];
    }
  }

  // Opens the picker and triggers an update for the specified source type.
  DesktopMediaID::Id OpenPickerAndSelect(
      DesktopMediaID::Type type,
      std::optional<CGWindowID> window_id = std::nullopt,
      base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback =
          base::DoNothing()) {
    DesktopMediaID::Id id = 0;
    if (@available(macOS 14.0, *)) {
      base::RunLoop run_loop;
      picker_->Open(type, base::DoNothing(),
                    base::BindLambdaForTesting([&](Source source) {
                      id = source.id;
                      run_loop.Quit();
                    }),
                    base::DoNothing(), base::DoNothing(),
                    std::move(stop_audio_callback));
      TriggerUpdate(window_id);
      run_loop.Run();
    }
    return id;
  }

  // Calls GetApplicationAudioCaptureId and verifies the result.
  void VerifyApplicationAudioCaptureId(
      DesktopMediaID::Id session_id,
      const std::optional<std::string>& expected_bundle_id) {
    base::test::TestFuture<
        const std::optional<desktop_capture::ApplicationAudioCaptureId>&>
        future;
    picker_->GetApplicationAudioCaptureId(session_id, future.GetCallback());
    const auto& capture_id = future.Get();
    if (expected_bundle_id.has_value()) {
      ASSERT_TRUE(capture_id.has_value());
      EXPECT_EQ(capture_id->bundle_id, *expected_bundle_id);
    } else {
      EXPECT_FALSE(capture_id.has_value());
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NativeScreenCapturePicker> picker_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> picker_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      ns_running_app_swizzler_;
};

TEST_F(NativeScreenCapturePickerMacTest, OpenCallsSystemPickerForScreen) {
  if (@available(macOS 14.0, *)) {
    picker_->Open(
        DesktopMediaID::TYPE_SCREEN,
        base::BindOnce([](DesktopMediaID::Id id) { EXPECT_EQ(id, 1); }),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing());

    EXPECT_TRUE(g_fake_picker.active);
    EXPECT_EQ(g_fake_picker.lastPresentedContentStyle,
              SCShareableContentStyleDisplay);
    EXPECT_EQ([g_fake_picker.maximumStreamCount intValue], 50);
    EXPECT_EQ(g_fake_picker.defaultConfiguration.allowedPickerModes,
              SCContentSharingPickerModeSingleDisplay);
  }
}

TEST_F(NativeScreenCapturePickerMacTest, OpenCallsSystemPickerForWindow) {
  if (@available(macOS 14.0, *)) {
    picker_->Open(
        DesktopMediaID::TYPE_WINDOW,
        base::BindOnce([](DesktopMediaID::Id id) { EXPECT_EQ(id, 1); }),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing());

    EXPECT_TRUE(g_fake_picker.active);
    EXPECT_EQ(g_fake_picker.lastPresentedContentStyle,
              SCShareableContentStyleWindow);
    EXPECT_EQ([g_fake_picker.maximumStreamCount intValue], 50);
    EXPECT_EQ(g_fake_picker.defaultConfiguration.allowedPickerModes,
              SCContentSharingPickerModeSingleWindow);
  }
}

TEST_F(NativeScreenCapturePickerMacTest, ObserverUpdateCallback) {
  if (@available(macOS 14.0, *)) {
    DesktopMediaID::Id captured_id =
        OpenPickerAndSelect(DesktopMediaID::TYPE_SCREEN);
    EXPECT_EQ(captured_id, 1);
  }
}

TEST_F(NativeScreenCapturePickerMacTest, CleanupLogic) {
  if (@available(macOS 14.0, *)) {
    DesktopMediaID::Id id = OpenPickerAndSelect(DesktopMediaID::TYPE_SCREEN);

    // Use CreateDevice to ensure the picker stays active.
    DesktopMediaID device_id(DesktopMediaID::TYPE_SCREEN, id);
    auto device = picker_->CreateDevice(device_id);
    EXPECT_TRUE(g_fake_picker.active);

    picker_->Close(device_id);
    EXPECT_FALSE(g_fake_picker.active);

    // Fast forward so CleanupContentFilter has a chance to run.
    task_environment_.FastForwardBy(base::Seconds(60));
  }
}

TEST_F(NativeScreenCapturePickerMacTest, DeactivateOnlyWhenLastDeviceClosed) {
  if (@available(macOS 14.0, *)) {
    DesktopMediaID::Id id1 = OpenPickerAndSelect(DesktopMediaID::TYPE_SCREEN);
    DesktopMediaID device_id1(DesktopMediaID::TYPE_SCREEN, id1);
    auto device1 = picker_->CreateDevice(device_id1);

    DesktopMediaID::Id id2 = OpenPickerAndSelect(DesktopMediaID::TYPE_SCREEN);
    DesktopMediaID device_id2(DesktopMediaID::TYPE_SCREEN, id2);
    auto device2 = picker_->CreateDevice(device_id2);

    EXPECT_TRUE(g_fake_picker.active);

    picker_->Close(device_id1);
    EXPECT_TRUE(g_fake_picker.active);

    picker_->Close(device_id2);
    EXPECT_FALSE(g_fake_picker.active);
  }
}

TEST_F(NativeScreenCapturePickerMacTest, MultipleOpenCalls) {
  if (@available(macOS 14.0, *)) {
    picker_->Open(
        DesktopMediaID::TYPE_SCREEN,
        base::BindOnce([](DesktopMediaID::Id id) { EXPECT_EQ(id, 1); }),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing());

    picker_->Open(
        DesktopMediaID::TYPE_WINDOW,
        base::BindOnce([](DesktopMediaID::Id id) { EXPECT_EQ(id, 2); }),
        base::DoNothing(), base::DoNothing(), base::DoNothing(),
        base::DoNothing());
  }
}

TEST_F(NativeScreenCapturePickerMacTest,
       DetectsPrimaryApplicationOnFirstUpdate) {
  if (@available(macOS 15.2, *)) {
    const CGWindowID kWindowId = 101;
    const std::string kExpectedBundleId = "com.example.app";
    GetBundleMap()[kWindowId] = kExpectedBundleId;

    OpenPickerAndSelect(DesktopMediaID::TYPE_WINDOW, kWindowId);
    ASSERT_NO_FATAL_FAILURE(
        VerifyApplicationAudioCaptureId(1, kExpectedBundleId));
  }
}

TEST_F(NativeScreenCapturePickerMacTest,
       TriggersStopAudioWhenPrimaryAppIsRemoved) {
  if (@available(macOS 15.2, *)) {
    base::RunLoop stop_audio_run_loop;
    const CGWindowID kWindowA = 101;
    const CGWindowID kWindowB = 102;
    GetBundleMap()[kWindowA] = "com.example.AppA";
    GetBundleMap()[kWindowB] = "com.example.AppB";

    OpenPickerAndSelect(DesktopMediaID::TYPE_WINDOW, kWindowA,
                        base::BindLambdaForTesting([&](DesktopMediaID::Id id) {
                          EXPECT_EQ(id, 1);
                          stop_audio_run_loop.Quit();
                        }));
    TriggerUpdate(kWindowB);

    stop_audio_run_loop.Run();
    EXPECT_TRUE(true);
  }
}

TEST_F(NativeScreenCapturePickerMacTest,
       MaintainsAudioIfPrimaryAppStillPresent) {
  if (@available(macOS 15.2, *)) {
    bool stop_audio_called = false;
    const CGWindowID kWindowA = 101;
    const CGWindowID kWindowB = 102;
    GetBundleMap()[kWindowA] = "com.example.AppA";
    GetBundleMap()[kWindowB] = "com.example.AppB";

    OpenPickerAndSelect(DesktopMediaID::TYPE_WINDOW, kWindowA,
                        base::BindLambdaForTesting([&](DesktopMediaID::Id id) {
                          stop_audio_called = true;
                        }));

    // Update with both windows (A and B).
    FakeSCContentFilter* filter = [[FakeSCContentFilter alloc] init];
    filter.includedWindows =
        @[ CreateFakeWindow(kWindowA), CreateFakeWindow(kWindowB) ];
    [g_fake_picker.observer
        contentSharingPicker:(SCContentSharingPicker*)g_fake_picker
         didUpdateWithFilter:(SCContentFilter*)filter
                   forStream:nil];

    base::RunLoop fence_run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, fence_run_loop.QuitClosure());
    fence_run_loop.Run();

    EXPECT_FALSE(stop_audio_called);
  }
}

TEST_F(NativeScreenCapturePickerMacTest, SequentialOpenCalls) {
  if (@available(macOS 15.2, *)) {
    const CGWindowID kWindow1 = 101;
    const std::string kBundle1 = "com.example.app1";
    GetBundleMap()[kWindow1] = kBundle1;

    OpenPickerAndSelect(DesktopMediaID::TYPE_WINDOW, kWindow1);
    ASSERT_NO_FATAL_FAILURE(VerifyApplicationAudioCaptureId(1, kBundle1));

    const CGWindowID kWindow2 = 102;
    const std::string kBundle2 = "com.example.app2";
    GetBundleMap()[kWindow2] = kBundle2;

    OpenPickerAndSelect(DesktopMediaID::TYPE_WINDOW, kWindow2);
    ASSERT_NO_FATAL_FAILURE(VerifyApplicationAudioCaptureId(1, kBundle1));
    ASSERT_NO_FATAL_FAILURE(VerifyApplicationAudioCaptureId(2, kBundle2));
  }
}

}  // namespace content

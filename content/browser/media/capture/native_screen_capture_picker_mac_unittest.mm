// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/browser/media/capture/native_screen_capture_picker.h"
#include "content/public/test/browser_task_environment.h"
#include "media/capture/video/mac/test/screen_capture_kit_test_helper.h"
#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using Source = webrtc::DesktopCapturer::Source;

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

namespace content {

class NativeScreenCapturePickerMacTest : public testing::Test {
 public:
  NativeScreenCapturePickerMacTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    if (@available(macOS 14.0, *)) {
      g_fake_picker = [[FakeSCContentSharingPicker alloc] init];
      picker_swizzler_ = std::make_unique<base::apple::ScopedObjCClassSwizzler>(
          [SCContentSharingPicker class], @selector(sharedPicker),
          @selector(fakeSharedPicker));
      picker_ = CreateNativeScreenCapturePickerMac();
    } else {
      GTEST_SKIP() << "Skipping tests on macOS < 14.0";
    }
  }

  void TearDown() override {
    picker_.reset();
    picker_swizzler_.reset();
    if (@available(macOS 14.0, *)) {
      g_fake_picker = nil;
    }
  }

  DesktopMediaID::Id OpenPickerAndTriggerUpdate(
      DesktopMediaID::Type type = DesktopMediaID::TYPE_SCREEN) {
    DesktopMediaID::Id id = 0;
    if (@available(macOS 14.0, *)) {
      base::RunLoop run_loop;
      picker_->Open(type, base::DoNothing(),
                    base::BindLambdaForTesting([&](Source source) {
                      id = source.id;
                      run_loop.Quit();
                    }),
                    base::DoNothing(), base::DoNothing());

      SCContentFilter* filter = (SCContentFilter*)[[NSObject alloc] init];
      [g_fake_picker.observer
          contentSharingPicker:(SCContentSharingPicker*)g_fake_picker
           didUpdateWithFilter:filter
                     forStream:nil];
      run_loop.Run();
    }
    return id;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<NativeScreenCapturePicker> picker_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler> picker_swizzler_;
};

TEST_F(NativeScreenCapturePickerMacTest, OpenCallsSystemPickerForScreen) {
  if (@available(macOS 14.0, *)) {
    picker_->Open(
        DesktopMediaID::TYPE_SCREEN,
        base::BindOnce([](DesktopMediaID::Id id) { EXPECT_EQ(id, 1); }),
        base::DoNothing(), base::DoNothing(), base::DoNothing());

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
        base::DoNothing(), base::DoNothing(), base::DoNothing());

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
    DesktopMediaID::Id captured_id = OpenPickerAndTriggerUpdate();
    EXPECT_EQ(captured_id, 1);
  }
}

TEST_F(NativeScreenCapturePickerMacTest, CleanupLogic) {
  if (@available(macOS 14.0, *)) {
    DesktopMediaID::Id id = OpenPickerAndTriggerUpdate();

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
    DesktopMediaID::Id id1 = OpenPickerAndTriggerUpdate();
    DesktopMediaID device_id1(DesktopMediaID::TYPE_SCREEN, id1);
    auto device1 = picker_->CreateDevice(device_id1);

    DesktopMediaID::Id id2 = OpenPickerAndTriggerUpdate();
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
        base::DoNothing(), base::DoNothing(), base::DoNothing());

    picker_->Open(
        DesktopMediaID::TYPE_WINDOW,
        base::BindOnce([](DesktopMediaID::Id id) { EXPECT_EQ(id, 2); }),
        base::DoNothing(), base::DoNothing(), base::DoNothing());
  }
}

}  // namespace content

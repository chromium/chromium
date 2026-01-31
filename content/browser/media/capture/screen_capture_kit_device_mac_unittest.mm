// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <objc/runtime.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "media/capture/video/mac/test/screen_capture_kit_test_helper.h"
#include "media/capture/video/mock_video_capture_device_client.h"
#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

// Globals used for swizzling.
// `g_simulated_windows` and `g_simulated_displays` are inputs to the swizzled
// API. `g_captured_stream` is an output captured from the swizzled API.
// WARNING: This pattern is NOT thread-safe for parallel test execution within
// the same process. Ensure tests run sequentially.
static NSArray* g_simulated_windows = nil;
static NSArray* g_simulated_displays = nil;
static id g_captured_stream = nil;

// --- Swizzling Categories ----------------------------------------------------

API_AVAILABLE(macos(12.3))
@interface SCShareableContent (ScreenCaptureKitDeviceMacTest)
+ (void)fakeGetShareableContentWithCompletionHandler:
    (void (^)(SCShareableContent*, NSError*))completionHandler;
@end

@implementation SCShareableContent (ScreenCaptureKitDeviceMacTest)
+ (void)fakeGetShareableContentWithCompletionHandler:
    (void (^)(SCShareableContent*, NSError*))completionHandler {
  NSArray* windows = g_simulated_windows ? g_simulated_windows : @[];
  NSArray* displays = g_simulated_displays ? g_simulated_displays : @[];

  id content = [[FakeSCShareableContent alloc]
      initWithWindows:(NSArray<FakeSCWindow*>*)windows
             displays:(NSArray<FakeSCDisplay*>*)displays];
  completionHandler(content, nil);
}
@end

API_AVAILABLE(macos(12.3))
@interface SCContentFilter (ScreenCaptureKitDeviceMacTest)
- (instancetype)initFakeWithDisplay:(id)display
                   excludingWindows:(NSArray*)windows;
- (instancetype)initFakeWithDesktopIndependentWindow:(id)window;
@end

@implementation SCContentFilter (ScreenCaptureKitDeviceMacTest)
- (instancetype)initFakeWithDisplay:(id)display
                   excludingWindows:(NSArray*)windows {
  return [super init];
}
- (instancetype)initFakeWithDesktopIndependentWindow:(id)window {
  return [super init];
}
@end

API_AVAILABLE(macos(12.3))
@interface FakeSCStream : NSObject
@property(class, copy) void (^onCreate)(FakeSCStream*);
@property(copy) void (^onStart)(void (^)(NSError*));
@property(copy) void (^onStop)(void (^)(NSError*));
@property(strong) id<SCStreamOutput> output;
- (instancetype)initWithFilter:(SCContentFilter*)filter
                 configuration:(SCStreamConfiguration*)config
                      delegate:(id<SCStreamDelegate>)delegate;
@end

@implementation FakeSCStream
@synthesize onStart = _onStart;
@synthesize onStop = _onStop;
@synthesize output = _output;

API_AVAILABLE(macos(12.3))
static void (^g_onCreate)(FakeSCStream*) = nil;

+ (void (^)(FakeSCStream*))onCreate {
  return g_onCreate;
}

+ (void)setOnCreate:(void (^)(FakeSCStream*))onCreate {
  g_onCreate = onCreate;
}

- (instancetype)initWithFilter:(SCContentFilter*)filter
                 configuration:(SCStreamConfiguration*)config
                      delegate:(id<SCStreamDelegate>)delegate {
  if (self = [super init]) {
    if (g_onCreate) {
      g_onCreate(self);
    }
  }
  return self;
}

- (BOOL)addStreamOutput:(id<SCStreamOutput>)output
                   type:(SCStreamOutputType)type
     sampleHandlerQueue:(dispatch_queue_t)sampleHandlerQueue
                  error:(NSError**)error {
  self.output = output;
  return YES;
}

- (BOOL)removeStreamOutput:(id<SCStreamOutput>)output
                      type:(SCStreamOutputType)type
                     error:(NSError**)error {
  self.output = nil;
  return YES;
}

- (void)startCaptureWithCompletionHandler:
    (void (^)(NSError* error))completionHandler {
  if (self.onStart) {
    self.onStart(completionHandler);
  } else {
    completionHandler(nil);
  }
}

- (void)stopCaptureWithCompletionHandler:
    (void (^)(NSError* error))completionHandler {
  if (self.onStop) {
    self.onStop(completionHandler);
  } else {
    completionHandler(nil);
  }
}

- (void)updateConfiguration:(SCStreamConfiguration*)streamConfiguration
          completionHandler:(void (^)(NSError* error))completionHandler {
  completionHandler(nil);
}

- (void)updateContentFilter:(SCContentFilter*)contentFilter
          completionHandler:(void (^)(NSError* error))completionHandler {
  completionHandler(nil);
}

@end

API_AVAILABLE(macos(12.3))
@interface SCStream (ScreenCaptureKitDeviceMacTest)
- (instancetype)initFakeWithFilter:(SCContentFilter*)filter
                     configuration:(SCStreamConfiguration*)config
                          delegate:(id<SCStreamDelegate>)delegate;
@end

@implementation SCStream (ScreenCaptureKitDeviceMacTest)
- (instancetype)initFakeWithFilter:(SCContentFilter*)filter
                     configuration:(SCStreamConfiguration*)config
                          delegate:(id<SCStreamDelegate>)delegate {
  // We return a new FakeSCStream instead of self.
  return (SCStream*)[[FakeSCStream alloc] initWithFilter:filter
                                           configuration:config
                                                delegate:delegate];
}
@end

namespace content {

class ScreenCaptureKitDeviceMacTest : public testing::Test {
 public:
  ScreenCaptureKitDeviceMacTest() = default;

  void SetUp() override {
    if (@available(macOS 13.2, *)) {
      shareable_content_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCShareableContent class],
              @selector(getShareableContentWithCompletionHandler:),
              @selector(fakeGetShareableContentWithCompletionHandler:));

      content_filter_window_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCContentFilter class],
              @selector(initWithDesktopIndependentWindow:),
              @selector(initFakeWithDesktopIndependentWindow:));

      content_filter_display_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCContentFilter class],
              @selector(initWithDisplay:excludingWindows:),
              @selector(initFakeWithDisplay:excludingWindows:));

      sc_stream_init_swizzler_ =
          std::make_unique<base::apple::ScopedObjCClassSwizzler>(
              [SCStream class],
              @selector(initWithFilter:configuration:delegate:),
              @selector(initFakeWithFilter:configuration:delegate:));
    } else {
      GTEST_SKIP() << "Skipping tests on macOS < 13.2";
    }
  }

  void TearDown() override {
    device_ = nullptr;
    shareable_content_swizzler_.reset();
    content_filter_window_swizzler_.reset();
    content_filter_display_swizzler_.reset();
    sc_stream_init_swizzler_.reset();
    g_simulated_windows = nil;
    g_simulated_displays = nil;
    g_captured_stream = nil;

    if (@available(macOS 12.3, *)) {
      [FakeSCStream setOnCreate:nil];
    }
  }

  void CreateDevice(const DesktopMediaID& source) {
    if (@available(macOS 13.2, *)) {
      device_ = CreateScreenCaptureKitDeviceMac(source, nil, base::DoNothing(),
                                                nullptr);
    }
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<media::VideoCaptureDevice> device_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      shareable_content_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      content_filter_window_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      content_filter_display_swizzler_;
  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
      sc_stream_init_swizzler_;

  void SetupContent(NSArray* windows, NSArray* displays = nil) {
    g_simulated_windows = windows;
    g_simulated_displays = displays;
  }

  void InitDevice(DesktopMediaID source) { CreateDevice(source); }

  std::unique_ptr<media::MockVideoCaptureDeviceClient> CreateClient() {
    return std::make_unique<media::MockVideoCaptureDeviceClient>();
  }

  void StartDevice(
      std::unique_ptr<media::MockVideoCaptureDeviceClient> mock_client)
      API_AVAILABLE(macos(12.3)) {
    [FakeSCStream setOnCreate:^(FakeSCStream* stream) {
      g_captured_stream = stream;
    }];

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(100, 100), 30.0f, media::PIXEL_FORMAT_NV12);

    base::RunLoop run_loop;
    EXPECT_CALL(*mock_client, OnStarted())
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

    device_->AllocateAndStart(params, std::move(mock_client));
    run_loop.Run();

    ASSERT_TRUE(g_captured_stream);
    ASSERT_TRUE(((FakeSCStream*)g_captured_stream).output);
  }

  void SimulateFrame() API_AVAILABLE(macos(12.3)) {
    ASSERT_TRUE(g_captured_stream);
    // Send a frame.
    base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
    // Create dummy sample buffer.
    {
      base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
      // IOSurface-backed.
      NSDictionary* options = @{(id)kCVPixelBufferIOSurfacePropertiesKey : @{}};
      ASSERT_EQ(
          CVPixelBufferCreate(kCFAllocatorDefault, 100, 100,
                              kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
                              (__bridge CFDictionaryRef)options,
                              pixel_buffer.InitializeInto()),
          kCVReturnSuccess);

      base::apple::ScopedCFTypeRef<CMVideoFormatDescriptionRef> format_desc;
      ASSERT_EQ(CMVideoFormatDescriptionCreateForImageBuffer(
                    kCFAllocatorDefault, pixel_buffer.get(),
                    format_desc.InitializeInto()),
                noErr);

      CMSampleTimingInfo timing_info = {
          .duration = kCMTimeInvalid,
          .presentationTimeStamp = CMTimeMake(0, 1),
          .decodeTimeStamp = kCMTimeInvalid};

      ASSERT_EQ(
          CMSampleBufferCreateForImageBuffer(
              kCFAllocatorDefault, pixel_buffer.get(), true, nullptr, nullptr,
              format_desc.get(), &timing_info, sample_buffer.InitializeInto()),
          noErr);
    }

    [((FakeSCStream*)g_captured_stream).output
                       stream:(SCStream*)g_captured_stream
        didOutputSampleBuffer:sample_buffer.get()
                       ofType:SCStreamOutputTypeScreen];
  }

  void SimulateAndVerifyFrame(media::MockVideoCaptureDeviceClient* mock_client)
      API_AVAILABLE(macos(12.3)) {
    base::RunLoop frame_loop;
    EXPECT_CALL(*mock_client,
                OnIncomingCapturedExternalBuffer(_, _, _, _, _, _))
        .WillOnce(base::test::RunClosure(frame_loop.QuitClosure()));

    ASSERT_NO_FATAL_FAILURE(SimulateFrame());
    frame_loop.Run();
  }
};

TEST_F(ScreenCaptureKitDeviceMacTest, StartAndCaptureFrame) {
  if (@available(macOS 13.2, *)) {
    FakeSCRunningApplication* app =
        [[FakeSCRunningApplication alloc] initWithProcessID:100
                                            applicationName:@"Fake App"
                                           bundleIdentifier:@"com.fake.app"];
    FakeSCWindow* window =
        [[FakeSCWindow alloc] initWithID:1
                                   title:@"Fake Window"
                       owningApplication:app
                             windowLayer:0
                                   frame:CGRectMake(0, 0, 100, 100)
                                onScreen:YES];
    SetupContent(@[ window ]);

    DesktopMediaID source(DesktopMediaID::TYPE_WINDOW, 1);
    InitDevice(source);

    auto mock_client = CreateClient();
    media::MockVideoCaptureDeviceClient* mock_client_ptr = mock_client.get();

    // Transfer ownership to device.
    ASSERT_NO_FATAL_FAILURE(StartDevice(std::move(mock_client)));

    // Send frame and verify.
    ASSERT_NO_FATAL_FAILURE(SimulateAndVerifyFrame(mock_client_ptr));

    device_->StopAndDeAllocate();
  } else {
    GTEST_SKIP();
  }
}

}  // namespace content

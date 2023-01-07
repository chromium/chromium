// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/mac/scoped_nsobject.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/io_surface_capture_device_base_mac.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/gfx/native_widget_types.h"

using SampleCallback = base::RepeatingCallback<void(gfx::ScopedInUseIOSurface)>;
using ErrorCallback = base::RepeatingClosure;

API_AVAILABLE(macos(12.3))
@interface ScreenCaptureKitDeviceHelper
    : NSObject <SCStreamDelegate, SCStreamOutput> {
  SampleCallback _sampleCallback;
  ErrorCallback _errorCallback;
}

- (instancetype)initWithSampleCallback:(SampleCallback)sampleCallback
                         errorCallback:(ErrorCallback)errorCallback;
@end

@implementation ScreenCaptureKitDeviceHelper

- (instancetype)initWithSampleCallback:(SampleCallback)sampleCallback
                         errorCallback:(ErrorCallback)errorCallback {
  if (self = [super init]) {
    _sampleCallback = sampleCallback;
    _errorCallback = errorCallback;
  }
  return self;
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer)
    return;
  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface)
    return;
  _sampleCallback.Run(
      gfx::ScopedInUseIOSurface(ioSurface, base::scoped_policy::RETAIN));
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  _errorCallback.Run();
}

@end

namespace content {

namespace {

class API_AVAILABLE(macos(12.3)) ScreenCaptureKitDeviceMac
    : public IOSurfaceCaptureDeviceBase {
 public:
  ScreenCaptureKitDeviceMac(const DesktopMediaID& source)
      : source_(source),
        device_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
    SampleCallback sample_callback = base::BindPostTask(
        base::ThreadTaskRunnerHandle::Get(),
        base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamSample,
                            weak_factory_.GetWeakPtr()));
    ErrorCallback error_callback = base::BindPostTask(
        base::ThreadTaskRunnerHandle::Get(),
        base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamError,
                            weak_factory_.GetWeakPtr()));
    helper_.reset([[ScreenCaptureKitDeviceHelper alloc]
        initWithSampleCallback:sample_callback
                 errorCallback:error_callback]);
  }
  ScreenCaptureKitDeviceMac(const ScreenCaptureKitDeviceMac&) = delete;
  ScreenCaptureKitDeviceMac& operator=(const ScreenCaptureKitDeviceMac&) =
      delete;
  ~ScreenCaptureKitDeviceMac() override = default;

  void OnShareableContentCreated(
      base::scoped_nsobject<SCShareableContent> content) {
    if (!content) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedGetShareableContent,
          FROM_HERE, "Failed getShareableContentWithCompletionHandler");
      return;
    }

    base::scoped_nsobject<SCContentFilter> filter;
    gfx::Size content_size;
    switch (source_.type) {
      case DesktopMediaID::TYPE_SCREEN:
        for (SCDisplay* display : [content displays]) {
          if (source_.id == [display displayID]) {
            NSArray<SCWindow*>* exclude_windows = nil;
            filter.reset([[SCContentFilter alloc]
                 initWithDisplay:display
                excludingWindows:exclude_windows]);
            content_size = gfx::Size([display width], [display height]);
            break;
          }
        }
        break;
      case DesktopMediaID::TYPE_WINDOW:
        for (SCWindow* window : [content windows]) {
          if (source_.id == [window windowID]) {
            filter.reset([[SCContentFilter alloc]
                initWithDesktopIndependentWindow:window]);
            CGRect frame = [window frame];
            content_size = gfx::Size(frame.size);
            break;
          }
        }
        break;
      default:
        NOTREACHED();
        break;
    }
    if (!filter) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedToFindSCDisplay,
          FROM_HERE, "Failed to find SCDisplay");
      return;
    }

    gfx::RectF dest_rect_in_frame;
    actual_capture_format_ = capture_params().requested_format;
    actual_capture_format_.pixel_format = media::PIXEL_FORMAT_NV12;
    ComputeFrameSizeAndDestRect(content_size, actual_capture_format_.frame_size,
                                dest_rect_in_frame);

    base::scoped_nsobject<SCStreamConfiguration> config(
        [[SCStreamConfiguration alloc] init]);
    [config setWidth:actual_capture_format_.frame_size.width()];
    [config setHeight:actual_capture_format_.frame_size.height()];
    [config setPixelFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange];
    [config setDestinationRect:dest_rect_in_frame.ToCGRect()];
    [config setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
    [config setScalesToFit:YES];
    [config setShowsCursor:YES];
    [config setColorSpaceName:kCGColorSpaceSRGB];
    [config
        setMinimumFrameInterval:CMTimeMakeWithSeconds(
                                    1 / actual_capture_format_.frame_rate, 1)];
    stream_.reset([[SCStream alloc] initWithFilter:filter
                                     configuration:config
                                          delegate:helper_]);
    {
      NSError* error = nil;
      bool add_stream_output_result =
          [stream_ addStreamOutput:helper_
                              type:SCStreamOutputTypeScreen
                sampleHandlerQueue:dispatch_get_main_queue()
                             error:&error];
      if (!add_stream_output_result) {
        stream_.reset();
        client()->OnError(
            media::VideoCaptureError::kScreenCaptureKitFailedAddStreamOutput,
            FROM_HERE, "Failed addStreamOutput");
        return;
      }
    }

    auto stream_started_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamStarted,
                            weak_factory_.GetWeakPtr()));
    auto handler = ^(NSError* error) {
      stream_started_callback.Run(!!error);
    };
    [stream_ startCaptureWithCompletionHandler:handler];
  }
  void OnStreamStarted(bool error) {
    if (error) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedStartCapture,
          FROM_HERE, "Failed startCaptureWithCompletionHandler");
      return;
    }
    client()->OnStarted();
  }
  void OnStreamStopped(bool error) {
    if (error) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedStopCapture,
          FROM_HERE, "Failed stopCaptureWithCompletionHandler");
      return;
    }
  }
  void OnStreamSample(gfx::ScopedInUseIOSurface io_surface) {
    // TODO(https://crbug.com/1309653): Reconfigure the stream if the IOSurface
    // should be resized.
    OnReceivedIOSurfaceFromStream(io_surface, actual_capture_format_);
  }
  void OnStreamError() {
    client()->OnError(media::VideoCaptureError::kScreenCaptureKitStreamError,
                      FROM_HERE, "Stream delegate called didStopWithError");
  }

  // IOSurfaceCaptureDeviceBase:
  void OnStart() override {
    auto content_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(
            &ScreenCaptureKitDeviceMac::OnShareableContentCreated,
            weak_factory_.GetWeakPtr()));
    auto handler = ^(SCShareableContent* content, NSError* error) {
      content_callback.Run(base::scoped_nsobject<SCShareableContent>(
          content, base::scoped_policy::RETAIN));
    };
    [SCShareableContent getShareableContentWithCompletionHandler:handler];
  }
  void OnStop() override {
    if (stream_) {
      auto stream_started_callback = base::BindPostTask(
          device_task_runner_,
          base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamStopped,
                              weak_factory_.GetWeakPtr()));
      auto handler = ^(NSError* error) {
        stream_started_callback.Run(!!error);
      };
      [stream_ stopCaptureWithCompletionHandler:handler];

      NSError* error = nil;
      bool remove_stream_output_result =
          [stream_ removeStreamOutput:helper_
                                 type:SCStreamOutputTypeScreen
                                error:&error];
      if (!remove_stream_output_result)
        DLOG(ERROR) << "Failed removeStreamOutput";
    }

    weak_factory_.InvalidateWeakPtrs();
    stream_.reset();
  }

 private:
  const DesktopMediaID source_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // The actual format of the video frames that are sent to `client`.
  media::VideoCaptureFormat actual_capture_format_;

  // Helper class that acts as output and delegate for `stream_`.
  base::scoped_nsobject<ScreenCaptureKitDeviceHelper> helper_;

  // The stream that does the capturing.
  base::scoped_nsobject<SCStream> stream_;

  base::WeakPtrFactory<ScreenCaptureKitDeviceMac> weak_factory_{this};
};

}  // namespace

std::unique_ptr<media::VideoCaptureDevice> CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source) {
  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN:
      // ScreenCaptureKitDeviceMac only supports a single display at a time. It
      // will not stitch desktops together.
      // https://crbug.com/1178360
      if (source.id == webrtc::kFullDesktopScreenId ||
          source.id == webrtc::kInvalidScreenId) {
        return nullptr;
      }
      break;
    case DesktopMediaID::TYPE_WINDOW:
      break;
    default:
      // ScreenCaptureKitDeviceMac supports only TYPE_SCREEN and TYPE_WINDOW.
      // https://crbug.com/1176900
      return nullptr;
  }

  IncrementDesktopCaptureCounter(SCREEN_CAPTURER_CREATED);
  IncrementDesktopCaptureCounter(source.audio_share
                                     ? SCREEN_CAPTURER_CREATED_WITH_AUDIO
                                     : SCREEN_CAPTURER_CREATED_WITHOUT_AUDIO);
  if (@available(macos 12.3, *))
    return std::make_unique<ScreenCaptureKitDeviceMac>(source);
  return nullptr;
}

}  // namespace content

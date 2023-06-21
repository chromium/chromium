// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/task/bind_post_task.h"
#import "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/io_surface_capture_device_base_mac.h"
#include "content/browser/media/capture/screen_capture_kit_fullscreen_module.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/gfx/native_widget_types.h"

using SampleCallback = base::RepeatingCallback<void(gfx::ScopedInUseIOSurface,
                                                    absl::optional<gfx::Size>,
                                                    absl::optional<gfx::Rect>)>;
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

  // Read out width, height and scaling from metadata to determine
  // |contentSize|, which is the size of the content on screen, and
  // |visibleRect|, which is the region of the IOSurface that contains the
  // captured content. |contentSize| is used to detect when a captured window is
  // resized so that the stream configuration can be updated and |visibleRect|
  // is needed because the IOSurface may be larger than the captured content.
  absl::optional<gfx::Size> contentSize;
  absl::optional<gfx::Rect> visibleRect;
  CFArrayRef attachmentsArray =
      CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
  if (attachmentsArray && CFArrayGetCount(attachmentsArray) > 0) {
    CFDictionaryRef attachment = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(attachmentsArray, 0));
    if (attachment) {
      CFDictionaryRef contentRectValue = base::mac::CFCast<CFDictionaryRef>(
          CFDictionaryGetValue(attachment, SCStreamFrameInfoContentRect));
      CFNumberRef scaleFactorValue = base::mac::CFCast<CFNumberRef>(
          CFDictionaryGetValue(attachment, SCStreamFrameInfoScaleFactor));
      CFNumberRef contentScaleValue = base::mac::CFCast<CFNumberRef>(
          CFDictionaryGetValue(attachment, SCStreamFrameInfoContentScale));

      if (contentRectValue && scaleFactorValue && contentScaleValue) {
        CGRect contentRect = {};
        bool succeed = CGRectMakeWithDictionaryRepresentation(contentRectValue,
                                                              &contentRect);
        float scaleFactor = 1.0f;
        succeed &= CFNumberGetValue(scaleFactorValue, kCFNumberFloatType,
                                    &scaleFactor);
        float contentScale = 1.0f;
        succeed &= CFNumberGetValue(contentScaleValue, kCFNumberFloatType,
                                    &contentScale);
        if (succeed) {
          contentRect.size.width *= scaleFactor;
          contentRect.size.height *= scaleFactor;
          visibleRect.emplace(contentRect);
          contentSize.emplace(round(contentRect.size.width / contentScale),
                              round(contentRect.size.height / contentScale));
        }
      }
    }
  }
  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface)
    return;
  _sampleCallback.Run(
      gfx::ScopedInUseIOSurface(ioSurface, base::scoped_policy::RETAIN),
      contentSize, visibleRect);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  _errorCallback.Run();
}

+ (base::scoped_nsobject<SCStreamConfiguration>)
    createStreamConfigurationWithFrameSize:(gfx::Size)frameSize
                           destRectInFrame:(gfx::RectF)destRectInFrame
                                 frameRate:(float)frameRate {
  base::scoped_nsobject<SCStreamConfiguration> config(
      [[SCStreamConfiguration alloc] init]);
  [config setWidth:frameSize.width()];
  [config setHeight:frameSize.height()];
  [config setPixelFormat:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange];
  [config setDestinationRect:destRectInFrame.ToCGRect()];
  [config setBackgroundColor:CGColorGetConstantColor(kCGColorBlack)];
  [config setScalesToFit:YES];
  [config setShowsCursor:YES];
  [config setColorSpaceName:kCGColorSpaceSRGB];
  [config setMinimumFrameInterval:CMTimeMake(media::kFrameRatePrecision,
                                             static_cast<int>(
                                                 frameRate *
                                                 media::kFrameRatePrecision))];
  return config;
}

@end

namespace content {

namespace {

class API_AVAILABLE(macos(12.3)) ScreenCaptureKitDeviceMac
    : public IOSurfaceCaptureDeviceBase,
      public ScreenCaptureKitResetStreamInterface {
 public:
  ScreenCaptureKitDeviceMac(const DesktopMediaID& source)
      : source_(source),
        device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
    SampleCallback sample_callback = base::BindPostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamSample,
                            weak_factory_.GetWeakPtr()));
    ErrorCallback error_callback = base::BindPostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
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
    switch (source_.type) {
      case DesktopMediaID::TYPE_SCREEN:
        for (SCDisplay* display : [content displays]) {
          if (source_.id == [display displayID]) {
            NSArray<SCWindow*>* exclude_windows = nil;
            filter.reset([[SCContentFilter alloc]
                 initWithDisplay:display
                excludingWindows:exclude_windows]);
            stream_config_content_size_ =
                gfx::Size([display width], [display height]);
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
            stream_config_content_size_ = gfx::Size(frame.size);
            if (!fullscreen_module_) {
              fullscreen_module_ = MaybeCreateScreenCaptureKitFullscreenModule(
                  device_task_runner_, *this, window);
            }
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
    ComputeFrameSizeAndDestRect(stream_config_content_size_,
                                actual_capture_format_.frame_size,
                                dest_rect_in_frame);
    base::scoped_nsobject<SCStreamConfiguration> config =
        [ScreenCaptureKitDeviceHelper
            createStreamConfigurationWithFrameSize:actual_capture_format_
                                                       .frame_size
                                   destRectInFrame:dest_rect_in_frame
                                         frameRate:actual_capture_format_
                                                       .frame_rate];
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

    if (fullscreen_module_) {
      fullscreen_module_->Start();
    }
  }
  void OnStreamStopped(bool error) {
    if (error) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedStopCapture,
          FROM_HERE, "Failed stopCaptureWithCompletionHandler");
      return;
    }
  }
  void OnStreamSample(gfx::ScopedInUseIOSurface io_surface,
                      absl::optional<gfx::Size> content_size,
                      absl::optional<gfx::Rect> visible_rect) {
    if (requested_capture_format_) {
      // Does the size of io_surface match the requested format?
      size_t io_surface_width = IOSurfaceGetWidth(io_surface);
      size_t io_surface_height = IOSurfaceGetHeight(io_surface);
      DVLOG(3) << "Waiting for new capture format, "
               << requested_capture_format_->frame_size.width() << " x "
               << requested_capture_format_->frame_size.height()
               << ". IO surface size " << io_surface_width << " x "
               << io_surface_height;
      if (static_cast<size_t>(requested_capture_format_->frame_size.width()) ==
              io_surface_width &&
          static_cast<size_t>(requested_capture_format_->frame_size.height()) ==
              io_surface_height) {
        actual_capture_format_ = requested_capture_format_.value();
        requested_capture_format_.reset();
      }
    } else {
      // No current request for new capture format. Check to see if content_size
      // has changed and requires an updated configuration. We only track the
      // content size for window capturing since the resolution does not
      // normally change during a session and because the content scale is wrong
      // for retina displays.
      if (source_.type == DesktopMediaID::TYPE_WINDOW && content_size &&
          (stream_config_content_size_.width() != content_size->width() ||
           stream_config_content_size_.height() != content_size->height())) {
        DVLOG(3) << "Content size changed to " << content_size->width() << " x "
                 << content_size->height() << ". It was "
                 << stream_config_content_size_.width() << " x "
                 << stream_config_content_size_.height();
        stream_config_content_size_ = content_size.value();
        gfx::RectF dest_rect_in_frame;
        gfx::Size new_frame_size;
        ComputeFrameSizeAndDestRect(stream_config_content_size_, new_frame_size,
                                    dest_rect_in_frame);
        if (new_frame_size.width() !=
                actual_capture_format_.frame_size.width() ||
            new_frame_size.height() !=
                actual_capture_format_.frame_size.height()) {
          DVLOG(3) << "Calling updateConfiguration with new frame size: "
                   << new_frame_size.width() << " x "
                   << new_frame_size.height();
          requested_capture_format_ = actual_capture_format_;
          requested_capture_format_->frame_size = new_frame_size;
          // Update stream configuration.
          base::scoped_nsobject<SCStreamConfiguration> config =
              [ScreenCaptureKitDeviceHelper
                  createStreamConfigurationWithFrameSize:
                      requested_capture_format_->frame_size
                                         destRectInFrame:dest_rect_in_frame
                                               frameRate:
                                                   requested_capture_format_->
                                                   frame_rate];
          [stream_
              updateConfiguration:config
                completionHandler:^(NSError* _Nullable error) {
                  if (error) {
                    client()->OnError(
                        media::VideoCaptureError::kScreenCaptureKitStreamError,
                        FROM_HERE, "Error on updateConfiguration");
                  }
                }];
        }
      }
    }
    // The IO surface may be larger than the actual content size. Pass on
    // visible rect to be able to render/encode the frame correctly.
    OnReceivedIOSurfaceFromStream(
        io_surface, actual_capture_format_,
        visible_rect.value_or(gfx::Rect(actual_capture_format_.frame_size)));
  }
  void OnStreamError() {
    if (is_resetting_ || (fullscreen_module_ &&
                          fullscreen_module_->is_fullscreen_window_active())) {
      // Clear `is_resetting_` because the completion handler in ResetStreamTo()
      // may not be called if there's an error.
      is_resetting_ = false;

      // The stream_ is no longer valid. Restart the stream from scratch.
      if (fullscreen_module_) {
        fullscreen_module_->Reset();
      }
      OnStart();
    } else {
      client()->OnError(media::VideoCaptureError::kScreenCaptureKitStreamError,
                        FROM_HERE, "Stream delegate called didStopWithError");
    }
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

  // ScreenCaptureKitResetStreamInterface.
  void ResetStreamTo(SCWindow* window) override {
    if (!window || is_resetting_) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitResetStreamError,
          FROM_HERE, "Error on ResetStreamTo.");
      return;
    }

    is_resetting_ = true;
    base::scoped_nsobject<SCContentFilter> filter;
    filter.reset(
        [[SCContentFilter alloc] initWithDesktopIndependentWindow:window]);

    [stream_ updateContentFilter:filter
               completionHandler:^(NSError* _Nullable error) {
                 is_resetting_ = false;
                 if (error) {
                   client()->OnError(
                       media::VideoCaptureError::kScreenCaptureKitStreamError,
                       FROM_HERE,
                       "Error on updateContentFilter (fullscreen window).");
                 }
               }];
  }

 private:
  const DesktopMediaID source_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // The actual format of the video frames that are sent to `client`.
  media::VideoCaptureFormat actual_capture_format_;

  // The requested format if a request to update the configuration has been
  // sent.
  absl::optional<media::VideoCaptureFormat> requested_capture_format_;

  // The size of the content at the time that we configured the stream.
  gfx::Size stream_config_content_size_;

  // Helper class that acts as output and delegate for `stream_`.
  base::scoped_nsobject<ScreenCaptureKitDeviceHelper> helper_;

  // This is used to detect when a captured presentation enters fullscreen mode.
  // If this happens, the module will call the ResetStreamTo function.
  std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module_;

  bool is_resetting_ = false;

  // The stream that does the capturing.
  base::scoped_nsobject<SCStream> stream_;

  base::WeakPtrFactory<ScreenCaptureKitDeviceMac> weak_factory_{this};
};

}  // namespace

std::unique_ptr<media::VideoCaptureDevice> CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source) {
  // Although ScreenCaptureKit is available in 12.3 there were some bugs that
  // were not fixed until 13.2.
  if (@available(macOS 13.2, *)) {
    switch (source.type) {
      case DesktopMediaID::TYPE_SCREEN:
        // ScreenCaptureKitDeviceMac only supports a single display at a time.
        // It will not stitch desktops together. https://crbug.com/1178360
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

    return std::make_unique<ScreenCaptureKitDeviceMac>(source);
  }
  return nullptr;
}

}  // namespace content

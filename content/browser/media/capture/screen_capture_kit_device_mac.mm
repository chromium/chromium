// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_device_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <optional>
#include <tuple>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/task/bind_post_task.h"
#import "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/io_surface_capture_device_base_mac.h"
#include "content/browser/media/capture/screen_capture_kit_fullscreen_module.h"
#include "content/public/common/content_features.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "ui/gfx/native_widget_types.h"

using SampleCallback = base::RepeatingCallback<void(gfx::ScopedInUseIOSurface,
                                                    std::optional<gfx::Size>,
                                                    std::optional<gfx::Rect>,
                                                    bool)>;
using ErrorCallback = base::RepeatingClosure;

namespace {
API_AVAILABLE(macos(12.3))
std::tuple<std::optional<gfx::Rect>, std::optional<gfx::Size>>
GetVisibleRectAndContentSize(CFDictionaryRef attachment) {
  std::optional<gfx::Rect> visibleRect;
  std::optional<gfx::Size> contentSize;

  CFDictionaryRef contentRectValue = base::apple::CFCast<CFDictionaryRef>(
      CFDictionaryGetValue(attachment, base::apple::NSToCFPtrCast(
                                           SCStreamFrameInfoContentRect)));
  CFNumberRef scaleFactorValue = base::apple::CFCast<CFNumberRef>(
      CFDictionaryGetValue(attachment, base::apple::NSToCFPtrCast(
                                           SCStreamFrameInfoScaleFactor)));
  CFNumberRef contentScaleValue = base::apple::CFCast<CFNumberRef>(
      CFDictionaryGetValue(attachment, base::apple::NSToCFPtrCast(
                                           SCStreamFrameInfoContentScale)));

  if (contentRectValue && scaleFactorValue && contentScaleValue) {
    CGRect contentRect = {};
    bool succeed =
        CGRectMakeWithDictionaryRepresentation(contentRectValue, &contentRect);
    float scaleFactor = 1.0f;
    succeed &=
        CFNumberGetValue(scaleFactorValue, kCFNumberFloatType, &scaleFactor);
    float contentScale = 1.0f;
    succeed &=
        CFNumberGetValue(contentScaleValue, kCFNumberFloatType, &contentScale);
    if (succeed) {
      contentRect.size.width *= scaleFactor;
      contentRect.size.height *= scaleFactor;
      visibleRect.emplace(contentRect);
      contentSize.emplace(round(contentRect.size.width / contentScale),
                          round(contentRect.size.height / contentScale));
    }
  }
  return std::make_tuple(visibleRect, contentSize);
}

bool IsPresenterOverlayLargeActive(CFDictionaryRef attachment) {
  if (@available(macOS 14.2, *)) {
    CFDictionaryRef overlayContentRectValue =
        base::apple::CFCast<CFDictionaryRef>(CFDictionaryGetValue(
            attachment, base::apple::NSToCFPtrCast(
                            SCStreamFrameInfoPresenterOverlayContentRect)));
    if (!overlayContentRectValue) {
      return false;
    }

    CGRect overlayContentRect = {};
    bool succeed = CGRectMakeWithDictionaryRepresentation(
        overlayContentRectValue, &overlayContentRect);
    // From local testing:
    // height > 0 and width > 0 signal that the presenter overlay is active.
    // x == y == 0 is used for the small overlay, where the capture size is the
    // same as the original content size.
    // x > 0 and y > 0 signal that the large overlay that is causing problems is
    // active. In this case the original screen capture is overlayed on the
    // frames captured by the camera with the presenter in the foreground.
    if (succeed && overlayContentRect.size.width > 0 &&
        overlayContentRect.size.height > 0 && overlayContentRect.origin.x > 0 &&
        overlayContentRect.origin.y > 0) {
      return true;
    }
  }
  return false;
}
}  // namespace

API_AVAILABLE(macos(12.3))
@interface ScreenCaptureKitDeviceHelper
    : NSObject <SCStreamDelegate, SCStreamOutput>

- (instancetype)initWithSampleCallback:(SampleCallback)sampleCallback
                         errorCallback:(ErrorCallback)errorCallback;
@end

@implementation ScreenCaptureKitDeviceHelper {
  SampleCallback _sampleCallback;
  ErrorCallback _errorCallback;
}

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
  std::optional<gfx::Size> contentSize;
  std::optional<gfx::Rect> visibleRect;
  bool isPresenterOverlayLargeActive = false;
  CFArrayRef attachmentsArray = CMSampleBufferGetSampleAttachmentsArray(
      sampleBuffer, /*createIfNecessary=*/false);
  if (attachmentsArray && CFArrayGetCount(attachmentsArray) > 0) {
    CFDictionaryRef attachment = base::apple::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(attachmentsArray, 0));
    if (attachment) {
      std::tie(visibleRect, contentSize) =
          GetVisibleRectAndContentSize(attachment);
      isPresenterOverlayLargeActive = IsPresenterOverlayLargeActive(attachment);
    }
  }
  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface)
    return;
  _sampleCallback.Run(
      gfx::ScopedInUseIOSurface(ioSurface, base::scoped_policy::RETAIN),
      contentSize, visibleRect, isPresenterOverlayLargeActive);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  _errorCallback.Run();
}

+ (SCStreamConfiguration*)streamConfigurationWithFrameSize:(gfx::Size)frameSize
                                           destRectInFrame:
                                               (gfx::RectF)destRectInFrame
                                                 frameRate:(float)frameRate {
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  config.width = frameSize.width();
  config.height = frameSize.height();
  config.pixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
  config.destinationRect = destRectInFrame.ToCGRect();
  config.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
  config.scalesToFit = YES;
  config.showsCursor = YES;
  config.colorSpaceName = kCGColorSpaceSRGB;
  config.minimumFrameInterval =
      CMTimeMake(media::kFrameRatePrecision,
                 static_cast<int>(frameRate * media::kFrameRatePrecision));
  return config;
}

@end

namespace content {

namespace {

BASE_FEATURE(kScreenCaptureKitFullDesktopFallback,
             "ScreenCaptureKitFullDesktopFallback",
             base::FEATURE_ENABLED_BY_DEFAULT);

class API_AVAILABLE(macos(12.3)) ScreenCaptureKitDeviceMac
    : public IOSurfaceCaptureDeviceBase,
      public ScreenCaptureKitResetStreamInterface {
 public:
  explicit ScreenCaptureKitDeviceMac(const DesktopMediaID& source,
                                     SCContentFilter* filter)
      : source_(source),
        filter_(filter),
        device_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
    SampleCallback sample_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamSample,
                            weak_factory_.GetWeakPtr()));
    ErrorCallback error_callback = base::BindPostTask(
        device_task_runner_,
        base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamError,
                            weak_factory_.GetWeakPtr()));
    helper_ = [[ScreenCaptureKitDeviceHelper alloc]
        initWithSampleCallback:sample_callback
                 errorCallback:error_callback];
  }
  ScreenCaptureKitDeviceMac(const ScreenCaptureKitDeviceMac&) = delete;
  ScreenCaptureKitDeviceMac& operator=(const ScreenCaptureKitDeviceMac&) =
      delete;
  ~ScreenCaptureKitDeviceMac() override = default;

  void OnShareableContentCreated(SCShareableContent* content) {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

    if (!content) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedGetShareableContent,
          FROM_HERE, "Failed getShareableContentWithCompletionHandler");
      return;
    }

    SCContentFilter* filter;
    switch (source_.type) {
      case DesktopMediaID::TYPE_SCREEN:
        for (SCDisplay* display in content.displays) {
          // There's currently no support for stitching desktops together as
          // requested by kFullDesktopScreenId. Capture the first display as a
          // fallback. See https://crbug.com/325530044.
          if (source_.id == display.displayID ||
              source_.id == webrtc::kFullDesktopScreenId) {
            filter = [[SCContentFilter alloc] initWithDisplay:display
                                             excludingWindows:@[]];
            stream_config_content_size_ =
                gfx::Size(display.width, display.height);
            break;
          }
        }
        break;
      case DesktopMediaID::TYPE_WINDOW:
        for (SCWindow* window in content.windows) {
          if (source_.id == window.windowID) {
            filter = [[SCContentFilter alloc]
                initWithDesktopIndependentWindow:window];
            CGRect frame = window.frame;
            stream_config_content_size_ = gfx::Size(frame.size);
            if (!fullscreen_module_) {
              fullscreen_module_ = MaybeCreateScreenCaptureKitFullscreenModule(
                  device_task_runner_, *this, window);
            }
          }
        }
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }

    CreateStream(filter);
  }

  void CreateStream(SCContentFilter* filter) {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
    if (!filter) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedToFindSCDisplay,
          FROM_HERE, "Failed to find SCDisplay");
      return;
    }

    if (@available(macOS 14.0, *)) {
      // Update the content size. This step is neccessary when used together
      // with SCContentSharingPicker. If the Chrome picker is used, it will
      // change to retina resolution if applicable.
      stream_config_content_size_ =
          gfx::Size(filter.contentRect.size.width * filter.pointPixelScale,
                    filter.contentRect.size.height * filter.pointPixelScale);
    }

    gfx::RectF dest_rect_in_frame;
    actual_capture_format_ = capture_params().requested_format;
    actual_capture_format_.pixel_format = media::PIXEL_FORMAT_NV12;
    ComputeFrameSizeAndDestRect(stream_config_content_size_,
                                actual_capture_format_.frame_size,
                                dest_rect_in_frame);
    SCStreamConfiguration* config = [ScreenCaptureKitDeviceHelper
        streamConfigurationWithFrameSize:actual_capture_format_.frame_size
                         destRectInFrame:dest_rect_in_frame
                               frameRate:actual_capture_format_.frame_rate];
    stream_ = [[SCStream alloc] initWithFilter:filter
                                 configuration:config
                                      delegate:helper_];
    {
      NSError* error = nil;
      bool add_stream_output_result =
          [stream_ addStreamOutput:helper_
                              type:SCStreamOutputTypeScreen
                sampleHandlerQueue:dispatch_get_main_queue()
                             error:&error];
      if (!add_stream_output_result) {
        stream_ = nil;
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
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

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
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

    if (error) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitFailedStopCapture,
          FROM_HERE, "Failed stopCaptureWithCompletionHandler");
      return;
    }
  }
  void OnStreamSample(gfx::ScopedInUseIOSurface io_surface,
                      std::optional<gfx::Size> content_size,
                      std::optional<gfx::Rect> visible_rect,
                      bool is_presenter_overlay_large_active) {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

    if (requested_capture_format_) {
      // Does the size of io_surface match the requested format?
      size_t io_surface_width = IOSurfaceGetWidth(io_surface.get());
      size_t io_surface_height = IOSurfaceGetHeight(io_surface.get());
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

        // There's a small variation in the reported content size when the large
        // presenter overlay is active which may result in updateConfiguration()
        // being repeatedly called at a high frequency toggling between two
        // different sizes. Avoid this by not reacting on too small changes.
        // The threshold was determined by local testing.
        constexpr int kDefaultThreshold = 1;
        constexpr int kPresenterOverlayLargeThreshold = 4;
        int threshold = is_presenter_overlay_large_active
                            ? kPresenterOverlayLargeThreshold
                            : kDefaultThreshold;
        if (std::abs(new_frame_size.width() -
                     actual_capture_format_.frame_size.width()) >= threshold ||
            std::abs(new_frame_size.height() -
                     actual_capture_format_.frame_size.height()) >= threshold) {
          DVLOG(3) << "Calling updateConfiguration with new frame size: "
                   << new_frame_size.width() << " x "
                   << new_frame_size.height();
          requested_capture_format_ = actual_capture_format_;
          requested_capture_format_->frame_size = new_frame_size;
          // Update stream configuration.
          SCStreamConfiguration* config = [ScreenCaptureKitDeviceHelper
              streamConfigurationWithFrameSize:requested_capture_format_
                                                   ->frame_size
                               destRectInFrame:dest_rect_in_frame
                                     frameRate:requested_capture_format_->
                                               frame_rate];

          __block base::OnceCallback<void()> on_update_configuration_error =
              base::BindPostTask(
                  device_task_runner_,
                  base::BindOnce(
                      &ScreenCaptureKitDeviceMac::OnUpdateConfigurationError,
                      weak_factory_.GetWeakPtr()));
          [stream_
              updateConfiguration:config
                completionHandler:^(NSError* _Nullable error) {
                  if (error) {
                    std::move(on_update_configuration_error).Run();
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
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

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
  void OnUpdateContentFilterCompleted(NSError* _Nullable error) {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
    is_resetting_ = false;

    if (error) {
      client()->OnError(media::VideoCaptureError::kScreenCaptureKitStreamError,
                        FROM_HERE,
                        "Error on updateContentFilter (fullscreen window).");
    }
  }
  void OnUpdateConfigurationError() {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
    client()->OnError(media::VideoCaptureError::kScreenCaptureKitStreamError,
                      FROM_HERE, "Error on updateConfiguration");
  }

  // IOSurfaceCaptureDeviceBase:
  void OnStart() override {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
    if (filter_) {
      // SCContentSharingPicker is used where filter_ is set on creation.
      CreateStream(filter_);
    } else {
      // Chrome picker is used.
      auto content_callback = base::BindPostTask(
          device_task_runner_,
          base::BindRepeating(
              &ScreenCaptureKitDeviceMac::OnShareableContentCreated,
              weak_factory_.GetWeakPtr()));
      auto handler = ^(SCShareableContent* content, NSError* error) {
        content_callback.Run(content);
      };
      [SCShareableContent getShareableContentWithCompletionHandler:handler];
    }
  }
  void OnStop() override {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

    if (stream_) {
      auto stream_stopped_callback = base::BindPostTask(
          device_task_runner_,
          base::BindRepeating(&ScreenCaptureKitDeviceMac::OnStreamStopped,
                              weak_factory_.GetWeakPtr()));
      auto handler = ^(NSError* error) {
        stream_stopped_callback.Run(!!error);
      };
      [stream_ stopCaptureWithCompletionHandler:handler];

      NSError* error = nil;
      bool remove_stream_output_result =
          [stream_ removeStreamOutput:helper_
                                 type:SCStreamOutputTypeScreen
                                error:&error];
      if (!remove_stream_output_result) {
        DLOG(ERROR) << "Failed removeStreamOutput";
      }
    }

    weak_factory_.InvalidateWeakPtrs();
    stream_ = nil;
  }

  // ScreenCaptureKitResetStreamInterface.
  void ResetStreamTo(SCWindow* window) override {
    DCHECK(device_task_runner_->RunsTasksInCurrentSequence());

    if (!window || is_resetting_) {
      client()->OnError(
          media::VideoCaptureError::kScreenCaptureKitResetStreamError,
          FROM_HERE, "Error on ResetStreamTo.");
      return;
    }

    is_resetting_ = true;
    SCContentFilter* filter =
        [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];

    __block base::OnceCallback<void(NSError*)>
        on_update_content_filter_completed = base::BindPostTask(
            device_task_runner_,
            base::BindOnce(
                &ScreenCaptureKitDeviceMac::OnUpdateContentFilterCompleted,
                weak_factory_.GetWeakPtr()));

    [stream_ updateContentFilter:filter
               completionHandler:^(NSError* _Nullable error) {
                 std::move(on_update_content_filter_completed).Run(error);
               }];
  }

 private:
  const DesktopMediaID source_;
  SCContentFilter* const filter_;
  const scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // The actual format of the video frames that are sent to `client`.
  media::VideoCaptureFormat actual_capture_format_;

  // The requested format if a request to update the configuration has been
  // sent.
  std::optional<media::VideoCaptureFormat> requested_capture_format_;

  // The size of the content at the time that we configured the stream.
  gfx::Size stream_config_content_size_;

  // Helper class that acts as output and delegate for `stream_`.
  ScreenCaptureKitDeviceHelper* __strong helper_;

  // This is used to detect when a captured presentation enters fullscreen mode.
  // If this happens, the module will call the ResetStreamTo function.
  std::unique_ptr<ScreenCaptureKitFullscreenModule> fullscreen_module_;

  bool is_resetting_ = false;

  // The stream that does the capturing.
  SCStream* __strong stream_;

  base::WeakPtrFactory<ScreenCaptureKitDeviceMac> weak_factory_{this};
};

}  // namespace

// Although ScreenCaptureKit is available in 12.3 there were some bugs that
// were not fixed until 13.2.
API_AVAILABLE(macos(13.2))
std::unique_ptr<media::VideoCaptureDevice> CreateScreenCaptureKitDeviceMac(
    const DesktopMediaID& source,
    SCContentFilter* filter) {
  switch (source.type) {
    case DesktopMediaID::TYPE_SCREEN:
      // ScreenCaptureKitDeviceMac only supports a single display at a time.
      // It will not stitch desktops together. If
      // kScreenCaptureKitFullDesktopFallback is enabled, we will fallback to
      // capturing the first display in the list returned from
      // getShareableContent. https://crbug.com/1178360 and
      // https://crbug.com/325530044
      if ((source.id == webrtc::kFullDesktopScreenId &&
           !base::FeatureList::IsEnabled(
               kScreenCaptureKitFullDesktopFallback)) ||
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

  return std::make_unique<ScreenCaptureKitDeviceMac>(source, filter);
}

}  // namespace content

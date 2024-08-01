// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#import "media/capture/video/apple/video_capture_device_avfoundation.h"
#include <optional>
#include "base/feature_list.h"
#include "base/time/time.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <stddef.h>
#include <stdint.h>
#include <optional>
#include <sstream>

#include "base/apple/foundation_util.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#include "components/crash/core/common/crash_key.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_types.h"
#include "media/capture/video/apple/video_capture_device_apple.h"
#import "media/capture/video/apple/video_capture_device_avfoundation_utils.h"
#include "media/capture/video/apple/video_capture_device_factory_apple.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_MAC)
#import "media/capture/video/mac/video_capture_metrics_mac.h"
#endif

#if BUILDFLAG(IS_IOS)
#import <UIKit/UIKit.h>
#endif

BASE_FEATURE(kAVFoundationCaptureForwardSampleTimestamps,
             "AVFoundationCaptureForwardSampleTimestamps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAVFoundationCaptureSonomaRestartStalledCamera,
             "AVFoundationCaptureSonomaRestartStalledCamera",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// Logitech 4K Pro
constexpr NSString* kModelIdLogitech4KPro =
    @"UVC Camera VendorID_1133 ProductID_2175";

constexpr gfx::ColorSpace kColorSpaceRec709Apple(
    gfx::ColorSpace::PrimaryID::BT709,
    gfx::ColorSpace::TransferID::BT709_APPLE,
    gfx::ColorSpace::MatrixID::SMPTE170M,
    gfx::ColorSpace::RangeID::LIMITED);

constexpr int kTimeToWaitBeforeStoppingPhotoOutputInSeconds = 60;
constexpr FourCharCode kDefaultFourCCPixelFormat =
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;  // NV12 (a.k.a. 420v)

// Allowable epsilon when comparing the requested framerate against the
// captures' min/max framerates, to handle float inaccuracies.
// Framerates will be in the range of 1-100 or so, meaning under- or
// overshooting by 0.001 fps will be negligible, but still handling float loss
// of precision during manipulation.
constexpr float kFrameRateEpsilon = 0.001;

std::optional<base::TimeTicks> GetCMSampleBufferTimestamp(
    CMSampleBufferRef sampleBuffer) {
  const CMTime cm_timestamp =
      CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
  if (CMTIME_IS_VALID(cm_timestamp)) {
    uint64_t mach_time = CMClockConvertHostTimeToSystemUnits(cm_timestamp);
    return base::TimeTicks::FromMachAbsoluteTime(mach_time);
  }
  return std::nullopt;
}

bool ShouldRestartStalledCamera() {
  // The stall check should not be needed on macOS 14 due to a redesign of the
  // camera capture in macOS 14. It also interferes with the Presenter's Overlay
  // feature that was introduced in macOS 14. See https://crbug.com/335210401.
  if (@available(macOS 14.0, *)) {
    return base::FeatureList::IsEnabled(
        kAVFoundationCaptureSonomaRestartStalledCamera);
  }
  return true;
}

constexpr size_t kPixelBufferPoolSize = 10;

}  // anonymous namespace

namespace media {

// Uses the most recent advice from Apple for configuring and starting.
BASE_FEATURE(kConfigureCaptureBeforeStart,
             "ConfigureCaptureBeforeStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Allow disabling optimizations (https://crbug.com/1143477,
// https://crbug.com/959962) because of flickering (https://crbug.com/1515598).
BASE_FEATURE(kOverrideCameraIOSurfaceColorSpace,
             "OverrideCameraIOSurfaceColorSpace",
             base::FEATURE_DISABLED_BY_DEFAULT);

AVCaptureDeviceFormat* FindBestCaptureFormat(
    NSArray<AVCaptureDeviceFormat*>* formats,
    int width,
    int height,
    float frame_rate) {
  AVCaptureDeviceFormat* bestCaptureFormat = nil;
  VideoPixelFormat bestPixelFormat = VideoPixelFormat::PIXEL_FORMAT_UNKNOWN;
  bool bestMatchesFrameRate = false;
  Float64 bestMaxFrameRate = 0;

  for (AVCaptureDeviceFormat* captureFormat in formats) {
    const FourCharCode fourcc =
        CMFormatDescriptionGetMediaSubType(captureFormat.formatDescription);
    VideoPixelFormat pixelFormat =
        [VideoCaptureDeviceAVFoundation FourCCToChromiumPixelFormat:fourcc];
    CMVideoDimensions dimensions =
        CMVideoFormatDescriptionGetDimensions(captureFormat.formatDescription);

    // If the pixel format is unsupported by our code, then it is not useful.
    if (pixelFormat == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
      continue;
    }

    // If our CMSampleBuffers will have a different size than the native
    // capture, then we will not be the fast path.
    if (dimensions.width != width || dimensions.height != height) {
      continue;
    }

    Float64 maxFrameRate = 0;
    bool matchesFrameRate = false;
    for (AVFrameRateRange* frameRateRange in captureFormat
             .videoSupportedFrameRateRanges) {
      maxFrameRate = std::max(maxFrameRate, frameRateRange.maxFrameRate);
      matchesFrameRate |=
          frameRateRange.minFrameRate <= frame_rate + kFrameRateEpsilon &&
          frame_rate - kFrameRateEpsilon <= frameRateRange.maxFrameRate;
    }
    // Prefer a capture format that handles the requested framerate to one
    // that doesn't.
    if (bestCaptureFormat) {
      if (bestMatchesFrameRate && !matchesFrameRate) {
        continue;
      }
      if (matchesFrameRate && !bestMatchesFrameRate) {
        bestCaptureFormat = nil;
      }
    }

    // Prefer a capture format with a lower maximum framerate, under the
    // assumption that that may have lower power consumption.
    if (bestCaptureFormat) {
      if (bestMaxFrameRate < maxFrameRate) {
        continue;
      }
      if (maxFrameRate < bestMaxFrameRate) {
        bestCaptureFormat = nil;
      }
    }

    // Finally, compare according to Chromium preference.
    if (bestCaptureFormat) {
      if (VideoCaptureFormat::ComparePixelFormatPreference(bestPixelFormat,
                                                           pixelFormat)) {
        continue;
      }
    }

    bestCaptureFormat = captureFormat;
    bestPixelFormat = pixelFormat;
    bestMaxFrameRate = maxFrameRate;
    bestMatchesFrameRate = matchesFrameRate;
  }

  VLOG(1) << "Selecting AVCaptureDevice format "
          << VideoPixelFormatToString(bestPixelFormat);
  return bestCaptureFormat;
}

}  // namespace media

@implementation VideoCaptureDeviceAVFoundation {
  // The following attributes are set via -setCaptureHeight:width:frameRate:.
  float _frameRate;

#if BUILDFLAG(IS_IOS)
  UIDeviceOrientation _orientation;
#endif
  int _rotation;

  // Usage of GPU memory buffer is controlled by
  // `--disable-video-capture-use-gpu-memory-buffer` and
  // `--video-capture-use-gpu-memory-buffer` commandline switches. This flag
  // handles whether to use a GPU memory for a video frame or not.
  bool _useGPUMemoryBuffer;

  // The capture format that best matches the above attributes.
  AVCaptureDeviceFormat* __strong _bestCaptureFormat;

  // A serial queue to deliver frames on, ensuring frames are delivered in
  // order.
  dispatch_queue_t __strong _sampleQueue;

  // Protects concurrent setting and using |frameReceiver_|. Note that the
  // GUARDED_BY decoration below does not have any effect.
  base::Lock _lock;
  // Used to avoid UAF in -captureOutput.
  base::Lock _destructionLock;
  raw_ptr<media::VideoCaptureDeviceAVFoundationFrameReceiver> _frameReceiver
      GUARDED_BY(_lock);  // weak.
  bool _capturedFirstFrame GUARDED_BY(_lock);
  bool _capturedFrameSinceLastStallCheck GUARDED_BY(_lock);
  struct SelfHolder {
    VideoCaptureDeviceAVFoundation* __weak the_self;
    base::WeakPtrFactory<SelfHolder> weak_ptr_factory{this};
  };
  SelfHolder _weakPtrHolderForStallCheck;
  // TimeTicks to subtract from all frames, to avoid leaking uptime.
  base::TimeTicks _startTimestamp;

  // Used to rate-limit crash reports for https://crbug.com/1168112.
  bool _hasDumpedForFrameSizeMismatch;

  AVCaptureSession* __strong _captureSession;

  // |captureDevice_| is an object coming from AVFoundation, used only to be
  // plugged in |captureDeviceInput_| and to query for session preset support.
  AVCaptureDevice* __strong _captureDevice;
  AVCaptureDeviceInput* __strong _captureDeviceInput;
  AVCaptureVideoDataOutput* __strong _captureVideoDataOutput;

  // When enabled, converts captured frames to NV12.
  std::unique_ptr<media::SampleBufferTransformer> _sampleBufferTransformer;

  AVCapturePhotoOutput* __strong _photoOutput;

  // Only accessed on the main thread. The takePhoto() operation is considered
  // pending until we're ready to take another photo, which involves a PostTask
  // back to the main thread after the photo was taken.
  size_t _pendingTakePhotos;
  SelfHolder _weakPtrHolderForTakePhoto;

  // For testing.
  base::RepeatingCallback<void()> _onPhotoOutputStopped;
  std::optional<bool> _isPortraitEffectSupportedForTesting;
  std::optional<bool> _isPortraitEffectActiveForTesting;

  scoped_refptr<base::SingleThreadTaskRunner> _mainThreadTaskRunner;
}

#pragma mark Class methods

+ (media::VideoPixelFormat)FourCCToChromiumPixelFormat:(FourCharCode)code {
  switch (code) {
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      return media::PIXEL_FORMAT_NV12;  // Mac fourcc: "420v".
    case kCVPixelFormatType_422YpCbCr8:
      return media::PIXEL_FORMAT_UYVY;  // Mac fourcc: "2vuy".
    case kCMPixelFormat_422YpCbCr8_yuvs:
      return media::PIXEL_FORMAT_YUY2;
    case kCMVideoCodecType_JPEG_OpenDML:
      return media::PIXEL_FORMAT_MJPEG;  // Mac fourcc: "dmb1".
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

#pragma mark Public methods

- (instancetype)initWithFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver {
  if ((self = [super init])) {
    _mainThreadTaskRunner = base::SingleThreadTaskRunner::GetCurrentDefault();
    _sampleQueue =
        dispatch_queue_create("org.chromium.VideoCaptureDeviceAVFoundation."
                              "SampleDeliveryDispatchQueue",
                              DISPATCH_QUEUE_SERIAL);
    DCHECK(frameReceiver);
    _rotation = 0;
    _useGPUMemoryBuffer = true;
    _capturedFirstFrame = false;
    _weakPtrHolderForStallCheck.the_self = self;
    _weakPtrHolderForTakePhoto.the_self = self;
    [self setFrameReceiver:frameReceiver];
    _captureSession = [[AVCaptureSession alloc] init];
    _sampleBufferTransformer = media::SampleBufferTransformer::Create();

#if BUILDFLAG(IS_IOS)
    _orientation = UIDeviceOrientationUnknown;
    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(orientationChanged:)
               name:UIDeviceOrientationDidChangeNotification
             object:[UIDevice currentDevice]];
#endif
  }
  return self;
}

#if BUILDFLAG(IS_IOS)
- (void)orientationChanged:(NSNotification*)note {
  UIDevice* device = note.object;
  UIDeviceOrientation deviceOrientation = device.orientation;
  AVCaptureConnection* captureConnection =
      [_captureVideoDataOutput connectionWithMediaType:AVMediaTypeVideo];
  if ([captureConnection isVideoOrientationSupported]) {
    _orientation = deviceOrientation;
    AVCaptureDevicePosition camera_position =
        [[_captureDeviceInput device] position];
    _rotation =
        media::MaybeGetVideoRotation(_orientation, camera_position).value_or(0);
    [self captureConfigurationChanged];
  }
}
#endif

- (void)dealloc {
  // Stopping a running photo output takes `_lock`. To avoid this happening
  // inside stopCapture() below which would deadlock, we ensure that the photo
  // output is already stopped before taking `_lock`.
  [self stopPhotoOutput];
  {
    // To avoid races with concurrent callbacks, grab the lock before stopping
    // capture and clearing all the variables.
    base::AutoLock lock(_lock);

    // Cleanup AVCaptureSession
    // 1. Stop the AVCaptureSession
    [self stopCapture];
    // 2. Remove AVCaptureInputs and AVCaptureOutputs
    for (AVCaptureInput* input in _captureSession.inputs) {
      [_captureSession removeInput:input];
    }
    for (AVCaptureOutput* output in _captureSession.outputs) {
      [_captureSession removeOutput:output];
    }
    // 3. Set the AVCaptureSession to nil to remove strong references
    _captureSession = nil;

    // Cleanup AVCaptureDevice
    // 1. Unlock any configuration (if locked)
    [_captureDevice unlockForConfiguration];
    // 2. Remove observer
    [_captureDevice removeObserver:self forKeyPath:@"portraitEffectActive"];
    // 3. Release and deallocate the capture device
    _captureDevice = nil;

    _frameReceiver = nullptr;
    _sampleBufferTransformer.reset();
    _mainThreadTaskRunner = nullptr;
    _sampleQueue = nil;
  }
  {
    // Ensures -captureOutput has finished before we continue the destruction
    // steps. If -captureOutput grabbed the destruction lock before us this
    // prevents UAF. If -captureOutput grabbed the destruction lock after us
    // it will exit early because |_frameReceiver| is already null at this
    // point.
    base::AutoLock destructionLock(_destructionLock);
  }
}

- (void)setFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver {
  base::AutoLock lock(_lock);
  _frameReceiver = frameReceiver;
}

- (void)logMessage:(const std::string&)message {
  base::AutoLock lock(_lock);
  [self logMessageLocked:message];
}

- (void)logMessageLocked:(const std::string&)message {
  auto loggedMessage = std::string("AVFoundation: ") + message;
  VLOG(1) << loggedMessage;
  if (_frameReceiver) {
    _frameReceiver->OnLog(loggedMessage);
  }
}

- (void)setUseGPUMemoryBuffer:(bool)useGPUMemoryBuffer {
  _useGPUMemoryBuffer = useGPUMemoryBuffer;
}

- (BOOL)setCaptureDevice:(NSString*)deviceId
            errorMessage:(NSString**)outMessage {
  DCHECK(_captureSession);
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());

  if (!deviceId) {
    // First stop the capture session, if it's running.
    [self stopCapture];
    // Now remove the input and output from the capture session.
    [_captureSession removeOutput:_captureVideoDataOutput];
    [self stopPhotoOutput];
    if (_captureDeviceInput) {
      DCHECK(_captureDevice);
      if (@available(macOS 12.0, *)) {
        [_captureDevice removeObserver:self forKeyPath:@"portraitEffectActive"];
      }
      [_captureSession stopRunning];
      [_captureSession removeInput:_captureDeviceInput];
      _captureDeviceInput = nil;
      _captureDevice = nil;
    }
    return YES;
  }

  // Look for input device with requested name.
  _captureDevice = [AVCaptureDevice deviceWithUniqueID:deviceId];
  if (!_captureDevice) {
    *outMessage = @"Could not open video capture device.";
    return NO;
  }

  // Create the capture input associated with the device. Easy peasy.
  NSError* error = nil;
  _captureDeviceInput =
      [AVCaptureDeviceInput deviceInputWithDevice:_captureDevice error:&error];
  if (!_captureDeviceInput) {
    _captureDevice = nil;
    *outMessage = [NSString
        stringWithFormat:@"Could not create video capture input (%@): %@",
                         error.localizedDescription,
                         error.localizedFailureReason];
    return NO;
  }
  [_captureSession addInput:_captureDeviceInput];

  // Create a new data output for video. The data output is configured to
  // discard late frames by default.
  _captureVideoDataOutput = [[AVCaptureVideoDataOutput alloc] init];
  if (!_captureVideoDataOutput) {
    [_captureSession removeInput:_captureDeviceInput];
    *outMessage = @"Could not create video data output.";
    return NO;
  }
  _captureVideoDataOutput.alwaysDiscardsLateVideoFrames = true;

  [_captureVideoDataOutput setSampleBufferDelegate:self queue:_sampleQueue];
  [_captureSession addOutput:_captureVideoDataOutput];

  if (@available(macOS 12.0, *)) {
    [_captureDevice addObserver:self
                     forKeyPath:@"portraitEffectActive"
                        options:0
                        context:(__bridge void*)_captureDevice];
  }

#if BUILDFLAG(IS_IOS)
  _orientation = [[UIDevice currentDevice] orientation];
  if (_orientation == UIDeviceOrientationUnknown) {
    _orientation = UIDeviceOrientationPortrait;
  }

  AVCaptureDevicePosition camera_position =
      [[_captureDeviceInput device] position];
  _rotation =
      media::MaybeGetVideoRotation(_orientation, camera_position).value_or(0);
#endif

  return YES;
}

- (BOOL)setCaptureHeight:(int)height
                   width:(int)width
               frameRate:(float)frameRate {
  DCHECK(![_captureSession isRunning]);
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());

  _frameRate = frameRate;
  _bestCaptureFormat = media::FindBestCaptureFormat(_captureDevice.formats,
                                                    width, height, frameRate);
  FourCharCode best_fourcc = kDefaultFourCCPixelFormat;
  if (_bestCaptureFormat) {
    best_fourcc = CMFormatDescriptionGetMediaSubType(
        _bestCaptureFormat.formatDescription);
  }

  if (best_fourcc == kCMVideoCodecType_JPEG_OpenDML) {
    // Capturing MJPEG for the following camera does not work (frames not
    // forwarded). macOS can convert to the default pixel format for us instead.
    // TODO(crbug.com/40147585): figure out if there's another workaround.
    if ([_captureDevice.modelID isEqualToString:kModelIdLogitech4KPro]) {
      LOG(WARNING) << "Activating MJPEG workaround for camera "
                   << base::SysNSStringToUTF8(kModelIdLogitech4KPro);
      best_fourcc = kDefaultFourCCPixelFormat;
    }
  }

  VLOG(2) << __func__ << ": configuring '"
          << media::MacFourCCToString(best_fourcc) << "' " << width << "x"
          << height << "@" << frameRate;

  // The capture output has to be configured, despite Mac documentation
  // detailing that setting the sessionPreset would be enough. The reason for
  // this mismatch is probably because most of the AVFoundation docs are written
  // for iOS and not for macOS. AVVideoScalingModeKey() refers to letterboxing
  // yes/no and preserve aspect ratio yes/no when scaling. Currently we set
  // cropping and preservation.
  NSDictionary* videoSettingsDictionary = @{
#if BUILDFLAG(IS_MAC)
    (id)kCVPixelBufferWidthKey : @(width),
    (id)kCVPixelBufferHeightKey : @(height),
    AVVideoScalingModeKey : AVVideoScalingModeResizeAspectFill,
#endif
    (id)kCVPixelBufferPixelFormatTypeKey : @(best_fourcc)
  };

  _captureVideoDataOutput.videoSettings = videoSettingsDictionary;

#if (!defined(__IPHONE_7_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_7_0)
  AVCaptureConnection* captureConnection =
      [_captureVideoDataOutput connectionWithMediaType:AVMediaTypeVideo];
  // CMTimeMake accepts integer arguments but |frameRate| is float, so round it.
  if (captureConnection.supportsVideoMinFrameDuration) {
    captureConnection.videoMinFrameDuration =
        CMTimeMake(media::kFrameRatePrecision,
                   (int)(frameRate * media::kFrameRatePrecision));
  }
  if (captureConnection.supportsVideoMaxFrameDuration) {
    captureConnection.videoMaxFrameDuration =
        CMTimeMake(media::kFrameRatePrecision,
                   (int)(frameRate * media::kFrameRatePrecision));
  }
#endif
  return YES;
}

- (BOOL)startCapture {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  if (!_captureSession) {
    [self logMessage:"Video capture session not initialized."];
    return NO;
  }
  // Connect the notifications.
  NSNotificationCenter* nc = NSNotificationCenter.defaultCenter;
  [nc addObserver:self
         selector:@selector(onVideoError:)
             name:AVCaptureSessionRuntimeErrorNotification
           object:_captureSession];

  if (base::FeatureList::IsEnabled(media::kConfigureCaptureBeforeStart)) {
    if (_bestCaptureFormat) {
      [_captureSession beginConfiguration];
      if ([_captureDevice lockForConfiguration:nil]) {
        [_captureDevice setActiveFormat:_bestCaptureFormat];
        [_captureDevice unlockForConfiguration];
      }
      [_captureSession commitConfiguration];
    }

    [_captureSession startRunning];
  } else {
    [_captureSession startRunning];
    if (_bestCaptureFormat && [_captureDevice lockForConfiguration:nil]) {
      [_captureDevice setActiveFormat:_bestCaptureFormat];
      [_captureDevice unlockForConfiguration];
    }
  }

  {
    base::AutoLock lock(_lock);
    _capturedFirstFrame = false;
    _capturedFrameSinceLastStallCheck = NO;
  }
  [self doStallCheck:0];
  return YES;
}

- (void)stopCapture {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  _weakPtrHolderForStallCheck.weak_ptr_factory.InvalidateWeakPtrs();
  [self stopPhotoOutput];
  if (_captureSession.running) {
    [_captureSession stopRunning];  // Synchronous.
  }
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)takePhoto {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  DCHECK(_captureSession.running);

  ++_pendingTakePhotos;
  if (_pendingTakePhotos > 1u) {
    // There is already pending takePhoto(). When it finishes it will kick off
    // the next takePhotoInternal(), so there is nothing more to do here.
    return;
  }
  // `_pendingTakePhotos` just went from 0 to 1. In case the 60 second delayed
  // task to perform stopPhotoOutput() is in-flight, invalidate weak ptrs to
  // cancel any such operation.
  _weakPtrHolderForTakePhoto.weak_ptr_factory.InvalidateWeakPtrs();

  // Ready to take a photo immediately?
  // Thread-safe because `_photoOutput` is only modified on the main thread.
  if (_photoOutput) {
    [self takePhotoInternal];
    return;
  }

  // Lazily instantiate `_photoOutput` so that if the app never calls
  // takePhoto() we don't have to pay the associated performance cost, see
  // https://crbug.com/1116241. This procedure is purposefully delayed by 3
  // seconds because the camera needs to ramp up after re-configuring itself in
  // order for 3A to stabilize or else the photo is dark/black.
  {
    // `_lock` is needed since `_photoOutput` may be read from non-main thread.
    base::AutoLock lock(_lock);
    _photoOutput = [[AVCapturePhotoOutput alloc] init];
  }
  if (![_captureSession canAddOutput:_photoOutput]) {
    {
      base::AutoLock lock(_lock);
      if (_frameReceiver) {
        _frameReceiver->OnPhotoError();
      }
    }
    [self takePhotoResolved];
    return;
  } else {
    [_captureSession addOutput:_photoOutput];
  }
  // A delay is needed before taking the photo or else the photo may be dark.
  // 2 seconds was enough in manual testing; we delay by 3 for good measure.
  _mainThreadTaskRunner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SelfHolder> weakSelf) {
            if (!weakSelf.get()) {
              return;
            }
            [weakSelf.get()->the_self takePhotoInternal];
          },
          _weakPtrHolderForTakePhoto.weak_ptr_factory.GetWeakPtr()),
      base::Seconds(3));
}

- (void)setOnPhotoOutputStoppedForTesting:
    (base::RepeatingCallback<void()>)onPhotoOutputStopped {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  _onPhotoOutputStopped = onPhotoOutputStopped;
}

#pragma mark Private methods

- (void)takePhotoInternal {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  DCHECK(_captureSession.running);
  // takePhotoInternal() can only happen when we have a `_photoOutput` because
  // stopPhotoOutput() cancels in-flight operations by invalidating weak ptrs.
  DCHECK(_photoOutput);
  @try {
    // Asynchronous success or failure is handled inside
    // captureOutput:didFinishProcessingPhoto:error on an unknown thread.
    // Synchronous failures are handled in the catch clause below.
    [_photoOutput
        capturePhotoWithSettings:[AVCapturePhotoSettings
                                     photoSettingsWithFormat:@{
                                       AVVideoCodecKey : AVVideoCodecTypeJPEG
                                     }]
                        delegate:self];
  } @catch (id exception) {
    {
      base::AutoLock lock(_lock);
      if (_frameReceiver) {
        _frameReceiver->OnPhotoError();
      }
    }
    [self takePhotoResolved];
  }
}

// Callback for the `_photoOutput` operation started in takePhotoInternal().
- (void)captureOutput:(id)output        // AVCapturePhotoOutput*
    didFinishProcessingPhoto:(id)photo  // AVCapturePhoto*
                       error:(NSError*)error {
  base::AutoLock lock(_lock);
  // If `output` is no longer current, ignore the result of this operation.
  // `_frameReceiver->OnPhotoError()` will already have been called inside
  // stopPhotoOutput().
  if (output != _photoOutput) {
    return;
  }
  if (_frameReceiver) {
    // Always non-nil according to Apple's documentation.
    DCHECK(photo);
    NSData* data = static_cast<AVCapturePhoto*>(photo).fileDataRepresentation;
    if (!error && data) {
      _frameReceiver->OnPhotoTaken(reinterpret_cast<const uint8_t*>(data.bytes),
                                   data.length, "image/jpeg");
    } else {
      _frameReceiver->OnPhotoError();
    }
  }
  // Whether we succeeded or failed, we need to resolve the pending
  // takePhoto() operation.
  _mainThreadTaskRunner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<SelfHolder> weakSelf) {
                       if (!weakSelf.get()) {
                         return;
                       }
                       [weakSelf.get()->the_self takePhotoResolved];
                     },
                     _weakPtrHolderForTakePhoto.weak_ptr_factory.GetWeakPtr()));
}

- (void)takePhotoResolved {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  --_pendingTakePhotos;
  if (_pendingTakePhotos > 0u) {
    // Take another photo.
    [self takePhotoInternal];
    return;
  }
  // All pending takePhoto()s have completed. If no more photos are taken
  // within 60 seconds, stop photo output to avoid expensive MJPEG conversions
  // going forward.
  _mainThreadTaskRunner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<SelfHolder> weakSelf) {
            if (!weakSelf.get()) {
              return;
            }
            [weakSelf.get()->the_self stopPhotoOutput];
          },
          _weakPtrHolderForTakePhoto.weak_ptr_factory.GetWeakPtr()),
      base::Seconds(kTimeToWaitBeforeStoppingPhotoOutputInSeconds));
}

- (void)stopPhotoOutput {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  // Already stopped?
  // Thread-safe because `_photoOutput` is only modified on the main thread.
  if (!_photoOutput) {
    return;
  }
  // Cancel all in-flight operations.
  _weakPtrHolderForTakePhoto.weak_ptr_factory.InvalidateWeakPtrs();
  {
    base::AutoLock lock(_lock);
    if (_captureSession) {
      [_captureSession removeOutput:_photoOutput];
    }
    // `_lock` is needed since `_photoOutput` may be read from non-main thread.
    _photoOutput = nil;
    // For every pending photo, report OnPhotoError().
    if (_pendingTakePhotos) {
      if (_frameReceiver) {
        for (size_t i = 0; i < _pendingTakePhotos; ++i) {
          _frameReceiver->OnPhotoError();
        }
      }
      _pendingTakePhotos = 0u;
    }
  }

  if (_onPhotoOutputStopped) {
    // Callback used by tests.
    _onPhotoOutputStopped.Run();
  }
}

- (void)processSample:(CMSampleBufferRef)sampleBuffer
         captureFormat:(const media::VideoCaptureFormat&)captureFormat
            colorSpace:(const gfx::ColorSpace&)colorSpace
             timestamp:(const base::TimeDelta)timestamp
    capture_begin_time:(std::optional<base::TimeTicks>)capture_begin_time {
  VLOG(3) << __func__;
  // Trust |_frameReceiver| to do decompression.
  char* baseAddress = nullptr;
  size_t frameSize = 0;
  _lock.AssertAcquired();
  DCHECK(_frameReceiver);
  const bool sample_buffer_addressable = media::ExtractBaseAddressAndLength(
      &baseAddress, &frameSize, sampleBuffer);
  DCHECK(sample_buffer_addressable);
  if (sample_buffer_addressable) {
    const bool safe_to_forward =
        captureFormat.pixel_format == media::PIXEL_FORMAT_MJPEG ||
        media::VideoFrame::AllocationSize(
            captureFormat.pixel_format, captureFormat.frame_size) <= frameSize;
    DCHECK(safe_to_forward);
    if (safe_to_forward) {
      _frameReceiver->ReceiveFrame(
          reinterpret_cast<const uint8_t*>(baseAddress), frameSize,
          captureFormat, colorSpace, 0, 0, timestamp, capture_begin_time,
          _rotation);
    }
  }
}

- (BOOL)processPixelBufferPlanes:(CVImageBufferRef)pixelBuffer
                   captureFormat:(const media::VideoCaptureFormat&)captureFormat
                      colorSpace:(const gfx::ColorSpace&)colorSpace
                       timestamp:(const base::TimeDelta)timestamp
              capture_begin_time:
                  (std::optional<base::TimeTicks>)capture_begin_time {
  VLOG(3) << __func__;
  if (CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly) !=
      kCVReturnSuccess) {
    return NO;
  }

  // Retrieve the layout of the planes of |pixelBuffer|.
  const size_t numPlanes =
      media::VideoFrame::NumPlanes(captureFormat.pixel_format);
  std::vector<uint8_t*> pixelBufferAddresses;
  std::vector<size_t> pixelBufferBytesPerRows;
  std::vector<size_t> pixelBufferHeights;
  if (!CVPixelBufferIsPlanar(pixelBuffer)) {
    // For nonplanar buffers, CVPixelBufferGetBaseAddress returns a pointer
    // to (0,0). (For planar buffers, it returns something else.)
    // https://developer.apple.com/documentation/corevideo/1457115-cvpixelbuffergetbaseaddress?language=objc
    CHECK_EQ(numPlanes, 1u);
    pixelBufferAddresses.push_back(
        static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer)));
    pixelBufferBytesPerRows.push_back(CVPixelBufferGetBytesPerRow(pixelBuffer));
    pixelBufferHeights.push_back(CVPixelBufferGetHeight(pixelBuffer));
  } else {
    // For planar buffers, CVPixelBufferGetBaseAddressOfPlane() is used. If
    // the buffer is contiguous (CHECK'd below) then we only need to know
    // the address of the first plane, regardless of
    // CVPixelBufferGetPlaneCount().
    CHECK_EQ(numPlanes, CVPixelBufferGetPlaneCount(pixelBuffer));
    for (size_t plane = 0; plane < numPlanes; ++plane) {
      pixelBufferAddresses.push_back(static_cast<uint8_t*>(
          CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, plane)));
      pixelBufferBytesPerRows.push_back(
          CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, plane));
      pixelBufferHeights.push_back(
          CVPixelBufferGetHeightOfPlane(pixelBuffer, plane));
    }
  }
  // CVPixelBufferGetDataSize() works for both nonplanar and planar buffers
  // as long as they are contiguous in memory. If it is not contiguous, 0 is
  // returned.
  size_t frameSize = CVPixelBufferGetDataSize(pixelBuffer);
  // Only contiguous buffers are supported.
  CHECK(frameSize);

  // Compute the tightly-packed layout for |captureFormat|.
  size_t packedBufferSize = 0;
  std::vector<size_t> packedBytesPerRows;
  std::vector<size_t> packedHeights;
  for (size_t plane = 0; plane < numPlanes; ++plane) {
    size_t bytesPerRow = media::VideoFrame::RowBytes(
        plane, captureFormat.pixel_format, captureFormat.frame_size.width());
    size_t height =
        media::VideoFrame::PlaneSize(captureFormat.pixel_format, plane,
                                     captureFormat.frame_size)
            .height();
    packedBytesPerRows.push_back(bytesPerRow);
    packedHeights.push_back(height);
    packedBufferSize += bytesPerRow * height;
  }

  // If media::VideoFrame::PlaneSize differs from the CVPixelBuffer's size then
  // generate a crash report to show the difference.
  // https://crbug.com/1168112
  CHECK_EQ(pixelBufferHeights.size(), packedHeights.size());
  for (size_t plane = 0; plane < pixelBufferHeights.size(); ++plane) {
    if (pixelBufferHeights[plane] != packedHeights[plane] &&
        !_hasDumpedForFrameSizeMismatch) {
      static crash_reporter::CrashKeyString<64> planeInfoKey(
          "core-video-plane-info");
      planeInfoKey.Set(
          base::StringPrintf("plane:%zu cv_height:%zu packed_height:%zu", plane,
                             pixelBufferHeights[plane], packedHeights[plane]));
      base::debug::DumpWithoutCrashing();
      _hasDumpedForFrameSizeMismatch = true;
    }
  }

  // If |pixelBuffer| is not tightly packed, then copy it to |packedBufferCopy|,
  // because ReceiveFrame() below assumes tight packing.
  // https://crbug.com/1151936
  bool needsCopyToPackedBuffer = pixelBufferBytesPerRows != packedBytesPerRows;
  std::vector<uint8_t> packedBufferCopy;
  if (needsCopyToPackedBuffer) {
    packedBufferCopy.resize(packedBufferSize, 0);
    uint8_t* dstAddr = packedBufferCopy.data();
    for (size_t plane = 0; plane < numPlanes; ++plane) {
      uint8_t* srcAddr = pixelBufferAddresses[plane];
      size_t row = 0;
      for (row = 0;
           row < std::min(packedHeights[plane], pixelBufferHeights[plane]);
           ++row) {
        memcpy(dstAddr, srcAddr,
               std::min(packedBytesPerRows[plane],
                        pixelBufferBytesPerRows[plane]));
        dstAddr += packedBytesPerRows[plane];
        srcAddr += pixelBufferBytesPerRows[plane];
      }
    }
  }

  _lock.AssertAcquired();
  DCHECK(_frameReceiver);
  _frameReceiver->ReceiveFrame(packedBufferCopy.empty()
                                   ? pixelBufferAddresses[0]
                                   : packedBufferCopy.data(),
                               frameSize, captureFormat, colorSpace, 0, 0,
                               timestamp, capture_begin_time, _rotation);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
  return YES;
}

- (void)processPixelBufferNV12IOSurface:(CVPixelBufferRef)pixelBuffer
                          captureFormat:
                              (const media::VideoCaptureFormat&)captureFormat
                             colorSpace:(const gfx::ColorSpace&)colorSpace
                              timestamp:(const base::TimeDelta)timestamp
                     capture_begin_time:
                         (std::optional<base::TimeTicks>)capture_begin_time {
  VLOG(3) << __func__;
  DCHECK_EQ(captureFormat.pixel_format, media::PIXEL_FORMAT_NV12);

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  DCHECK(ioSurface);
  media::CapturedExternalVideoBuffer externalBuffer =
      [self capturedExternalVideoBufferFromNV12IOSurface:ioSurface
                                           captureFormat:captureFormat
                                              colorSpace:colorSpace];

  // The lock is needed for |_frameReceiver|.
  _lock.AssertAcquired();
  DCHECK(_frameReceiver);
  _frameReceiver->ReceiveExternalGpuMemoryBufferFrame(
      std::move(externalBuffer), timestamp, capture_begin_time);
}

- (media::CapturedExternalVideoBuffer)
    capturedExternalVideoBufferFromNV12IOSurface:(IOSurfaceRef)ioSurface
                                   captureFormat:
                                       (const media::VideoCaptureFormat&)
                                           captureFormat
                                      colorSpace:
                                          (const gfx::ColorSpace&)colorSpace {
  DCHECK(ioSurface);
  gfx::GpuMemoryBufferHandle handle;
  handle.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  handle.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
  handle.io_surface.reset(ioSurface, base::scoped_policy::RETAIN);

  // The BT709_APPLE color space is stored as an ICC profile, which is parsed
  // every frame in the GPU process. For this particularly common case, go back
  // to ignoring the color profile, because doing so avoids doing an ICC profile
  // parse.
  // https://crbug.com/1143477 (CPU usage parsing ICC profile)
  // https://crbug.com/959962 (ignoring color space)
  gfx::ColorSpace overriddenColorSpace = colorSpace;
  if (colorSpace == kColorSpaceRec709Apple &&
      base::FeatureList::IsEnabled(media::kOverrideCameraIOSurfaceColorSpace)) {
    overriddenColorSpace = gfx::ColorSpace(
        gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::SRGB,
        gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
    IOSurfaceSetValue(ioSurface, CFSTR("IOSurfaceColorSpace"),
                      kCGColorSpaceSRGB);
  }

  return media::CapturedExternalVideoBuffer(std::move(handle), captureFormat,
                                            overriddenColorSpace);
}

// Sometimes (especially when the camera is accessed by another process, e.g,
// Photo Booth), the AVCaptureSession will stop producing new frames. This check
// happens with no errors or notifications being produced. To recover from this,
// check to see if a new frame has been captured second. If 5 of these checks
// fail consecutively, restart the capture session.
// https://crbug.com/1176568
- (void)doStallCheck:(int)failedCheckCount {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());

  int nextFailedCheckCount = failedCheckCount + 1;
  {
    base::AutoLock lock(_lock);
    // This is to detect a capture was working, but stopped submitting new
    // frames. If we haven't received any frames yet, don't do anything.
    if (!_capturedFirstFrame) {
      nextFailedCheckCount = 0;
    }

    // If we captured a frame since last check, then we aren't stalled.
    // We're also not considered stalled if takePhoto() is pending, avoiding
    // excessive capture restarts in unit tests with mock time.
    if (_capturedFrameSinceLastStallCheck || _pendingTakePhotos) {
      nextFailedCheckCount = 0;
    }
    _capturedFrameSinceLastStallCheck = NO;
  }

  constexpr int kMaxFailedCheckCount = 5;
  if (nextFailedCheckCount < kMaxFailedCheckCount) {
    // Post a task to check for progress in 1 second. Create the weak factory
    // for the posted task, if needed.
    constexpr base::TimeDelta kStallCheckInterval = base::Seconds(1);
    auto callbackLambda = [](base::WeakPtr<SelfHolder> weakSelf,
                             int failedCheckCount) {
      if (!weakSelf.get()) {
        return;
      }
      [weakSelf.get()->the_self doStallCheck:failedCheckCount];
    };
    _mainThreadTaskRunner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            callbackLambda,
            _weakPtrHolderForStallCheck.weak_ptr_factory.GetWeakPtr(),
            nextFailedCheckCount),
        kStallCheckInterval);
  } else {
    if (ShouldRestartStalledCamera()) {
      [self logMessage:"Capture appears to have stalled, restarting."];
      [self stopCapture];
      [self startCapture];
    } else {
      [self logMessage:
                "Capture appears to have stalled, restarting may have helped "
                "but is disabled. See https://issues.chromium.org/335210401."];
    }
  }
}

// |captureOutput| is called by the capture device to deliver a new frame.
// Since the callback is configured to happen on a global dispatch queue, calls
// may enter here concurrently and on any thread.
- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
  VLOG(3) << __func__;

  // Concurrent calls into |_frameReceiver| are not supported, so take |_lock|
  // before any of the subsequent paths. The |_destructionLock| must be grabbed
  // first to avoid races with -dealloc.
  base::AutoLock destructionLock(_destructionLock);
  base::AutoLock lock(_lock);
  _capturedFrameSinceLastStallCheck = YES;
  if (!_frameReceiver || !_sampleBufferTransformer) {
    VLOG(1) << "dropping frame due to no receiver";
    return;
  }
  auto capture_begin_time = GetCMSampleBufferTimestamp(sampleBuffer);
  const base::TimeTicks pres_timestamp =
      capture_begin_time.value_or(base::TimeTicks());
  if (_startTimestamp.is_null()) {
    _startTimestamp = pres_timestamp;
  }
  const base::TimeDelta timestamp = pres_timestamp - _startTimestamp;
#if BUILDFLAG(IS_MAC)
  bool logUma = !std::exchange(_capturedFirstFrame, true);
  if (logUma) {
    [self logMessageLocked:"First frame received for this capturer instance"];
    media::LogFirstCapturedVideoFrame(_bestCaptureFormat, sampleBuffer);
  }
#endif
  // Forget the sample timestamp if we're out of the experiment.
  if (!base::FeatureList::IsEnabled(
          kAVFoundationCaptureForwardSampleTimestamps)) {
    capture_begin_time = std::nullopt;
  }

  // The SampleBufferTransformer CHECK-crashes if the sample buffer is not MJPEG
  // and does not have a pixel buffer (https://crbug.com/1160647) so we fall
  // back on the M87 code path if this is the case.
  // TODO(crbug.com/40162135): When the SampleBufferTransformer is
  // patched to support non-MJPEG-and-non-pixel-buffer sample buffers, remove
  // this workaround and the fallback other code path.
  bool sampleHasPixelBufferOrIsMjpeg =
      CMSampleBufferGetImageBuffer(sampleBuffer) ||
      CMFormatDescriptionGetMediaSubType(CMSampleBufferGetFormatDescription(
          sampleBuffer)) == kCMVideoCodecType_JPEG_OpenDML;

  // If the SampleBufferTransformer is enabled, convert all possible capture
  // formats to an IOSurface-backed NV12 pixel buffer.
  // TODO(crbug.com/40747183): Refactor to not hijack the code paths
  // below the transformer code.
  if (_useGPUMemoryBuffer && sampleHasPixelBufferOrIsMjpeg) {
    _sampleBufferTransformer->Reconfigure(
        media::SampleBufferTransformer::GetBestTransformerForNv12Output(
            sampleBuffer),
        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
        media::GetSampleBufferSize(sampleBuffer), _rotation,
        kPixelBufferPoolSize);
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> pixelBuffer =
        _sampleBufferTransformer->Transform(sampleBuffer);
    if (!pixelBuffer) {
      [self logMessageLocked:
                "Failed to transform captured frame. Dropping frame."];
      return;
    }

#if BUILDFLAG(IS_MAC)
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> final_pixel_buffer =
        pixelBuffer;
#else
    // The rotated_pixelBuffer might not be the same size as the source
    // pixelBuffer as it gets rotated by rotation_angle_. In order to restore
    // the original size, rotated_pixelBuffer need to scale it to its original
    // size by transforming it.
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> rotated_pixelBuffer =
        _sampleBufferTransformer->Rotate(pixelBuffer.get());
    base::apple::ScopedCFTypeRef<CVPixelBufferRef> final_pixel_buffer =
        _sampleBufferTransformer->Transform(rotated_pixelBuffer.get());

#endif

    const media::VideoCaptureFormat captureFormat(
        gfx::Size(CVPixelBufferGetWidth(final_pixel_buffer.get()),
                  CVPixelBufferGetHeight(final_pixel_buffer.get())),
        _frameRate, media::PIXEL_FORMAT_NV12);
    // When the |pixelBuffer| is the result of a conversion (not camera
    // pass-through) then it originates from a CVPixelBufferPool and the color
    // space is not recognized by media::GetImageBufferColorSpace(). This
    // results in log spam and a default color space format is returned. To
    // avoid this, we pretend the color space is kColorSpaceRec709Apple which
    // triggers a path that avoids color space parsing inside of
    // processPixelBufferNV12IOSurface.
    // TODO(hbos): Investigate how to successfully parse and/or configure the
    // color space correctly. The implications of this hack is not fully
    // understood.
    [self processPixelBufferNV12IOSurface:final_pixel_buffer.get()
                            captureFormat:captureFormat
                               colorSpace:kColorSpaceRec709Apple
                                timestamp:timestamp
                       capture_begin_time:capture_begin_time];
    return;
  }

  // We have certain format expectation for capture output:
  // For MJPEG, |sampleBuffer| is expected to always be a CVBlockBuffer.
  // For other formats, |sampleBuffer| may be either CVBlockBuffer or
  // CVImageBuffer. CVBlockBuffer seems to be used in the context of CoreMedia
  // plugins/virtual cameras. In order to find out whether it is CVBlockBuffer
  // or CVImageBuffer we call CMSampleBufferGetImageBuffer() and check if the
  // return value is nil.
  const CMFormatDescriptionRef formatDescription =
      CMSampleBufferGetFormatDescription(sampleBuffer);
  const CMVideoDimensions dimensions =
      CMVideoFormatDescriptionGetDimensions(formatDescription);
  OSType sampleBufferPixelFormat =
      CMFormatDescriptionGetMediaSubType(formatDescription);
  media::VideoPixelFormat videoPixelFormat = [VideoCaptureDeviceAVFoundation
      FourCCToChromiumPixelFormat:sampleBufferPixelFormat];

  const media::VideoCaptureFormat captureFormat(
      gfx::Size(dimensions.width, dimensions.height), _frameRate,
      videoPixelFormat);

  if (CVPixelBufferRef pixelBuffer =
          CMSampleBufferGetImageBuffer(sampleBuffer)) {
    const gfx::ColorSpace colorSpace =
        media::GetImageBufferColorSpace(pixelBuffer);
    OSType pixelBufferPixelFormat =
        CVPixelBufferGetPixelFormatType(pixelBuffer);
    DCHECK_EQ(pixelBufferPixelFormat, sampleBufferPixelFormat);

    // First preference is to use an NV12 IOSurface as a GpuMemoryBuffer.
    if (_useGPUMemoryBuffer) {
      if (CVPixelBufferGetIOSurface(pixelBuffer) &&
          videoPixelFormat == media::PIXEL_FORMAT_NV12) {
        [self processPixelBufferNV12IOSurface:pixelBuffer
                                captureFormat:captureFormat
                                   colorSpace:colorSpace
                                    timestamp:timestamp
                           capture_begin_time:capture_begin_time];
        return;
      }
    }
    // Second preference is to read the CVPixelBuffer's planes.
    if ([self processPixelBufferPlanes:pixelBuffer
                         captureFormat:captureFormat
                            colorSpace:colorSpace
                             timestamp:timestamp
                    capture_begin_time:capture_begin_time]) {
      return;
    }
  }

  // Last preference is to read the CMSampleBuffer.
  gfx::ColorSpace colorSpace =
      media::GetFormatDescriptionColorSpace(formatDescription);
  [self processSample:sampleBuffer
           captureFormat:captureFormat
              colorSpace:colorSpace
               timestamp:timestamp
      capture_begin_time:capture_begin_time];
}

- (void)setIsPortraitEffectSupportedForTesting:
    (bool)isPortraitEffectSupportedForTesting {
  _isPortraitEffectSupportedForTesting = isPortraitEffectSupportedForTesting;
}

- (bool)isPortraitEffectSupported {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  if (_isPortraitEffectSupportedForTesting.has_value()) {
    return _isPortraitEffectSupportedForTesting.value();
  }
  if (@available(macOS 12.0, *)) {
    return _captureDevice.activeFormat.portraitEffectSupported;
  }
  return false;
}

- (void)setIsPortraitEffectActiveForTesting:
    (bool)isPortraitEffectActiveForTesting {
  if (_isPortraitEffectActiveForTesting.has_value() &&
      _isPortraitEffectActiveForTesting == isPortraitEffectActiveForTesting) {
    return;
  }
  _isPortraitEffectActiveForTesting = isPortraitEffectActiveForTesting;
  [self captureConfigurationChanged];
}

- (bool)isPortraitEffectActive {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  if (_isPortraitEffectActiveForTesting.has_value()) {
    return _isPortraitEffectActiveForTesting.value();
  }
  if (@available(macOS 12.0, *)) {
    return _captureDevice.portraitEffectActive;
  }
  return false;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if (@available(macOS 12.0, *)) {
    if ([keyPath isEqual:@"portraitEffectActive"]) {
      [self captureConfigurationChanged];
    }
  }
}

- (void)captureConfigurationChanged {
  base::AutoLock lock(_lock);
  if (_frameReceiver) {
    _frameReceiver->ReceiveCaptureConfigurationChanged();
  }
}

- (void)onVideoError:(NSNotification*)errorNotification {
  NSError* error = base::apple::ObjCCast<NSError>(
      errorNotification.userInfo[AVCaptureSessionErrorKey]);
  [self
      sendErrorString:[NSString stringWithFormat:@"%@: %@",
                                                 error.localizedDescription,
                                                 error.localizedFailureReason]];
}

- (void)sendErrorString:(NSString*)error {
  auto message = base::SysNSStringToUTF8(error);
  VLOG(1) << __func__ << " message " << message;
  base::AutoLock lock(_lock);
  if (_frameReceiver) {
    _frameReceiver->ReceiveError(
        media::VideoCaptureError::
            kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification,
        FROM_HERE, message);
  }
}

- (void)callLocked:(base::OnceClosure)lambda {
  base::AutoLock lock(_lock);
  std::move(lambda).Run();
}

@end

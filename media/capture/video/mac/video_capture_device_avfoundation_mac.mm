// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <stddef.h>
#include <stdint.h>
#include <sstream>

#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/crash/core/common/crash_key.h"
#include "media/base/mac/color_space_util_mac.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_types.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#import "media/capture/video/mac/video_capture_metrics_mac.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Logitech 4K Pro
constexpr NSString* kModelIdLogitech4KPro =
    @"UVC Camera VendorID_1133 ProductID_2175";

constexpr gfx::ColorSpace kColorSpaceRec709Apple(
    gfx::ColorSpace::PrimaryID::BT709,
    gfx::ColorSpace::TransferID::BT709_APPLE,
    gfx::ColorSpace::MatrixID::SMPTE170M,
    gfx::ColorSpace::RangeID::LIMITED);

constexpr int kTimeToWaitBeforeStoppingStillImageCaptureInSeconds = 60;
constexpr FourCharCode kDefaultFourCCPixelFormat =
    kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;  // NV12 (a.k.a. 420v)

// Allowable epsilon when comparing the requested framerate against the
// captures' min/max framerates, to handle float inaccuracies.
// Framerates will be in the range of 1-100 or so, meaning under- or
// overshooting by 0.001 fps will be negligable, but still handling float loss
// of precision during manipulation.
constexpr float kFrameRateEpsilon = 0.001;

base::TimeDelta GetCMSampleBufferTimestamp(CMSampleBufferRef sampleBuffer) {
  const CMTime cm_timestamp =
      CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
  const base::TimeDelta timestamp =
      CMTIME_IS_VALID(cm_timestamp)
          ? base::Seconds(CMTimeGetSeconds(cm_timestamp))
          : media::kNoTimestamp;
  return timestamp;
}

constexpr size_t kPixelBufferPoolSize = 10;

}  // anonymous namespace

namespace media {

BASE_FEATURE(kInCapturerScaling,
             "InCapturerScaling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Uses the most recent advice from Apple for configuring and starting.
BASE_FEATURE(kConfigureCaptureBeforeStart,
             "ConfigureCaptureBeforeStart",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
        CMFormatDescriptionGetMediaSubType([captureFormat formatDescription]);
    VideoPixelFormat pixelFormat =
        [VideoCaptureDeviceAVFoundation FourCCToChromiumPixelFormat:fourcc];
    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(
        [captureFormat formatDescription]);
    Float64 maxFrameRate = 0;
    bool matchesFrameRate = false;
    for (AVFrameRateRange* frameRateRange in
         [captureFormat videoSupportedFrameRateRanges]) {
      maxFrameRate = std::max(maxFrameRate, [frameRateRange maxFrameRate]);
      matchesFrameRate |=
          [frameRateRange minFrameRate] <= frame_rate + kFrameRateEpsilon &&
          frame_rate - kFrameRateEpsilon <= [frameRateRange maxFrameRate];
    }

    // If the pixel format is unsupported by our code, then it is not useful.
    if (pixelFormat == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN)
      continue;

    // If our CMSampleBuffers will have a different size than the native
    // capture, then we will not be the fast path.
    if (dimensions.width != width || dimensions.height != height)
      continue;

    // Prefer a capture format that handles the requested framerate to one
    // that doesn't.
    if (bestCaptureFormat) {
      if (bestMatchesFrameRate && !matchesFrameRate)
        continue;
      if (matchesFrameRate && !bestMatchesFrameRate)
        bestCaptureFormat = nil;
    }

    // Prefer a capture format with a lower maximum framerate, under the
    // assumption that that may have lower power consumption.
    if (bestCaptureFormat) {
      if (bestMaxFrameRate < maxFrameRate)
        continue;
      if (maxFrameRate < bestMaxFrameRate)
        bestCaptureFormat = nil;
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

@implementation VideoCaptureDeviceAVFoundation

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
    _sampleQueue.reset(
        dispatch_queue_create("org.chromium.VideoCaptureDeviceAVFoundation."
                              "SampleDeliveryDispatchQueue",
                              DISPATCH_QUEUE_SERIAL),
        base::scoped_policy::ASSUME);
    DCHECK(frameReceiver);
    _capturedFirstFrame = false;
    _weakPtrFactoryForTakePhoto =
        std::make_unique<base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>(
            self);
    [self setFrameReceiver:frameReceiver];
    _captureSession.reset([[AVCaptureSession alloc] init]);
    _sampleBufferTransformer = media::SampleBufferTransformer::Create();
  }
  return self;
}

- (void)dealloc {
  {
    // To avoid races with concurrent callbacks, grab the lock before stopping
    // capture and clearing all the variables.
    base::AutoLock lock(_lock);
    [self stopStillImageOutput];
    [self stopCapture];
    _frameReceiver = nullptr;
    _sampleBufferTransformer.reset();
    _weakPtrFactoryForTakePhoto = nullptr;
    _mainThreadTaskRunner = nullptr;
    _sampleQueue.reset();
  }
  {
    // Ensures -captureOutput has finished before we continue the destruction
    // steps. If -captureOutput grabbed the destruction lock before us this
    // prevents UAF. If -captureOutput grabbed the destruction lock after us
    // it will exit early because |_frameReceiver| is already null at this
    // point.
    base::AutoLock destructionLock(_destructionLock);
  }
  [super dealloc];
}

- (void)setFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver {
  base::AutoLock lock(_lock);
  _frameReceiver = frameReceiver;
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
    [self stopStillImageOutput];
    if (_captureDeviceInput) {
      DCHECK(_captureDevice);
      [_captureSession stopRunning];
      [_captureSession removeInput:_captureDeviceInput];
      _captureDeviceInput.reset();
      _captureDevice.reset();
    }
    return YES;
  }

  // Look for input device with requested name.
  _captureDevice.reset([AVCaptureDevice deviceWithUniqueID:deviceId],
                       base::scoped_policy::RETAIN);
  if (!_captureDevice) {
    *outMessage = @"Could not open video capture device.";
    return NO;
  }

  // Create the capture input associated with the device. Easy peasy.
  NSError* error = nil;
  _captureDeviceInput.reset(
      [AVCaptureDeviceInput deviceInputWithDevice:_captureDevice error:&error],
      base::scoped_policy::RETAIN);
  if (!_captureDeviceInput) {
    _captureDevice.reset();
    *outMessage = [NSString
        stringWithFormat:@"Could not create video capture input (%@): %@",
                         [error localizedDescription],
                         [error localizedFailureReason]];
    return NO;
  }
  [_captureSession addInput:_captureDeviceInput];

  // Create a new data output for video. The data output is configured to
  // discard late frames by default.
  _captureVideoDataOutput.reset([[AVCaptureVideoDataOutput alloc] init]);
  if (!_captureVideoDataOutput) {
    [_captureSession removeInput:_captureDeviceInput];
    *outMessage = @"Could not create video data output.";
    return NO;
  }
  [_captureVideoDataOutput setAlwaysDiscardsLateVideoFrames:true];

  [_captureVideoDataOutput setSampleBufferDelegate:self queue:_sampleQueue];
  [_captureSession addOutput:_captureVideoDataOutput];

  return YES;
}

- (BOOL)setCaptureHeight:(int)height
                   width:(int)width
               frameRate:(float)frameRate {
  DCHECK(![_captureSession isRunning]);
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());

  _frameWidth = width;
  _frameHeight = height;
  _frameRate = frameRate;
  _bestCaptureFormat.reset(
      media::FindBestCaptureFormat([_captureDevice formats], width, height,
                                   frameRate),
      base::scoped_policy::RETAIN);
  FourCharCode best_fourcc = kDefaultFourCCPixelFormat;
  if (_bestCaptureFormat) {
    best_fourcc = CMFormatDescriptionGetMediaSubType(
        [_bestCaptureFormat formatDescription]);
  }

  if (best_fourcc == kCMVideoCodecType_JPEG_OpenDML) {
    // Capturing MJPEG for the following camera does not work (frames not
    // forwarded). macOS can convert to the default pixel format for us instead.
    // TODO(crbugs.com/1124884): figure out if there's another workaround.
    if ([[_captureDevice modelID] isEqualToString:kModelIdLogitech4KPro]) {
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
  // for iOS and not for MacOsX. AVVideoScalingModeKey() refers to letterboxing
  // yes/no and preserve aspect ratio yes/no when scaling. Currently we set
  // cropping and preservation.
  NSDictionary* videoSettingsDictionary = @{
    (id)kCVPixelBufferWidthKey : @(width),
    (id)kCVPixelBufferHeightKey : @(height),
    (id)kCVPixelBufferPixelFormatTypeKey : @(best_fourcc),
    AVVideoScalingModeKey : AVVideoScalingModeResizeAspectFill
  };
  [_captureVideoDataOutput setVideoSettings:videoSettingsDictionary];

  AVCaptureConnection* captureConnection =
      [_captureVideoDataOutput connectionWithMediaType:AVMediaTypeVideo];
  // CMTimeMake accepts integer arguments but |frameRate| is float, so round it.
  if ([captureConnection isVideoMinFrameDurationSupported]) {
    [captureConnection
        setVideoMinFrameDuration:CMTimeMake(media::kFrameRatePrecision,
                                            (int)(frameRate *
                                                  media::kFrameRatePrecision))];
  }
  if ([captureConnection isVideoMaxFrameDurationSupported]) {
    [captureConnection
        setVideoMaxFrameDuration:CMTimeMake(media::kFrameRatePrecision,
                                            (int)(frameRate *
                                                  media::kFrameRatePrecision))];
  }
  return YES;
}

- (void)setScaledResolutions:(std::vector<gfx::Size>)resolutions {
  if (!base::FeatureList::IsEnabled(media::kInCapturerScaling)) {
    return;
  }
  // The lock is needed for |_scaledFrameTransformers|.
  base::AutoLock lock(_lock);
  bool reconfigureScaledFrameTransformers = false;
  if (resolutions.size() != _scaledFrameTransformers.size()) {
    reconfigureScaledFrameTransformers = true;
  } else {
    for (const auto& resolution : resolutions) {
      bool resolutionHasTransformer = false;
      for (const auto& scaledFrameTransformer : _scaledFrameTransformers) {
        if (resolution == scaledFrameTransformer->destination_size()) {
          resolutionHasTransformer = true;
          break;
        }
      }
      if (!resolutionHasTransformer) {
        reconfigureScaledFrameTransformers = true;
        break;
      }
    }
  }
  if (!reconfigureScaledFrameTransformers)
    return;
  std::stringstream str;
  str << "[";
  for (size_t i = 0; i < resolutions.size(); ++i) {
    if (i != 0)
      str << ", ";
    str << resolutions[i].ToString();
  }
  str << "]";
  VLOG(1) << "Configuring scaled resolutions: " << str.str();
  _scaledFrameTransformers.clear();
  for (size_t i = 0; i < resolutions.size(); ++i) {
    DCHECK(i == 0 || resolutions[i - 1].height() >= resolutions[i].height());
    // Configure the transformer to and from NV12 pixel buffers - we only want
    // to pay scaling costs, not conversion costs.
    auto scaledFrameTransformer = media::SampleBufferTransformer::Create();
    scaledFrameTransformer->Reconfigure(
        media::SampleBufferTransformer::
            kBestTransformerForPixelBufferToNv12Output,
        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, resolutions[i],
        kPixelBufferPoolSize);
    _scaledFrameTransformers.push_back(std::move(scaledFrameTransformer));
  }
}

- (BOOL)startCapture {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  if (!_captureSession) {
    DLOG(ERROR) << "Video capture session not initialized.";
    return NO;
  }
  // Connect the notifications.
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
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
  _weakPtrFactoryForStallCheck.reset();
  [self stopStillImageOutput];
  if ([_captureSession isRunning])
    [_captureSession stopRunning];  // Synchronous.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)takePhoto {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  DCHECK([_captureSession isRunning]);

  ++_takePhotoStartedCount;

  // Ready to take a photo immediately?
  if (_stillImageOutput && _stillImageOutputWarmupCompleted) {
    [self takePhotoInternal];
    return;
  }

  // Lazily instantiate the |_stillImageOutput| the first time takePhoto() is
  // called. When takePhoto() isn't called, this avoids JPEG compession work for
  // every frame. This can save a lot of CPU in some cases (see
  // https://crbug.com/1116241). However because it can take a couple of second
  // for the 3A to stabilize, lazily instantiating like may result in noticeable
  // delays. To avoid delays in future takePhoto() calls we don't delete
  // |_stillImageOutput| until takePhoto() has not been called for 60 seconds.
  if (!_stillImageOutput) {
    // We use AVCaptureStillImageOutput for historical reasons, but note that it
    // has been deprecated in macOS 10.15[1] in favor of
    // AVCapturePhotoOutput[2].
    //
    // [1]
    // https://developer.apple.com/documentation/avfoundation/avcapturestillimageoutput
    // [2]
    // https://developer.apple.com/documentation/avfoundation/avcapturephotooutput
    // TODO(https://crbug.com/1124322): Migrate to the new API.
    _stillImageOutput.reset([[AVCaptureStillImageOutput alloc] init]);
    if (!_stillImageOutput ||
        ![_captureSession canAddOutput:_stillImageOutput]) {
      // Complete this started photo as error.
      ++_takePhotoPendingCount;
      {
        base::AutoLock lock(_lock);
        if (_frameReceiver) {
          _frameReceiver->OnPhotoError();
        }
      }
      [self takePhotoCompleted];
      return;
    }
    [_captureSession addOutput:_stillImageOutput];
    // A delay is needed before taking the photo or else the photo may be dark.
    // 2 seconds was enough in manual testing; we delay by 3 for good measure.
    _mainThreadTaskRunner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<VideoCaptureDeviceAVFoundation> weakSelf) {
              [weakSelf.get() takePhotoInternal];
            },
            _weakPtrFactoryForTakePhoto->GetWeakPtr()),
        base::Seconds(3));
  }
}

- (void)setOnStillImageOutputStoppedForTesting:
    (base::RepeatingCallback<void()>)onStillImageOutputStopped {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  _onStillImageOutputStopped = onStillImageOutputStopped;
}

#pragma mark Private methods

- (void)takePhotoInternal {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  // stopStillImageOutput invalidates all weak ptrs, meaning in-flight
  // operations are affectively cancelled. So if this method is running, still
  // image output must be good to go.
  DCHECK([_captureSession isRunning]);
  DCHECK(_stillImageOutput);
  DCHECK([[_stillImageOutput connections] count] == 1);
  AVCaptureConnection* const connection =
      [[_stillImageOutput connections] firstObject];
  DCHECK(connection);
  _stillImageOutputWarmupCompleted = true;

  // For all photos started that are not yet pending, take photos.
  while (_takePhotoPendingCount < _takePhotoStartedCount) {
    ++_takePhotoPendingCount;
    const auto handler = ^(CMSampleBufferRef sampleBuffer, NSError* error) {
      {
        base::AutoLock lock(_lock);
        if (_frameReceiver) {
          if (error != nil) {
            _frameReceiver->OnPhotoError();
          } else {
            // Recommended compressed pixel format is JPEG, we don't expect
            // surprises.
            // TODO(mcasas): Consider using [1] for merging EXIF output
            // information:
            // [1]
            // +(NSData*)jpegStillImageNSDataRepresentation:jpegSampleBuffer;
            DCHECK_EQ(kCMVideoCodecType_JPEG,
                      CMFormatDescriptionGetMediaSubType(
                          CMSampleBufferGetFormatDescription(sampleBuffer)));

            char* baseAddress = 0;
            size_t length = 0;
            const bool sample_buffer_addressable =
                media::ExtractBaseAddressAndLength(&baseAddress, &length,
                                                   sampleBuffer);
            DCHECK(sample_buffer_addressable);
            if (sample_buffer_addressable) {
              _frameReceiver->OnPhotoTaken(
                  reinterpret_cast<uint8_t*>(baseAddress), length,
                  "image/jpeg");
            }
          }
        }
      }
      // Called both on success and failure.
      _mainThreadTaskRunner->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<VideoCaptureDeviceAVFoundation> weakSelf) {
                [weakSelf.get() takePhotoCompleted];
              },
              _weakPtrFactoryForTakePhoto->GetWeakPtr()));
    };
    [_stillImageOutput captureStillImageAsynchronouslyFromConnection:connection
                                                   completionHandler:handler];
  }
}

- (void)takePhotoCompleted {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  ++_takePhotoCompletedCount;
  if (_takePhotoStartedCount != _takePhotoCompletedCount)
    return;
  // All pending takePhoto()s have completed. If no more photos are taken
  // within 60 seconds, stop still image output to avoid expensive MJPEG
  // conversions going forward.
  _mainThreadTaskRunner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<VideoCaptureDeviceAVFoundation> weakSelf,
             size_t takePhotoCount) {
            VideoCaptureDeviceAVFoundation* strongSelf = weakSelf.get();
            if (!strongSelf)
              return;
            // Don't stop the still image output if takePhoto() was called
            // while the task was pending.
            if (strongSelf->_takePhotoStartedCount != takePhotoCount)
              return;
            [strongSelf stopStillImageOutput];
          },
          _weakPtrFactoryForTakePhoto->GetWeakPtr(), _takePhotoStartedCount),
      base::Seconds(kTimeToWaitBeforeStoppingStillImageCaptureInSeconds));
}

- (void)stopStillImageOutput {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
  if (!_stillImageOutput) {
    // Already stopped.
    return;
  }
  if (_captureSession) {
    [_captureSession removeOutput:_stillImageOutput];
  }
  _stillImageOutput.reset();
  _stillImageOutputWarmupCompleted = false;

  // Cancel all in-flight operations.
  _weakPtrFactoryForTakePhoto->InvalidateWeakPtrs();
  // Report error for all pending calls that were stopped.
  size_t pendingCalls = _takePhotoStartedCount - _takePhotoCompletedCount;
  _takePhotoCompletedCount = _takePhotoPendingCount = _takePhotoStartedCount;
  {
    base::AutoLock lock(_lock);
    if (_frameReceiver) {
      for (size_t i = 0; i < pendingCalls; ++i) {
        _frameReceiver->OnPhotoError();
      }
    }
  }

  if (_onStillImageOutputStopped) {
    // Callback used by tests.
    _onStillImageOutputStopped.Run();
  }
}

- (void)processSample:(CMSampleBufferRef)sampleBuffer
        captureFormat:(const media::VideoCaptureFormat&)captureFormat
           colorSpace:(const gfx::ColorSpace&)colorSpace
            timestamp:(const base::TimeDelta)timestamp {
  VLOG(3) << __func__;
  // Trust |_frameReceiver| to do decompression.
  char* baseAddress = 0;
  size_t frameSize = 0;
  _lock.AssertAcquired();
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
          captureFormat, colorSpace, 0, 0, timestamp);
    }
  }
}

- (BOOL)processPixelBufferPlanes:(CVImageBufferRef)pixelBuffer
                   captureFormat:(const media::VideoCaptureFormat&)captureFormat
                      colorSpace:(const gfx::ColorSpace&)colorSpace
                       timestamp:(const base::TimeDelta)timestamp {
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
  _frameReceiver->ReceiveFrame(
      packedBufferCopy.empty() ? pixelBufferAddresses[0]
                               : packedBufferCopy.data(),
      frameSize, captureFormat, colorSpace, 0, 0, timestamp);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
  return YES;
}

- (void)processPixelBufferNV12IOSurface:(CVPixelBufferRef)pixelBuffer
                          captureFormat:
                              (const media::VideoCaptureFormat&)captureFormat
                             colorSpace:(const gfx::ColorSpace&)colorSpace
                              timestamp:(const base::TimeDelta)timestamp {
  VLOG(3) << __func__;
  DCHECK_EQ(captureFormat.pixel_format, media::PIXEL_FORMAT_NV12);

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  DCHECK(ioSurface);
  media::CapturedExternalVideoBuffer externalBuffer =
      [self capturedExternalVideoBufferFromNV12IOSurface:ioSurface
                                           captureFormat:captureFormat
                                              colorSpace:colorSpace];

  // The lock is needed for |_scaledFrameTransformers| and |_frameReceiver|.
  _lock.AssertAcquired();
  // References to any scaled pixel buffers need to be retained until after
  // ReceiveExternalGpuMemoryBufferFrame().
  std::vector<base::ScopedCFTypeRef<CVPixelBufferRef>> scaledPixelBuffers;
  std::vector<media::CapturedExternalVideoBuffer> scaledExternalBuffers;
  scaledPixelBuffers.reserve(_scaledFrameTransformers.size());
  scaledExternalBuffers.reserve(_scaledFrameTransformers.size());
  for (auto& scaledFrameTransformer : _scaledFrameTransformers) {
    gfx::Size scaledFrameSize = scaledFrameTransformer->destination_size();
    // Only proceed if this results in downscaling in one or both dimensions.
    //
    // It is not clear that we want to continue to allow changing the aspect
    // ratio like this since this causes visible stretching in the image if the
    // stretch is significantly large.
    // TODO(https://crbug.com/1157072): When we know what to do about aspect
    // ratios, consider adding a DCHECK here or otherwise ignore wrong aspect
    // ratios (within some fault tolerance).
    if (scaledFrameSize.width() > captureFormat.frame_size.width() ||
        scaledFrameSize.height() > captureFormat.frame_size.height() ||
        scaledFrameSize == captureFormat.frame_size) {
      continue;
    }
    CVPixelBufferRef bufferToScale =
        !scaledPixelBuffers.empty() ? scaledPixelBuffers.back() : pixelBuffer;
    base::ScopedCFTypeRef<CVPixelBufferRef> scaledPixelBuffer =
        scaledFrameTransformer->Transform(bufferToScale);
    if (!scaledPixelBuffer) {
      LOG(ERROR) << "Failed to downscale frame, skipping resolution "
                 << scaledFrameSize.ToString();
      continue;
    }
    scaledPixelBuffers.push_back(scaledPixelBuffer);
    IOSurfaceRef scaledIoSurface = CVPixelBufferGetIOSurface(scaledPixelBuffer);
    media::VideoCaptureFormat scaledCaptureFormat = captureFormat;
    scaledCaptureFormat.frame_size = scaledFrameSize;
    scaledExternalBuffers.push_back([self
        capturedExternalVideoBufferFromNV12IOSurface:scaledIoSurface
                                       captureFormat:scaledCaptureFormat
                                          colorSpace:colorSpace]);
  }

  _frameReceiver->ReceiveExternalGpuMemoryBufferFrame(
      std::move(externalBuffer), std::move(scaledExternalBuffers), timestamp);
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
  if (colorSpace == kColorSpaceRec709Apple) {
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
    if (!_capturedFirstFrame)
      nextFailedCheckCount = 0;

    // If we captured a frame since last check, then we aren't stalled.
    if (_capturedFrameSinceLastStallCheck)
      nextFailedCheckCount = 0;
    _capturedFrameSinceLastStallCheck = NO;
  }

  constexpr int kMaxFailedCheckCount = 5;
  if (nextFailedCheckCount < kMaxFailedCheckCount) {
    // Post a task to check for progress in 1 second. Create the weak factory
    // for the posted task, if needed.
    if (!_weakPtrFactoryForStallCheck) {
      _weakPtrFactoryForStallCheck = std::make_unique<
          base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>(self);
    }
    constexpr base::TimeDelta kStallCheckInterval = base::Seconds(1);
    auto callback_lambda =
        [](base::WeakPtr<VideoCaptureDeviceAVFoundation> weakSelf,
           int failedCheckCount) {
          VideoCaptureDeviceAVFoundation* strongSelf = weakSelf.get();
          if (!strongSelf)
            return;
          [strongSelf doStallCheck:failedCheckCount];
        };
    _mainThreadTaskRunner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(callback_lambda,
                       _weakPtrFactoryForStallCheck->GetWeakPtr(),
                       nextFailedCheckCount),
        kStallCheckInterval);
  } else {
    // Capture appears to be stalled. Restart it.
    LOG(ERROR) << "Capture appears to have stalled, restarting.";
    [self stopCapture];
    [self startCapture];
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
  if (!_frameReceiver)
    return;

  const base::TimeDelta pres_timestamp =
      GetCMSampleBufferTimestamp(sampleBuffer);
  if (start_timestamp_.is_zero()) {
    start_timestamp_ = pres_timestamp;
  }
  const base::TimeDelta timestamp = pres_timestamp - start_timestamp_;

  bool logUma = !std::exchange(_capturedFirstFrame, true);
  if (logUma) {
    media::LogFirstCapturedVideoFrame(_bestCaptureFormat, sampleBuffer);
  }

  // The SampleBufferTransformer CHECK-crashes if the sample buffer is not MJPEG
  // and does not have a pixel buffer (https://crbug.com/1160647) so we fall
  // back on the M87 code path if this is the case.
  // TODO(https://crbug.com/1160315): When the SampleBufferTransformer is
  // patched to support non-MJPEG-and-non-pixel-buffer sample buffers, remove
  // this workaround and the fallback other code path.
  bool sampleHasPixelBufferOrIsMjpeg =
      CMSampleBufferGetImageBuffer(sampleBuffer) ||
      CMFormatDescriptionGetMediaSubType(CMSampleBufferGetFormatDescription(
          sampleBuffer)) == kCMVideoCodecType_JPEG_OpenDML;

  // If the SampleBufferTransformer is enabled, convert all possible capture
  // formats to an IOSurface-backed NV12 pixel buffer.
  // TODO(https://crbug.com/1175142): Refactor to not hijack the code paths
  // below the transformer code.
  if (sampleHasPixelBufferOrIsMjpeg) {
    _sampleBufferTransformer->Reconfigure(
        media::SampleBufferTransformer::GetBestTransformerForNv12Output(
            sampleBuffer),
        kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
        media::GetSampleBufferSize(sampleBuffer), kPixelBufferPoolSize);
    base::ScopedCFTypeRef<CVPixelBufferRef> pixelBuffer =
        _sampleBufferTransformer->Transform(sampleBuffer);
    if (!pixelBuffer) {
      LOG(ERROR) << "Failed to transform captured frame. Dropping frame.";
      return;
    }
    const media::VideoCaptureFormat captureFormat(
        gfx::Size(CVPixelBufferGetWidth(pixelBuffer),
                  CVPixelBufferGetHeight(pixelBuffer)),
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
    [self processPixelBufferNV12IOSurface:pixelBuffer
                            captureFormat:captureFormat
                               colorSpace:kColorSpaceRec709Apple
                                timestamp:timestamp];
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
    if (CVPixelBufferGetIOSurface(pixelBuffer) &&
        videoPixelFormat == media::PIXEL_FORMAT_NV12) {
      [self processPixelBufferNV12IOSurface:pixelBuffer
                              captureFormat:captureFormat
                                 colorSpace:colorSpace
                                  timestamp:timestamp];
      return;
    }

    // Second preference is to read the CVPixelBuffer's planes.
    if ([self processPixelBufferPlanes:pixelBuffer
                         captureFormat:captureFormat
                            colorSpace:colorSpace
                             timestamp:timestamp]) {
      return;
    }
  }

  // Last preference is to read the CMSampleBuffer.
  gfx::ColorSpace colorSpace =
      media::GetFormatDescriptionColorSpace(formatDescription);
  [self processSample:sampleBuffer
        captureFormat:captureFormat
           colorSpace:colorSpace
            timestamp:timestamp];
}

- (void)onVideoError:(NSNotification*)errorNotification {
  NSError* error = base::mac::ObjCCast<NSError>(
      [errorNotification userInfo][AVCaptureSessionErrorKey]);
  [self sendErrorString:[NSString
                            stringWithFormat:@"%@: %@",
                                             [error localizedDescription],
                                             [error localizedFailureReason]]];
}

- (void)sendErrorString:(NSString*)error {
  DLOG(ERROR) << base::SysNSStringToUTF8(error);
  base::AutoLock lock(_lock);
  if (_frameReceiver)
    _frameReceiver->ReceiveError(
        media::VideoCaptureError::
            kMacAvFoundationReceivedAVCaptureSessionRuntimeErrorNotification,
        FROM_HERE, base::SysNSStringToUTF8(error));
}

- (void)callLocked:(base::OnceClosure)lambda {
  base::AutoLock lock(_lock);
  std::move(lambda).Run();
}

@end

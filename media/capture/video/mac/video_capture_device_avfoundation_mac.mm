// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <stddef.h>
#include <stdint.h>

#include "base/location.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_types.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_utils_mac.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "media/capture/video_capture_types.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr int kTimeToWaitBeforeStoppingStillImageCaptureInSeconds = 60;

}  // anonymous namespace

@implementation VideoCaptureDeviceAVFoundation

#pragma mark Public methods

- (id)initWithFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver {
  if ((self = [super init])) {
    _mainThreadTaskRunner = base::ThreadTaskRunnerHandle::Get();
    DCHECK(frameReceiver);
    _weakPtrFactoryForTakePhoto =
        std::make_unique<base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>(
            self);
    [self setFrameReceiver:frameReceiver];
    _captureSession.reset([[AVCaptureSession alloc] init]);
  }
  return self;
}

- (void)dealloc {
  [self stopStillImageOutput];
  [self stopCapture];
  _weakPtrFactoryForTakePhoto = nullptr;
  _mainThreadTaskRunner = nullptr;
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
    *outMessage =
        [NSString stringWithUTF8String:"Could not open video capture device."];
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
    *outMessage =
        [NSString stringWithUTF8String:"Could not create video data output."];
    return NO;
  }
  [_captureVideoDataOutput setAlwaysDiscardsLateVideoFrames:true];
  [_captureVideoDataOutput
      setSampleBufferDelegate:self
                        queue:dispatch_get_global_queue(
                                  DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
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

  FourCharCode best_fourcc = kCMPixelFormat_422YpCbCr8;
  if (_bestCaptureFormat) {
    best_fourcc = CMFormatDescriptionGetMediaSubType(
        [_bestCaptureFormat formatDescription]);
  }

  if (best_fourcc == kCMVideoCodecType_JPEG_OpenDML) {
    // Capturing MJPEG directly never worked. Request a conversion to what has
    // historically been the default pixel format.
    // TODO(https://crbug.com/1124884): Investigate the performance of
    // performing MJPEG ourselves.
    best_fourcc = kCMPixelFormat_422YpCbCr8;
  }

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
  // Check selector existence, related to bugs http://crbug.com/327532 and
  // http://crbug.com/328096.
  // CMTimeMake accepts integer argumenst but |frameRate| is float, round it.
  if ([captureConnection
          respondsToSelector:@selector(isVideoMinFrameDurationSupported)] &&
      [captureConnection isVideoMinFrameDurationSupported]) {
    [captureConnection
        setVideoMinFrameDuration:CMTimeMake(media::kFrameRatePrecision,
                                            (int)(frameRate *
                                                  media::kFrameRatePrecision))];
  }
  if ([captureConnection
          respondsToSelector:@selector(isVideoMaxFrameDurationSupported)] &&
      [captureConnection isVideoMaxFrameDurationSupported]) {
    [captureConnection
        setVideoMaxFrameDuration:CMTimeMake(media::kFrameRatePrecision,
                                            (int)(frameRate *
                                                  media::kFrameRatePrecision))];
  }
  return YES;
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
  [_captureSession startRunning];

  // Update the active capture format once the capture session is running.
  // Setting it before the capture session is running has no effect.
  if (_bestCaptureFormat) {
    if ([_captureDevice lockForConfiguration:nil]) {
      [_captureDevice setActiveFormat:_bestCaptureFormat];
      [_captureDevice unlockForConfiguration];
    }
  }

  return YES;
}

- (void)stopCapture {
  DCHECK(_mainThreadTaskRunner->BelongsToCurrentThread());
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
        base::TimeDelta::FromSeconds(3));
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
            media::ExtractBaseAddressAndLength(&baseAddress, &length,
                                               sampleBuffer);
            _frameReceiver->OnPhotoTaken(
                reinterpret_cast<uint8_t*>(baseAddress), length, "image/jpeg");
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
      base::TimeDelta::FromSeconds(
          kTimeToWaitBeforeStoppingStillImageCaptureInSeconds));
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

// |captureOutput| is called by the capture device to deliver a new frame.
// AVFoundation calls from a number of threads, depending on, at least, if
// Chrome is on foreground or background.
- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
  const CMFormatDescriptionRef formatDescription =
      CMSampleBufferGetFormatDescription(sampleBuffer);
  const FourCharCode fourcc =
      CMFormatDescriptionGetMediaSubType(formatDescription);
  const CMVideoDimensions dimensions =
      CMVideoFormatDescriptionGetDimensions(formatDescription);
  const media::VideoCaptureFormat captureFormat(
      gfx::Size(dimensions.width, dimensions.height), _frameRate,
      media::FourCCToChromiumPixelFormat(fourcc));
  gfx::ColorSpace colorSpace;

  // We have certain format expectation for capture output:
  // For MJPEG, |sampleBuffer| is expected to always be a CVBlockBuffer.
  // For other formats, |sampleBuffer| may be either CVBlockBuffer or
  // CVImageBuffer. CVBlockBuffer seems to be used in the context of CoreMedia
  // plugins/virtual cameras. In order to find out whether it is CVBlockBuffer
  // or CVImageBuffer we call CMSampleBufferGetImageBuffer() and check if the
  // return value is nil.
  char* baseAddress = 0;
  size_t frameSize = 0;
  CVImageBufferRef videoFrame = nil;
  if (fourcc != kCMVideoCodecType_JPEG_OpenDML) {
    videoFrame = CMSampleBufferGetImageBuffer(sampleBuffer);
    // Lock the frame and calculate frame size.
    if (videoFrame &&
        CVPixelBufferLockBaseAddress(videoFrame, kCVPixelBufferLock_ReadOnly) ==
            kCVReturnSuccess) {
      baseAddress = static_cast<char*>(CVPixelBufferGetBaseAddress(videoFrame));
      frameSize = CVPixelBufferGetHeight(videoFrame) *
                  CVPixelBufferGetBytesPerRow(videoFrame);

      // TODO(julien.isorce): move GetImageBufferColorSpace(CVImageBufferRef)
      // from media::VTVideoDecodeAccelerator to media/base/mac and call it
      // here to get the color space. See https://crbug.com/959962.
      // colorSpace = media::GetImageBufferColorSpace(videoFrame);
    } else {
      videoFrame = nil;
    }
  }
  if (!videoFrame) {
    media::ExtractBaseAddressAndLength(&baseAddress, &frameSize, sampleBuffer);
  }

  {
    base::AutoLock lock(_lock);
    const CMTime cm_timestamp =
        CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    const base::TimeDelta timestamp =
        CMTIME_IS_VALID(cm_timestamp)
            ? base::TimeDelta::FromMicroseconds(
                  cm_timestamp.value * base::TimeTicks::kMicrosecondsPerSecond /
                  cm_timestamp.timescale)
            : media::kNoTimestamp;

    if (_frameReceiver && baseAddress) {
      _frameReceiver->ReceiveFrame(reinterpret_cast<uint8_t*>(baseAddress),
                                   frameSize, captureFormat, colorSpace, 0, 0,
                                   timestamp);
    }
  }

  if (videoFrame)
    CVPixelBufferUnlockBaseAddress(videoFrame, kCVPixelBufferLock_ReadOnly);
}

- (void)onVideoError:(NSNotification*)errorNotification {
  NSError* error = base::mac::ObjCCast<NSError>(
      [[errorNotification userInfo] objectForKey:AVCaptureSessionErrorKey]);
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

@end

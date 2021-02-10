// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/capture/video/mac/video_capture_device_avfoundation_legacy_mac.h"

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
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "media/base/timestamp_constants.h"
#include "media/capture/video/mac/video_capture_device_factory_mac.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "media/capture/video_capture_types.h"
#include "services/video_capture/public/uma/video_capture_service_event.h"
#include "ui/gfx/geometry/size.h"

namespace {

enum MacBookVersions {
  OTHER = 0,
  MACBOOK_5,  // MacBook5.X
  MACBOOK_6,
  MACBOOK_7,
  MACBOOK_8,
  MACBOOK_PRO_11,  // MacBookPro11.X
  MACBOOK_PRO_12,
  MACBOOK_PRO_13,
  MACBOOK_AIR_5,  // MacBookAir5.X
  MACBOOK_AIR_6,
  MACBOOK_AIR_7,
  MACBOOK_AIR_8,
  MACBOOK_AIR_3,
  MACBOOK_AIR_4,
  MACBOOK_4,
  MACBOOK_9,
  MACBOOK_10,
  MACBOOK_PRO_10,
  MACBOOK_PRO_9,
  MACBOOK_PRO_8,
  MACBOOK_PRO_7,
  MACBOOK_PRO_6,
  MACBOOK_PRO_5,
  MAX_MACBOOK_VERSION = MACBOOK_PRO_5
};

MacBookVersions GetMacBookModel(const std::string& model) {
  struct {
    const char* name;
    MacBookVersions version;
  } static const kModelToVersion[] = {
      {"MacBook4,", MACBOOK_4},          {"MacBook5,", MACBOOK_5},
      {"MacBook6,", MACBOOK_6},          {"MacBook7,", MACBOOK_7},
      {"MacBook8,", MACBOOK_8},          {"MacBook9,", MACBOOK_9},
      {"MacBook10,", MACBOOK_10},        {"MacBookPro5,", MACBOOK_PRO_5},
      {"MacBookPro6,", MACBOOK_PRO_6},   {"MacBookPro7,", MACBOOK_PRO_7},
      {"MacBookPro8,", MACBOOK_PRO_8},   {"MacBookPro9,", MACBOOK_PRO_9},
      {"MacBookPro10,", MACBOOK_PRO_10}, {"MacBookPro11,", MACBOOK_PRO_11},
      {"MacBookPro12,", MACBOOK_PRO_12}, {"MacBookPro13,", MACBOOK_PRO_13},
      {"MacBookAir3,", MACBOOK_AIR_3},   {"MacBookAir4,", MACBOOK_AIR_4},
      {"MacBookAir5,", MACBOOK_AIR_5},   {"MacBookAir6,", MACBOOK_AIR_6},
      {"MacBookAir7,", MACBOOK_AIR_7},   {"MacBookAir8,", MACBOOK_AIR_8},
  };

  for (const auto& entry : kModelToVersion) {
    if (base::StartsWith(model, entry.name,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return entry.version;
    }
  }
  return OTHER;
}

// Add Uma stats for number of detected devices on MacBooks. These are used for
// investigating crbug/582931.
void MaybeWriteUma(int number_of_devices, int number_of_suspended_devices) {
  std::string model = base::mac::GetModelIdentifier();
  if (!base::StartsWith(model, "MacBook",
                        base::CompareCase::INSENSITIVE_ASCII)) {
    return;
  }
  static int attempt_since_process_start_counter = 0;
  static int device_count_at_last_attempt = 0;
  static bool has_seen_zero_device_count = false;
  const int attempt_count_since_process_start =
      ++attempt_since_process_start_counter;
  const int retry_count =
      media::VideoCaptureDeviceFactoryMac::GetGetDevicesInfoRetryCount();
  const int device_count = number_of_devices + number_of_suspended_devices;
  UMA_HISTOGRAM_COUNTS_1M("Media.VideoCapture.MacBook.NumberOfDevices",
                          device_count);
  if (device_count == 0) {
    UMA_HISTOGRAM_ENUMERATION(
        "Media.VideoCapture.MacBook.HardwareVersionWhenNoCamera",
        GetMacBookModel(model), MAX_MACBOOK_VERSION + 1);
    if (!has_seen_zero_device_count) {
      UMA_HISTOGRAM_COUNTS_1M(
          "Media.VideoCapture.MacBook.AttemptCountWhenNoCamera",
          attempt_count_since_process_start);
      has_seen_zero_device_count = true;
    }
  }

  if (attempt_count_since_process_start == 1) {
    if (retry_count == 0) {
      video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
          device_count == 0
              ? video_capture::uma::
                    AVF_RECEIVED_ZERO_INFOS_FIRST_TRY_FIRST_ATTEMPT
              : video_capture::uma::
                    AVF_RECEIVED_NONZERO_INFOS_FIRST_TRY_FIRST_ATTEMPT);
    } else {
      video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
          device_count == 0
              ? video_capture::uma::AVF_RECEIVED_ZERO_INFOS_RETRY
              : video_capture::uma::AVF_RECEIVED_NONZERO_INFOS_RETRY);
    }
    // attempt count > 1
  } else if (retry_count == 0) {
    video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
        device_count == 0
            ? video_capture::uma::
                  AVF_RECEIVED_ZERO_INFOS_FIRST_TRY_NONFIRST_ATTEMPT
            : video_capture::uma::
                  AVF_RECEIVED_NONZERO_INFOS_FIRST_TRY_NONFIRST_ATTEMPT);
  }
  if (attempt_count_since_process_start > 1 &&
      device_count != device_count_at_last_attempt) {
    video_capture::uma::LogMacbookRetryGetDeviceInfosEvent(
        device_count == 0
            ? video_capture::uma::AVF_DEVICE_COUNT_CHANGED_FROM_POSITIVE_TO_ZERO
            : video_capture::uma::
                  AVF_DEVICE_COUNT_CHANGED_FROM_ZERO_TO_POSITIVE);
  }
  device_count_at_last_attempt = device_count;
}

// Extracts |base_address| and |length| out of a SampleBuffer.
void ExtractBaseAddressAndLength(char** base_address,
                                 size_t* length,
                                 CMSampleBufferRef sample_buffer) {
  CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sample_buffer);
  DCHECK(block_buffer);

  size_t length_at_offset;
  const OSStatus status = CMBlockBufferGetDataPointer(
      block_buffer, 0, &length_at_offset, length, base_address);
  DCHECK_EQ(noErr, status);
  // Expect the (M)JPEG data to be available as a contiguous reference, i.e.
  // not covered by multiple memory blocks.
  DCHECK_EQ(length_at_offset, *length);
}

}  // anonymous namespace

@implementation VideoCaptureDeviceAVFoundationLegacy

#pragma mark Class methods

+ (media::VideoPixelFormat)FourCCToChromiumPixelFormat:(FourCharCode)code {
  switch (code) {
    case kCVPixelFormatType_422YpCbCr8:
      return media::PIXEL_FORMAT_UYVY;
    case kCMPixelFormat_422YpCbCr8_yuvs:
      return media::PIXEL_FORMAT_YUY2;
    case kCMVideoCodecType_JPEG_OpenDML:
      return media::PIXEL_FORMAT_MJPEG;
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

+ (void)getDeviceNames:(NSMutableDictionary*)deviceNames {
  // At this stage we already know that AVFoundation is supported and the whole
  // library is loaded and initialised, by the device monitoring.
  NSArray* devices = [AVCaptureDevice devices];
  int number_of_suspended_devices = 0;
  for (AVCaptureDevice* device in devices) {
    if ([device hasMediaType:AVMediaTypeVideo] ||
        [device hasMediaType:AVMediaTypeMuxed]) {
      if ([device isSuspended]) {
        ++number_of_suspended_devices;
        continue;
      }
      DeviceNameAndTransportType* nameAndTransportType =
          [[[DeviceNameAndTransportType alloc]
               initWithName:[device localizedName]
              transportType:[device transportType]] autorelease];
      [deviceNames setObject:nameAndTransportType forKey:[device uniqueID]];
    }
  }
  MaybeWriteUma([deviceNames count], number_of_suspended_devices);
}

+ (NSDictionary*)deviceNames {
  NSMutableDictionary* deviceNames =
      [[[NSMutableDictionary alloc] init] autorelease];
  // The device name retrieval is not going to happen in the main thread, and
  // this might cause instabilities (it did in QTKit), so keep an eye here.
  [self getDeviceNames:deviceNames];
  return deviceNames;
}

+ (void)getDevice:(const media::VideoCaptureDeviceDescriptor&)descriptor
    supportedFormats:(media::VideoCaptureFormats*)formats {
  NSArray* devices = [AVCaptureDevice devices];
  AVCaptureDevice* device = nil;
  for (device in devices) {
    if (base::SysNSStringToUTF8([device uniqueID]) == descriptor.device_id)
      break;
  }
  if (device == nil)
    return;
  for (AVCaptureDeviceFormat* format in device.formats) {
    // MediaSubType is a CMPixelFormatType but can be used as CVPixelFormatType
    // as well according to CMFormatDescription.h
    const media::VideoPixelFormat pixelFormat =
        [VideoCaptureDeviceAVFoundationLegacy
            FourCCToChromiumPixelFormat:CMFormatDescriptionGetMediaSubType(
                                            [format formatDescription])];

    CMVideoDimensions dimensions =
        CMVideoFormatDescriptionGetDimensions([format formatDescription]);

    for (AVFrameRateRange* frameRate in
         [format videoSupportedFrameRateRanges]) {
      media::VideoCaptureFormat format(
          gfx::Size(dimensions.width, dimensions.height),
          frameRate.maxFrameRate, pixelFormat);
      formats->push_back(format);
      DVLOG(2) << descriptor.display_name() << " "
               << media::VideoCaptureFormat::ToString(format);
    }
  }
}

#pragma mark Public methods

- (id)initWithFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver {
  if ((self = [super init])) {
    DCHECK(_main_thread_checker.CalledOnValidThread());
    DCHECK(frameReceiver);
    [self setFrameReceiver:frameReceiver];
    _captureSession.reset([[AVCaptureSession alloc] init]);
  }
  return self;
}

- (void)dealloc {
  [self stopCapture];
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
  DCHECK(_main_thread_checker.CalledOnValidThread());

  if (!deviceId) {
    // First stop the capture session, if it's running.
    [self stopCapture];
    // Now remove the input and output from the capture session.
    [_captureSession removeOutput:_captureVideoDataOutput];
    if (_stillImageOutput)
      [_captureSession removeOutput:_stillImageOutput];
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

  // Create and plug the still image capture output. This should happen in
  // advance of the actual picture to allow for the 3A to stabilize.
  _stillImageOutput.reset([[AVCaptureStillImageOutput alloc] init]);
  if (_stillImageOutput && [_captureSession canAddOutput:_stillImageOutput])
    [_captureSession addOutput:_stillImageOutput];

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
  DCHECK(![_captureSession isRunning] &&
         _main_thread_checker.CalledOnValidThread());

  _frameWidth = width;
  _frameHeight = height;
  _frameRate = frameRate;

  FourCharCode best_fourcc = kCMPixelFormat_422YpCbCr8;
  for (AVCaptureDeviceFormat* format in [_captureDevice formats]) {
    const FourCharCode fourcc =
        CMFormatDescriptionGetMediaSubType([format formatDescription]);
    // Compare according to Chromium preference.
    if (media::VideoCaptureFormat::ComparePixelFormatPreference(
            [VideoCaptureDeviceAVFoundationLegacy
                FourCCToChromiumPixelFormat:fourcc],
            [VideoCaptureDeviceAVFoundationLegacy
                FourCCToChromiumPixelFormat:best_fourcc])) {
      best_fourcc = fourcc;
    }
  }

  if (best_fourcc == kCMVideoCodecType_JPEG_OpenDML) {
    [_captureSession removeOutput:_stillImageOutput];
    _stillImageOutput.reset();
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

- (void)setScaledResolutions:(std::vector<gfx::Size>)resolutions {
  // The legacy capturer does not implement in-capturer scaling.
}

- (BOOL)startCapture {
  DCHECK(_main_thread_checker.CalledOnValidThread());
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
  return YES;
}

- (void)stopCapture {
  DCHECK(_main_thread_checker.CalledOnValidThread());
  if ([_captureSession isRunning])
    [_captureSession stopRunning];  // Synchronous.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)takePhoto {
  DCHECK(_main_thread_checker.CalledOnValidThread());
  DCHECK([_captureSession isRunning]);
  if (!_stillImageOutput)
    return;

  DCHECK_EQ(1u, [[_stillImageOutput connections] count]);
  AVCaptureConnection* const connection =
      [[_stillImageOutput connections] firstObject];
  if (!connection) {
    base::AutoLock lock(_lock);
    _frameReceiver->OnPhotoError();
    return;
  }

  const auto handler = ^(CMSampleBufferRef sampleBuffer, NSError* error) {
    base::AutoLock lock(_lock);
    if (!_frameReceiver)
      return;
    if (error != nil) {
      _frameReceiver->OnPhotoError();
      return;
    }

    // Recommended compressed pixel format is JPEG, we don't expect surprises.
    // TODO(mcasas): Consider using [1] for merging EXIF output information:
    // [1] +(NSData*)jpegStillImageNSDataRepresentation:jpegSampleBuffer;
    DCHECK_EQ(kCMVideoCodecType_JPEG,
              CMFormatDescriptionGetMediaSubType(
                  CMSampleBufferGetFormatDescription(sampleBuffer)));

    char* baseAddress = 0;
    size_t length = 0;
    ExtractBaseAddressAndLength(&baseAddress, &length, sampleBuffer);
    _frameReceiver->OnPhotoTaken(reinterpret_cast<uint8_t*>(baseAddress),
                                 length, "image/jpeg");
  };

  [_stillImageOutput captureStillImageAsynchronouslyFromConnection:connection
                                                 completionHandler:handler];
}

#pragma mark Private methods

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
      [VideoCaptureDeviceAVFoundationLegacy
          FourCCToChromiumPixelFormat:fourcc]);
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
    ExtractBaseAddressAndLength(&baseAddress, &frameSize, sampleBuffer);
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

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video/apple/sample_buffer_transformer.h"
#include "media/capture/video/apple/video_capture_device_frame_receiver.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

// Find the best capture format from |formats| for the specified dimensions and
// frame rate. Returns an element of |formats|, or nil.
AVCaptureDeviceFormat* CAPTURE_EXPORT
FindBestCaptureFormat(NSArray<AVCaptureDeviceFormat*>* formats,
                      int width,
                      int height,
                      float frame_rate);

}  // namespace media

// TODO(crbug.com/40148253): rename this file to be suffixed by the
// "next generation" moniker.
CAPTURE_EXPORT
@interface VideoCaptureDeviceAVFoundation
    : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate,
                AVCapturePhotoCaptureDelegate>

// Previous to any use, clients must call -initWithFrameReceiver: to
// initialise an object of this class and register a |frameReceiver_|. This
// initializes the instance and the underlying capture session and registers the
// frame receiver.
- (instancetype)initWithFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver;

// Frame receiver registration or removal can also happen via explicit call
// to -setFrameReceiver:. Re-registrations are safe and allowed, even during
// capture using this method.
- (void)setFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver;

// Whether to use a GPU memory for video frames or not.
- (void)setUseGPUMemoryBuffer:(bool)useGPUMemoryBuffer;

// Sets which capture device to use by name, retrieved via |deviceNames|.
// Method -setCaptureDevice: must be called at least once with a device
// identifier from GetVideoCaptureDeviceNames(). It creates all the necessary
// AVFoundation objects on the first call; it connects them ready for capture
// every time. Once the deviceId is known, the library objects are created if
// needed and connected for the capture, and a by default resolution is set. If
// |deviceId| is nil, then the eventual capture is stopped and library objects
// are disconnected. Returns YES on success, NO otherwise. If the return value
// is NO, an error message is assigned to |outMessage|. This method should not
// be called during capture (i.e. between -startCapture and -stopCapture).
- (BOOL)setCaptureDevice:(NSString*)deviceId
            errorMessage:(NSString**)outMessage;

// Configures the capture properties for the capture session and the video data
// output; this means it MUST be called after setCaptureDevice:. Return YES on
// success, else NO.
- (BOOL)setCaptureHeight:(int)height
                   width:(int)width
               frameRate:(float)frameRate;

// Starts video capturing and registers notification listeners. Must be
// called after setCaptureDevice:, and, eventually, also after
// setCaptureHeight:width:frameRate:.
// The capture can be stopped and restarted multiple times, potentially
// reconfiguring the device in between.
// Returns YES on success, NO otherwise.
- (BOOL)startCapture;

// Stops video capturing and stops listening to notifications. Same as
// setCaptureDevice:nil but doesn't disconnect the library objects. The capture
// can be
- (void)stopCapture;

// Takes a photo. This method should only be called between -startCapture and
// -stopCapture.
- (void)takePhoto;

// This function translates Mac Core Video pixel formats to Chromium pixel
// formats. This implementation recognizes NV12.
+ (media::VideoPixelFormat)FourCCToChromiumPixelFormat:(FourCharCode)code;

- (void)setOnPhotoOutputStoppedForTesting:
    (base::RepeatingCallback<void()>)onPhotoOutputStopped;

// Use the below only for test.
- (void)callLocked:(base::OnceClosure)lambda;

- (void)processPixelBufferNV12IOSurface:(CVPixelBufferRef)pixelBuffer
                          captureFormat:
                              (const media::VideoCaptureFormat&)captureFormat
                             colorSpace:(const gfx::ColorSpace&)colorSpace
                              timestamp:(const base::TimeDelta)timestamp
                     capture_begin_time:
                         (std::optional<base::TimeTicks>)capture_begin_time;

- (BOOL)processPixelBufferPlanes:(CVImageBufferRef)pixelBuffer
                   captureFormat:(const media::VideoCaptureFormat&)captureFormat
                      colorSpace:(const gfx::ColorSpace&)colorSpace
                       timestamp:(const base::TimeDelta)timestamp
              capture_begin_time:
                  (std::optional<base::TimeTicks>)capture_begin_time;

// Returns whether the format supports the Portrait Effect feature or not.
- (bool)isPortraitEffectSupported;

// Returns whether the Portrait Effect is active on a device or not.
- (bool)isPortraitEffectActive;

- (void)setIsPortraitEffectSupportedForTesting:
    (bool)isPortraitEffectSupportedForTesting;
- (void)setIsPortraitEffectActiveForTesting:
    (bool)isPortraitEffectActiveForTesting;

@end

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_H_

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include "base/callback_forward.h"

#include "base/mac/scoped_dispatch_object.h"
#include "base/mac/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video/mac/sample_buffer_transformer_mac.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_protocol_mac.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

// When this feature is enabled, the capturer can be configured using
// setScaledResolutions to output scaled versions of the captured frame (in
// addition to the original frame), whenever NV12 IOSurfaces are available to
// the capturer. These are available either when the camera supports it and
// kAVFoundationCaptureV2ZeroCopy is enabled or when kInCaptureConvertToNv12 is
// used to convert frames to NV12.
CAPTURE_EXPORT extern const base::Feature kInCapturerScaling;

// Find the best capture format from |formats| for the specified dimensions and
// frame rate. Returns an element of |formats|, or nil.
AVCaptureDeviceFormat* CAPTURE_EXPORT
FindBestCaptureFormat(NSArray<AVCaptureDeviceFormat*>* formats,
                      int width,
                      int height,
                      float frame_rate);

}  // namespace media

// TODO(crbug.com/1126690): rename this file to be suffixed by the
// "next generation" moniker.
CAPTURE_EXPORT
@interface VideoCaptureDeviceAVFoundation
    : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate,
                VideoCaptureDeviceAVFoundationProtocol> {
 @private
  // The following attributes are set via -setCaptureHeight:width:frameRate:.
  int _frameWidth;
  int _frameHeight;
  float _frameRate;

  // The capture format that best matches the above attributes.
  base::scoped_nsobject<AVCaptureDeviceFormat> _bestCaptureFormat;

  // A serial queue to deliver frames on, ensuring frames are delivered in
  // order.
  base::ScopedDispatchObject<dispatch_queue_t> _sampleQueue;

  // Protects concurrent setting and using |frameReceiver_|. Note that the
  // GUARDED_BY decoration below does not have any effect.
  base::Lock _lock;
  media::VideoCaptureDeviceAVFoundationFrameReceiver* _frameReceiver
      GUARDED_BY(_lock);  // weak.
  bool _capturedFirstFrame GUARDED_BY(_lock);
  bool _capturedFrameSinceLastStallCheck GUARDED_BY(_lock);
  std::unique_ptr<base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>
      _weakPtrFactoryForStallCheck;

  // Used to rate-limit crash reports for https://crbug.com/1168112.
  bool _hasDumpedForFrameSizeMismatch;

  base::scoped_nsobject<AVCaptureSession> _captureSession;

  // |captureDevice_| is an object coming from AVFoundation, used only to be
  // plugged in |captureDeviceInput_| and to query for session preset support.
  base::scoped_nsobject<AVCaptureDevice> _captureDevice;
  base::scoped_nsobject<AVCaptureDeviceInput> _captureDeviceInput;
  base::scoped_nsobject<AVCaptureVideoDataOutput> _captureVideoDataOutput;

  // When enabled, converts captured frames to NV12.
  std::unique_ptr<media::SampleBufferTransformer> _sampleBufferTransformer;
  // Transformers used to create downscaled versions of the captured image.
  // Enabled when setScaledResolutions is called (i.e. media::VideoFrameFeedback
  // asks for scaled frames on behalf of a consumer in the Renderer process),
  // NV12 output is enabled and the kInCapturerScaling feature is on.
  std::vector<std::unique_ptr<media::SampleBufferTransformer>>
      _scaledFrameTransformers;

  // An AVDataOutput specialized for taking pictures out of |captureSession_|.
  base::scoped_nsobject<AVCaptureStillImageOutput> _stillImageOutput;
  size_t _takePhotoStartedCount;
  size_t _takePhotoPendingCount;
  size_t _takePhotoCompletedCount;
  bool _stillImageOutputWarmupCompleted;
  std::unique_ptr<base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>
      _weakPtrFactoryForTakePhoto;

  // For testing.
  base::RepeatingCallback<void()> _onStillImageOutputStopped;

  scoped_refptr<base::SingleThreadTaskRunner> _mainThreadTaskRunner;
}

// This function translates Mac Core Video pixel formats to Chromium pixel
// formats. This implementation recognizes NV12.
+ (media::VideoPixelFormat)FourCCToChromiumPixelFormat:(FourCharCode)code;

- (void)setOnStillImageOutputStoppedForTesting:
    (base::RepeatingCallback<void()>)onStillImageOutputStopped;

// Use the below only for test.
- (void)callLocked:(base::OnceClosure)lambda;

- (void)processPixelBufferNV12IOSurface:(CVPixelBufferRef)pixelBuffer
                          captureFormat:
                              (const media::VideoCaptureFormat&)captureFormat
                             colorSpace:(const gfx::ColorSpace&)colorSpace
                              timestamp:(const base::TimeDelta)timestamp;

- (BOOL)processPixelBufferPlanes:(CVImageBufferRef)pixelBuffer
                   captureFormat:(const media::VideoCaptureFormat&)captureFormat
                      colorSpace:(const gfx::ColorSpace&)colorSpace
                       timestamp:(const base::TimeDelta)timestamp;

@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

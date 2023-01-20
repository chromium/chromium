// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

#include "base/memory/raw_ptr.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include "base/functional/callback_forward.h"
#include "base/task/single_thread_task_runner.h"

#include "base/mac/scoped_dispatch_object.h"
#include "base/mac/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video/mac/sample_buffer_transformer_mac.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceAVFoundationFrameReceiver {
 public:
  virtual ~VideoCaptureDeviceAVFoundationFrameReceiver() = default;

  // Called to deliver captured video frames.  It's safe to call this method
  // from any thread, including those controlled by AVFoundation.
  virtual void ReceiveFrame(const uint8_t* video_frame,
                            int video_frame_length,
                            const VideoCaptureFormat& frame_format,
                            const gfx::ColorSpace color_space,
                            int aspect_numerator,
                            int aspect_denominator,
                            base::TimeDelta timestamp) = 0;

  // Called to deliver GpuMemoryBuffer-wrapped captured video frames. This
  // function may be called from any thread, including those controlled by
  // AVFoundation.
  virtual void ReceiveExternalGpuMemoryBufferFrame(
      CapturedExternalVideoBuffer frame,
      std::vector<CapturedExternalVideoBuffer> scaled_frames,
      base::TimeDelta timestamp) = 0;

  // Callbacks with the result of a still image capture, or in case of error,
  // respectively. It's safe to call these methods from any thread.
  virtual void OnPhotoTaken(const uint8_t* image_data,
                            size_t image_length,
                            const std::string& mime_type) = 0;

  // Callback when a call to takePhoto fails.
  virtual void OnPhotoError() = 0;

  // Forwarder to VideoCaptureDevice::Client::OnError().
  virtual void ReceiveError(VideoCaptureError error,
                            const base::Location& from_here,
                            const std::string& reason) = 0;

  // Forwarder to VideoCaptureDevice::Client::OnCaptureConfigurationChanged().
  virtual void ReceiveCaptureConfigurationChanged() = 0;
};

// When this feature is enabled, the capturer can be configured using
// setScaledResolutions to output scaled versions of the captured frame (in
// addition to the original frame), whenever NV12 IOSurfaces are available to
// the capturer. These are available either when the camera supports it and
// kAVFoundationCaptureV2ZeroCopy is enabled or when kInCaptureConvertToNv12 is
// used to convert frames to NV12.
CAPTURE_EXPORT BASE_DECLARE_FEATURE(kInCapturerScaling);

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
                AVCapturePhotoCaptureDelegate> {
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
  // Used to avoid UAF in -captureOutput.
  base::Lock _destructionLock;
  raw_ptr<media::VideoCaptureDeviceAVFoundationFrameReceiver> _frameReceiver
      GUARDED_BY(_lock);  // weak.
  bool _capturedFirstFrame GUARDED_BY(_lock);
  bool _capturedFrameSinceLastStallCheck GUARDED_BY(_lock);
  std::unique_ptr<base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>
      _weakPtrFactoryForStallCheck;
  // Timestamp offset to subtract from all frames, to avoid leaking uptime.
  base::TimeDelta start_timestamp_;

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
  // Enabled when setScaledResolutions is called (i.e.
  // media::VideoCaptureFeedback asks for scaled frames on behalf of a consumer
  // in the Renderer process), NV12 output is enabled and the kInCapturerScaling
  // feature is on.
  std::vector<std::unique_ptr<media::SampleBufferTransformer>>
      _scaledFrameTransformers;

  // On macOS 10.15 or later, this has type AVCapturePhotoOutput.
  // On earlier versions, this has type AVCaptureStillImageOutput.
  // You say tomato, I say potato.
  base::scoped_nsobject<id> _photoOutput;
  // Only accessed on the main thread. The takePhoto() operation is considered
  // pending until we're ready to take another photo, which involves a PostTask
  // back to the main thread after the photo was taken.
  size_t _pendingTakePhotos;
  std::unique_ptr<base::WeakPtrFactory<VideoCaptureDeviceAVFoundation>>
      _weakPtrFactoryForTakePhoto;

  // For testing.
  base::RepeatingCallback<void()> _onPhotoOutputStopped;
  bool _forceLegacyStillImageApi;
  absl::optional<bool> _isPortraitEffectSupportedForTesting;
  absl::optional<bool> _isPortraitEffectActiveForTesting;

  scoped_refptr<base::SingleThreadTaskRunner> _mainThreadTaskRunner;
}

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

// If an efficient path is available, the capturer will perform scaling and
// deliver scaled frames to the |frameReceiver| as specified by |resolutions|.
// The scaled frames are delivered in addition to the original captured frame.
// Resolutions that match the captured frame or that would result in upscaling
// are ignored.
- (void)setScaledResolutions:(std::vector<gfx::Size>)resolutions;

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
- (void)setForceLegacyStillImageApiForTesting:(bool)forceLegacyApi;

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

// Returns whether the format supports the Portrait Effect feature or not.
- (bool)isPortraitEffectSupported;

// Returns whether the Portrait Effect is active on a device or not.
- (bool)isPortraitEffectActive;

- (void)setIsPortraitEffectSupportedForTesting:
    (bool)isPortraitEffectSupportedForTesting;
- (void)setIsPortraitEffectActiveForTesting:
    (bool)isPortraitEffectActiveForTesting;

@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

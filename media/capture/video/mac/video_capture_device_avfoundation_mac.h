// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace media {
class VideoCaptureDeviceMac;

class CAPTURE_EXPORT VideoCaptureDeviceAVFoundationFrameReceiver {
 public:
  virtual ~VideoCaptureDeviceAVFoundationFrameReceiver() = default;

  virtual void ReceiveFrame(const uint8_t* video_frame,
                            int video_frame_length,
                            const VideoCaptureFormat& frame_format,
                            const gfx::ColorSpace color_space,
                            int aspect_numerator,
                            int aspect_denominator,
                            base::TimeDelta timestamp) = 0;
  virtual void OnPhotoTaken(const uint8_t* image_data,
                            size_t image_length,
                            const std::string& mime_type) = 0;
  virtual void OnPhotoError() = 0;
  virtual void ReceiveError(VideoCaptureError error,
                            const base::Location& from_here,
                            const std::string& reason) = 0;
};

}  // namespace media

// Class used by VideoCaptureDeviceMac (VCDM) for video and image capture using
// AVFoundation API. This class lives inside the thread created by its owner
// VCDM.
//
//  * Previous to any use, clients (VCDM) must call -initWithFrameReceiver: to
//    initialise an object of this class and register a |frameReceiver_|.
//  * Frame receiver registration or removal can also happen via explicit call
//    to -setFrameReceiver:. Re-registrations are safe and allowed, even during
//    capture using this method.
//  * Method -setCaptureDevice: must be called at least once with a device
//    identifier from +deviceNames. Creates all the necessary AVFoundation
//    objects on first call; it connects them ready for capture every time.
//    This method should not be called during capture (i.e. between
//    -startCapture and -stopCapture).
//  * -setCaptureWidth:height:frameRate: is called if a resolution or frame rate
//    different than the by default one set by -setCaptureDevice: is needed.
//    This method should not be called during capture. This method must be
//    called after -setCaptureDevice:.
//  * -startCapture registers the notification listeners and starts the
//    capture. The capture can be stop using -stopCapture. The capture can be
//    restarted and restoped multiple times, reconfiguring or not the device in
//    between.
//  * -setCaptureDevice can be called with a |nil| value, case in which it stops
//    the capture and disconnects the library objects. This step is not
//    necessary.
//  * Deallocation of the library objects happens gracefully on destruction of
//    the VideoCaptureDeviceAVFoundation object.
//
//
CAPTURE_EXPORT
@interface VideoCaptureDeviceAVFoundation
    : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
 @private
  // The following attributes are set via -setCaptureHeight:width:frameRate:.
  int _frameWidth;
  int _frameHeight;
  float _frameRate;

  // The capture format that best matches the above attributes.
  base::scoped_nsobject<AVCaptureDeviceFormat> _bestCaptureFormat;

  base::Lock _lock;  // Protects concurrent setting and using |frameReceiver_|.
  media::VideoCaptureDeviceAVFoundationFrameReceiver* _frameReceiver;  // weak.

  base::scoped_nsobject<AVCaptureSession> _captureSession;

  // |captureDevice_| is an object coming from AVFoundation, used only to be
  // plugged in |captureDeviceInput_| and to query for session preset support.
  base::scoped_nsobject<AVCaptureDevice> _captureDevice;
  base::scoped_nsobject<AVCaptureDeviceInput> _captureDeviceInput;
  base::scoped_nsobject<AVCaptureVideoDataOutput> _captureVideoDataOutput;

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

// Initializes the instance and the underlying capture session and registers the
// frame receiver.
- (id)initWithFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver;

// Sets the frame receiver.
- (void)setFrameReceiver:
    (media::VideoCaptureDeviceAVFoundationFrameReceiver*)frameReceiver;

// Sets which capture device to use by name, retrieved via |deviceNames|. Once
// the deviceId is known, the library objects are created if needed and
// connected for the capture, and a by default resolution is set. If deviceId is
// nil, then the eventual capture is stopped and library objects are
// disconnected. Returns YES on success, NO otherwise. If the return value is
// NO, an error message is assigned to |outMessage|. This method should not be
// called during capture.
- (BOOL)setCaptureDevice:(NSString*)deviceId
            errorMessage:(NSString**)outMessage;

// Configures the capture properties for the capture session and the video data
// output; this means it MUST be called after setCaptureDevice:. Return YES on
// success, else NO.
- (BOOL)setCaptureHeight:(int)height
                   width:(int)width
               frameRate:(float)frameRate;

// Starts video capturing and register the notification listeners. Must be
// called after setCaptureDevice:, and, eventually, also after
// setCaptureHeight:width:frameRate:. Returns YES on success, NO otherwise.
- (BOOL)startCapture;

// Stops video capturing and stops listening to notifications.
- (void)stopCapture;

// Takes a photo. This method should only be called between -startCapture and
// -stopCapture.
- (void)takePhoto;

- (void)setOnStillImageOutputStoppedForTesting:
    (base::RepeatingCallback<void()>)onStillImageOutputStopped;

@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

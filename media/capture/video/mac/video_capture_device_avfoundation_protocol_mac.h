// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_PROTOCOL_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_PROTOCOL_MAC_H_

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class VideoCaptureDeviceMac;

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
};
}  // namespace media

// Protocol used by VideoCaptureDeviceMac for video and image capture using
// AVFoundation API. Concrete implementation objects live inside the thread
// created by its owner VideoCaptureDeviceMac.
@protocol VideoCaptureDeviceAVFoundationProtocol

// Previous to any use, clients must call -initWithFrameReceiver: to
// initialise an object of this class and register a |frameReceiver_|. This
// initializes the instance and the underlying capture session and registers the
// frame receiver.
- (id)initWithFrameReceiver:
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
@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_PROTOCOL_MAC_H_

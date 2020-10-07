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
#import "media/capture/video/mac/video_capture_device_avfoundation_protocol_mac.h"
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

  // Protects concurrent setting and using |frameReceiver_|. Note that the
  // GUARDED_BY decoration below does not have any effect.
  base::Lock _lock;
  media::VideoCaptureDeviceAVFoundationFrameReceiver* _frameReceiver
      GUARDED_BY(_lock);  // weak.

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

// This function translates Mac Core Video pixel formats to Chromium pixel
// formats. This implementation recognizes NV12.
+ (media::VideoPixelFormat)FourCCToChromiumPixelFormat:(FourCharCode)code;

- (void)setOnStillImageOutputStoppedForTesting:
    (base::RepeatingCallback<void()>)onStillImageOutputStopped;

@end

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_AVFOUNDATION_MAC_H_

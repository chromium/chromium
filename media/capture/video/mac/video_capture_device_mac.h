// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MacOSX implementation of generic VideoCaptureDevice, using AVFoundation as
// native capture API. AVFoundation is available in versions 10.7 (Lion) and
// later.

#ifndef MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_
#define MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#import "media/capture/video/mac/video_capture_device_avfoundation_mac.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video_capture_types.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace base {
class Location;
}  // namespace base

// Small class to bundle device name and connection type into a dictionary.
CAPTURE_EXPORT
@interface DeviceNameAndTransportType : NSObject {
 @private
  base::scoped_nsobject<NSString> _deviceName;
  // The transport type of the device (USB, PCI, etc), values are defined in
  // <IOKit/audio/IOAudioTypes.h> as kIOAudioDeviceTransportType*.
  int32_t _transportType;
}

- (instancetype)initWithName:(NSString*)name
               transportType:(int32_t)transportType;

- (NSString*)deviceName;
- (int32_t)transportType;
@end

namespace media {

// Called by VideoCaptureManager to open, close and start, stop Mac video
// capture devices.
class VideoCaptureDeviceMac
    : public VideoCaptureDevice,
      public VideoCaptureDeviceAVFoundationFrameReceiver {
 public:
  explicit VideoCaptureDeviceMac(
      const VideoCaptureDeviceDescriptor& device_descriptor);

  VideoCaptureDeviceMac(const VideoCaptureDeviceMac&) = delete;
  VideoCaptureDeviceMac& operator=(const VideoCaptureDeviceMac&) = delete;

  ~VideoCaptureDeviceMac() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(
      const VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  // VideoFrameConsumerFeedbackObserver implementation.
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override;

  bool Init(VideoCaptureApi capture_api_type);

  // VideoCaptureDeviceAVFoundationFrameReceiver:
  void ReceiveFrame(const uint8_t* video_frame,
                    int video_frame_length,
                    const VideoCaptureFormat& frame_format,
                    const gfx::ColorSpace color_space,
                    int aspect_numerator,
                    int aspect_denominator,
                    base::TimeDelta timestamp) override;
  void ReceiveExternalGpuMemoryBufferFrame(
      CapturedExternalVideoBuffer frame,
      std::vector<CapturedExternalVideoBuffer> scaled_frames,
      base::TimeDelta timestamp) override;
  void OnPhotoTaken(const uint8_t* image_data,
                    size_t image_length,
                    const std::string& mime_type) override;
  void OnPhotoError() override;
  void ReceiveError(VideoCaptureError error,
                    const base::Location& from_here,
                    const std::string& reason) override;

  // Forwarder to VideoCaptureDevice::Client::OnLog().
  void LogMessage(const std::string& message);

  static std::string GetDeviceModelId(const std::string& device_id,
                                      VideoCaptureApi capture_api,
                                      VideoCaptureTransportType transport_type);

  static VideoCaptureControlSupport GetControlSupport(
      const std::string& device_model);

 private:
  void SetErrorState(VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason);
  bool UpdateCaptureResolution();

  // Flag indicating the internal state.
  enum InternalState { kNotInitialized, kIdle, kCapturing, kError };

  VideoCaptureDeviceDescriptor device_descriptor_;
  std::unique_ptr<VideoCaptureDevice::Client> client_;

  VideoCaptureFormat capture_format_;

  // Only read and write state_ from inside this loop.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  InternalState state_;

  base::scoped_nsobject<VideoCaptureDeviceAVFoundation> capture_device_;

  // To hold on to the TakePhotoCallback while the picture is being taken.
  TakePhotoCallback photo_callback_;

  // Used with Bind and PostTask to ensure that methods aren't called after the
  // VideoCaptureDeviceMac is destroyed.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<VideoCaptureDeviceMac> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MAC_VIDEO_CAPTURE_DEVICE_MAC_H_

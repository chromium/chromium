// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Windows specific implementation of VideoCaptureDevice. DirectShow is used for
// capturing. DirectShow provide its own threads for capturing.

#ifndef MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_WIN_H_
#define MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_WIN_H_

// Avoid including strsafe.h via dshow as it will cause build warnings.
#define NO_DSHOW_STRSAFE
#include <dshow.h>
#include <stdint.h>
#include <vidcap.h>
#include <wrl/client.h>

#include <map>
#include <optional>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/win/capability_list_win.h"
#include "media/capture/video/win/sink_filter_win.h"
#include "media/capture/video/win/sink_input_pin_win.h"
#include "media/capture/video_capture_types.h"

namespace base {
class Location;
}  // namespace base

namespace media {

// All the methods in the class can only be run on a COM initialized thread.
class VideoCaptureDeviceWin : public VideoCaptureDevice,
                              public SinkFilterObserver {
 public:
  // A utility class that wraps the AM_MEDIA_TYPE type and guarantees that
  // we free the structure when exiting the scope.  DCHECKing is also done to
  // avoid memory leaks.
  class ScopedMediaType {
   public:
    ScopedMediaType() : media_type_(nullptr) {}
    ~ScopedMediaType() { Free(); }

    AM_MEDIA_TYPE* operator->() { return media_type_; }
    AM_MEDIA_TYPE* get() { return media_type_; }
    void Free();
    raw_ptr<AM_MEDIA_TYPE>* Receive();

   private:
    void FreeMediaType(AM_MEDIA_TYPE* mt);
    void DeleteMediaType(AM_MEDIA_TYPE* mt);

    raw_ptr<AM_MEDIA_TYPE> media_type_;
  };

  static VideoCaptureControlSupport GetControlSupport(
      Microsoft::WRL::ComPtr<IBaseFilter> capture_filter);
  static void GetDeviceCapabilityList(
      Microsoft::WRL::ComPtr<IBaseFilter> capture_filter,
      bool query_detailed_frame_rates,
      CapabilityList* out_capability_list);
  static void GetPinCapabilityList(
      Microsoft::WRL::ComPtr<IBaseFilter> capture_filter,
      Microsoft::WRL::ComPtr<IPin> output_capture_pin,
      bool query_detailed_frame_rates,
      CapabilityList* out_capability_list);
  static Microsoft::WRL::ComPtr<IPin> GetPin(
      Microsoft::WRL::ComPtr<IBaseFilter> capture_filter,
      PIN_DIRECTION pin_dir,
      REFGUID category,
      REFGUID major_type);
  static VideoPixelFormat TranslateMediaSubtypeToPixelFormat(
      const GUID& sub_type);

  VideoCaptureDeviceWin() = delete;

  VideoCaptureDeviceWin(const VideoCaptureDeviceDescriptor& device_descriptor,
                        Microsoft::WRL::ComPtr<IBaseFilter> capture_filter);

  VideoCaptureDeviceWin(const VideoCaptureDeviceWin&) = delete;
  VideoCaptureDeviceWin& operator=(const VideoCaptureDeviceWin&) = delete;

  ~VideoCaptureDeviceWin() override;

  // Opens the device driver for this device.
  bool Init();

  // VideoCaptureDevice implementation.
  void AllocateAndStart(
      const VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client) override;
  void StopAndDeAllocate() override;
  void TakePhoto(TakePhotoCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;

 private:
  enum InternalState {
    kIdle,       // The device driver is opened but camera is not in use.
    kCapturing,  // Video is being captured.
    kError       // Error accessing HW functions.
                 // User needs to recover by destroying the object.
  };

  static bool GetCameraAndVideoControls(
      Microsoft::WRL::ComPtr<IBaseFilter> capture_filter,
      ICameraControl** camera_control,
      IVideoProcAmp** video_control);

  // Implements SinkFilterObserver.
  void FrameReceived(const uint8_t* buffer,
                     int length,
                     const VideoCaptureFormat& format,
                     base::TimeDelta timestamp,
                     bool flip_y) override;
  void FrameDropped(VideoCaptureFrameDropReason reason) override;

  bool CreateCapabilityMap();
  void SetAntiFlickerInCaptureFilter(const VideoCaptureParams& params);
  void SetErrorState(media::VideoCaptureError error,
                     const base::Location& from_here,
                     const std::string& reason,
                     HRESULT hr);

  const VideoCaptureDeviceDescriptor device_descriptor_;
  InternalState state_;
  std::unique_ptr<VideoCaptureDevice::Client> client_;

  Microsoft::WRL::ComPtr<IBaseFilter> capture_filter_;

  Microsoft::WRL::ComPtr<IGraphBuilder> graph_builder_;
  Microsoft::WRL::ComPtr<ICaptureGraphBuilder2> capture_graph_builder_;

  Microsoft::WRL::ComPtr<IMediaControl> media_control_;
  Microsoft::WRL::ComPtr<IPin> input_sink_pin_;
  Microsoft::WRL::ComPtr<IPin> output_capture_pin_;

  scoped_refptr<SinkFilter> sink_filter_;

  // Map of all capabilities this device support.
  CapabilityList capabilities_;

  VideoCaptureFormat capture_format_;

  Microsoft::WRL::ComPtr<ICameraControl> camera_control_;
  Microsoft::WRL::ComPtr<IVideoProcAmp> video_control_;
  // These flags keep the manual/auto mode between cycles of SetPhotoOptions().
  bool white_balance_mode_manual_;
  bool exposure_mode_manual_;
  bool focus_mode_manual_;

  base::TimeTicks first_ref_time_;

  base::queue<TakePhotoCallback> take_photo_callbacks_;

  base::ThreadChecker thread_checker_;

  // Used to guard between race checking capture state between the thread used
  // in |thread_checker_| and a thread used in
  // |SinkFilterObserver::SinkFilterObserver| callbacks.
  base::Lock lock_;

  bool enable_get_photo_state_;

  std::optional<int> camera_rotation_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_WIN_VIDEO_CAPTURE_DEVICE_WIN_H_

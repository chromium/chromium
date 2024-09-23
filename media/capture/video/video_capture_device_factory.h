// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_info.h"

#if BUILDFLAG(IS_WIN)
#include "media/base/win/dxgi_device_manager.h"
#endif

namespace media {
// VideoCaptureErrorOrDevice stores the result of CreateDevice function. This is
// designed to pass information such that when device creation fails, instead of
// returning a null_ptr, this would store an error_code explaining why the
// creation failed.
class CAPTURE_EXPORT VideoCaptureErrorOrDevice {
 public:
  explicit VideoCaptureErrorOrDevice(
      std::unique_ptr<VideoCaptureDevice> video_device);
  explicit VideoCaptureErrorOrDevice(VideoCaptureError err_code);
  ~VideoCaptureErrorOrDevice();

  VideoCaptureErrorOrDevice(VideoCaptureErrorOrDevice&& other);

  bool ok() const { return error_code_ == VideoCaptureError::kNone; }
  VideoCaptureError error() const { return error_code_; }

  std::unique_ptr<VideoCaptureDevice> ReleaseDevice();

 private:
  std::unique_ptr<VideoCaptureDevice> device_;
  VideoCaptureError error_code_;
};

// VideoCaptureDeviceFactory is the base class for creation of video capture
// devices in the different platforms. VCDFs are created by MediaStreamManager
// on UI thread and plugged into VideoCaptureManager, who owns and operates them
// in Device Thread (a.k.a. Audio Thread).
// Typical operation is to first call GetDevicesInfo() to obtain information
// about available devices. The obtained descriptors can then be used to either
// obtain the supported formats of a device using GetSupportedFormats(), or to
// create an instance of VideoCaptureDevice for the device using CreateDevice().
// TODO(chfremer): Add a layer on top of the platform-specific implementations
// that uses strings instead of descriptors as keys for accessing devices.
// crbug.com/665065
class CAPTURE_EXPORT VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactory();

  VideoCaptureDeviceFactory(const VideoCaptureDeviceFactory&) = delete;
  VideoCaptureDeviceFactory& operator=(const VideoCaptureDeviceFactory&) =
      delete;

  virtual ~VideoCaptureDeviceFactory();

  // Return type is VideoCaptureErrorOrDevice which can be used to access a
  // VideoCaptureDevice, if device creation is successful, or a
  // VideoCaptureError, if something goes wrong.
  virtual VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) = 0;

  // Enumerates video capture devices and passes the results to the supplied
  // |callback|. The callback is called synchronously or asynchronously on the
  // same thread. The callback is guaranteed not to be called after the factory
  // is destroyed.
  using GetDevicesInfoCallback = base::OnceCallback<void(
      std::vector<VideoCaptureDeviceInfo> devices_info)>;
  virtual void GetDevicesInfo(GetDevicesInfoCallback callback) = 0;

#if BUILDFLAG(IS_WIN)
  // Returns used DXGI device manager.
  // This is used for testing and to allow sharing the same DXGI device manager
  // with GpuMemoryBufferTracker in VideoCaptureBufferPool. Default
  // implementation always returns nullptr. Should be overridden by actual
  // factory implementation on Windows.
  virtual scoped_refptr<DXGIDeviceManager> GetDxgiDeviceManager();

  // Default implementation does nothing. Should be overridden by actual
  // factory implementation on Windows.
  virtual void OnGpuInfoUpdate(const CHROME_LUID& luid);
#endif

 protected:
  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_

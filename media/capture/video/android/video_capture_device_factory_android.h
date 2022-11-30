// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_ANDROID_VIDEO_CAPTURE_DEVICE_FACTORY_ANDROID_H_
#define MEDIA_CAPTURE_VIDEO_ANDROID_VIDEO_CAPTURE_DEVICE_FACTORY_ANDROID_H_

#include "media/capture/video/video_capture_device_factory.h"

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// VideoCaptureDeviceFactory on Android. This class implements the static
// VideoCapture methods and the factory of VideoCaptureAndroid.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryAndroid
    : public VideoCaptureDeviceFactory {
 public:
  static base::android::ScopedJavaLocalRef<jobject> createVideoCaptureAndroid(
      int id,
      jlong nativeVideoCaptureDeviceAndroid);

  VideoCaptureDeviceFactoryAndroid();

  VideoCaptureDeviceFactoryAndroid(const VideoCaptureDeviceFactoryAndroid&) =
      delete;
  VideoCaptureDeviceFactoryAndroid& operator=(
      const VideoCaptureDeviceFactoryAndroid&) = delete;

  ~VideoCaptureDeviceFactoryAndroid() override;

  VideoCaptureErrorOrDevice CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDevicesInfo(GetDevicesInfoCallback callback) override;

  static bool IsLegacyOrDeprecatedDevice(const std::string& device_id);

  // Configures all subsequent CreateDevice()s in test mode.
  void ConfigureForTesting() { test_mode_ = true; }

 private:
  VideoCaptureFormats GetSupportedFormats(int device_index,
                                          const std::string& display_name);

  // Switch to indicate that all created Java capturers will be in test mode.
  bool test_mode_ = false;

  // VideoCaptureFormats and zooms are cached, so GetSupportedFormats() and
  // Java_VideoCaptureFactory_isZoomSupported() respectively don't need to be
  // called for every device every time GetDevicesInfo() is called. It also
  // allows to workaround bugs on some devices that don't handle the case when
  // an actively used camera is opened again (see https://crbug.com/1138608).
  base::flat_map<std::string, VideoCaptureFormats> supported_formats_cache_;
  base::flat_map<std::string, bool> zooms_cache_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_ANDROID_VIDEO_CAPTURE_DEVICE_FACTORY_ANDROID_H_

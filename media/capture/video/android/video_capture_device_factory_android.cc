// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/video_capture_device_factory_android.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/android/capture_jni_headers/VideoCaptureFactory_jni.h"
#include "media/capture/video/android/video_capture_device_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
ScopedJavaLocalRef<jobject>
VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
    int id,
    jlong nativeVideoCaptureDeviceAndroid) {
  return (Java_VideoCaptureFactory_createVideoCapture(
      AttachCurrentThread(), id, nativeVideoCaptureDeviceAndroid));
}

std::unique_ptr<VideoCaptureDevice>
VideoCaptureDeviceFactoryAndroid::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device_descriptor.device_id, &id))
    return std::unique_ptr<VideoCaptureDevice>();

  std::unique_ptr<VideoCaptureDeviceAndroid> video_capture_device(
      new VideoCaptureDeviceAndroid(device_descriptor));

  if (video_capture_device->Init()) {
    if (test_mode_)
      video_capture_device->ConfigureForTesting();
    return std::move(video_capture_device);
  }

  DLOG(ERROR) << "Error creating Video Capture Device.";
  return nullptr;
}

void VideoCaptureDeviceFactoryAndroid::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_descriptors->clear();

  JNIEnv* env = AttachCurrentThread();

  const int num_cameras = Java_VideoCaptureFactory_getNumberOfCameras(env);
  DVLOG(1) << __func__ << ": num_cameras=" << num_cameras;
  if (num_cameras <= 0)
    return;

  for (int camera_id = num_cameras - 1; camera_id >= 0; --camera_id) {
    base::android::ScopedJavaLocalRef<jstring> device_name =
        Java_VideoCaptureFactory_getDeviceName(env, camera_id);
    if (device_name.obj() == NULL)
      continue;

    const int capture_api_type =
        Java_VideoCaptureFactory_getCaptureApiType(env, camera_id);
    const int facing_mode =
        Java_VideoCaptureFactory_getFacingMode(env, camera_id);
    const std::string display_name =
        base::android::ConvertJavaStringToUTF8(device_name);
    const std::string device_id = base::NumberToString(camera_id);

    // Android cameras are not typically USB devices, and the model_id is
    // currently only used for USB model identifiers, so this implementation
    // just indicates an unknown device model (by not providing one).
    VideoCaptureDeviceDescriptor descriptor(
        display_name, device_id, "" /*model_id*/,
        static_cast<VideoCaptureApi>(capture_api_type),
        VideoCaptureTransportType::OTHER_TRANSPORT,
        static_cast<VideoFacingMode>(facing_mode));

    // We put user-facing devices to the front of the list in order to make
    // them by-default preferred over environment-facing ones when no other
    // constraints for device selection are given.
    if (facing_mode == MEDIA_VIDEO_FACING_USER)
      device_descriptors->insert(device_descriptors->begin(),
                                 std::move(descriptor));
    else
      device_descriptors->emplace_back(std::move(descriptor));

    DVLOG(1) << __func__ << ": camera "
             << "device_name=" << display_name << ", unique_id=" << device_id;
  }
}

void VideoCaptureDeviceFactoryAndroid::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* capture_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device.device_id, &id))
    return;
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> collected_formats =
      Java_VideoCaptureFactory_getDeviceSupportedFormats(env, id);
  if (collected_formats.is_null())
    return;

  for (auto format : collected_formats.ReadElements<jobject>()) {
    VideoPixelFormat pixel_format = PIXEL_FORMAT_UNKNOWN;
    switch (Java_VideoCaptureFactory_getCaptureFormatPixelFormat(env, format)) {
      case VideoCaptureDeviceAndroid::ANDROID_IMAGE_FORMAT_YV12:
        pixel_format = PIXEL_FORMAT_YV12;
        break;
      case VideoCaptureDeviceAndroid::ANDROID_IMAGE_FORMAT_NV21:
        pixel_format = PIXEL_FORMAT_NV21;
        break;
      case VideoCaptureDeviceAndroid::ANDROID_IMAGE_FORMAT_YUV_420_888:
        pixel_format = PIXEL_FORMAT_I420;
        break;
      default:
        // TODO(crbug.com/792260): break here and let the enumeration continue
        // with UNKNOWN pixel format because the platform doesn't know until
        // capture, but some unrelated tests timeout https://crbug.com/644910.
        continue;
    }
    VideoCaptureFormat capture_format(
        gfx::Size(Java_VideoCaptureFactory_getCaptureFormatWidth(env, format),
                  Java_VideoCaptureFactory_getCaptureFormatHeight(env, format)),
        Java_VideoCaptureFactory_getCaptureFormatFramerate(env, format),
        pixel_format);
    capture_formats->push_back(capture_format);
    DVLOG(1) << device.display_name() << " "
             << VideoCaptureFormat::ToString(capture_format);
  }
}

bool VideoCaptureDeviceFactoryAndroid::IsLegacyOrDeprecatedDevice(
    const std::string& device_id) {
  int id;
  if (!base::StringToInt(device_id, &id))
    return true;
  return (Java_VideoCaptureFactory_isLegacyOrDeprecatedDevice(
      AttachCurrentThread(), id));
}

}  // namespace media

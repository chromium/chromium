// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/video_capture_device_factory_android.h"

#include <utility>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "media/capture/video/android/video_capture_device_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/capture/video/android/capture_jni_headers/VideoCaptureFactory_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace media {

// static
ScopedJavaLocalRef<jobject>
VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
    int id,
    jlong nativeVideoCaptureDeviceAndroid) {
  return (Java_VideoCaptureFactory_createVideoCapture(
      AttachCurrentThread(), id, nativeVideoCaptureDeviceAndroid));
}

VideoCaptureDeviceFactoryAndroid::VideoCaptureDeviceFactoryAndroid() = default;
VideoCaptureDeviceFactoryAndroid::~VideoCaptureDeviceFactoryAndroid() = default;

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryAndroid::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device_descriptor.device_id, &id))
    return VideoCaptureErrorOrDevice(
        VideoCaptureError::
            kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested);

  auto video_capture_device =
      std::make_unique<VideoCaptureDeviceAndroid>(device_descriptor);

  if (video_capture_device->Init()) {
    if (test_mode_)
      video_capture_device->ConfigureForTesting();
    return VideoCaptureErrorOrDevice(std::move(video_capture_device));
  }

  DLOG(ERROR) << "Error creating Video Capture Device.";
  return VideoCaptureErrorOrDevice(
      VideoCaptureError::kAndroidApi2ErrorConfiguringCamera);
}

void VideoCaptureDeviceFactoryAndroid::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = AttachCurrentThread();

  const int num_cameras = Java_VideoCaptureFactory_getNumberOfCameras(env);
  DVLOG(1) << __func__ << ": num_cameras=" << num_cameras;
  if (num_cameras <= 0) {
    std::move(callback).Run({});
    return;
  }

  std::vector<VideoCaptureDeviceInfo> devices_info;
  for (int camera_index = num_cameras - 1; camera_index >= 0; --camera_index) {
    base::android::ScopedJavaLocalRef<jstring> device_name =
        Java_VideoCaptureFactory_getDeviceName(env, camera_index);
    if (device_name.obj() == nullptr)
      continue;
    const std::string display_name =
        base::android::ConvertJavaStringToUTF8(device_name);

    base::android::ScopedJavaLocalRef<jstring> device_id_jstring =
        Java_VideoCaptureFactory_getDeviceId(env, camera_index);
    if (device_id_jstring.obj() == nullptr)
      continue;
    const std::string device_id =
        base::android::ConvertJavaStringToUTF8(device_id_jstring);

    const VideoCaptureApi capture_api_type = static_cast<VideoCaptureApi>(
        Java_VideoCaptureFactory_getCaptureApiType(env, camera_index));
    if (capture_api_type == VideoCaptureApi::UNKNOWN)
      continue;
    VideoCaptureControlSupport control_support;
    const int facing_mode =
        Java_VideoCaptureFactory_getFacingMode(env, camera_index);

    auto zoom_it = zooms_cache_.find(device_id);
    if (zoom_it != zooms_cache_.end()) {
      control_support.zoom = zoom_it->second;
    } else {
      control_support.zoom =
          Java_VideoCaptureFactory_isZoomSupported(env, camera_index);
      zooms_cache_.emplace(device_id, control_support.zoom);
    }

    // Android cameras are not typically USB devices, and the model_id is
    // currently only used for USB model identifiers, so this implementation
    // just indicates an unknown device model (by not providing one).
    VideoCaptureDeviceInfo device_info(VideoCaptureDeviceDescriptor(
        display_name, device_id, "" /*model_id*/, capture_api_type,
        control_support, VideoCaptureTransportType::OTHER_TRANSPORT,
        static_cast<VideoFacingMode>(facing_mode)));

    auto it = supported_formats_cache_.find(device_id);
    if (it != supported_formats_cache_.end()) {
      device_info.supported_formats = it->second;
    } else {
      device_info.supported_formats =
          GetSupportedFormats(camera_index, display_name);
      supported_formats_cache_.emplace(device_id,
                                       device_info.supported_formats);
    }

    // We put user-facing devices to the front of the list in order to make
    // them by-default preferred over environment-facing ones when no other
    // constraints for device selection are given.
    if (facing_mode == MEDIA_VIDEO_FACING_USER) {
      devices_info.insert(devices_info.begin(), std::move(device_info));
    } else {
      devices_info.emplace_back(std::move(device_info));
    }

    DVLOG(1) << __func__ << ": camera "
             << "device_name=" << display_name << ", unique_id=" << device_id;
  }

  // Remove old entries from |supported_formats_cache_| if necessary.
  if (supported_formats_cache_.size() > devices_info.size()) {
    base::EraseIf(supported_formats_cache_, [&devices_info](const auto& entry) {
      return base::ranges::none_of(
          devices_info, [&entry](const VideoCaptureDeviceInfo& info) {
            return entry.first == info.descriptor.device_id;
          });
    });
  }

  // Remove old entries from |zooms_cache_| if necessary.
  if (zooms_cache_.size() > devices_info.size()) {
    base::EraseIf(zooms_cache_, [&devices_info](const auto& entry) {
      return base::ranges::none_of(
          devices_info, [&entry](const VideoCaptureDeviceInfo& info) {
            return entry.first == info.descriptor.device_id;
          });
    });
  }

  std::move(callback).Run(std::move(devices_info));
}

VideoCaptureFormats VideoCaptureDeviceFactoryAndroid::GetSupportedFormats(
    int device_index,
    const std::string& display_name) {
  DCHECK(thread_checker_.CalledOnValidThread());

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> collected_formats =
      Java_VideoCaptureFactory_getDeviceSupportedFormats(env, device_index);
  if (collected_formats.is_null())
    return {};

  VideoCaptureFormats capture_formats;
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
        // TODO(crbug.com/40553340): break here and let the enumeration continue
        // with UNKNOWN pixel format because the platform doesn't know until
        // capture, but some unrelated tests timeout https://crbug.com/644910.
        continue;
    }
    VideoCaptureFormat capture_format(
        gfx::Size(Java_VideoCaptureFactory_getCaptureFormatWidth(env, format),
                  Java_VideoCaptureFactory_getCaptureFormatHeight(env, format)),
        Java_VideoCaptureFactory_getCaptureFormatFramerate(env, format),
        pixel_format);
    DVLOG(1) << display_name << " "
             << VideoCaptureFormat::ToString(capture_format);
    capture_formats.push_back(std::move(capture_format));
  }

  return capture_formats;
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

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/ndk_video_encode_accelerator_svc_api.h"

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "media/base/android/media_jni_headers/VideoAcceleratorUtil_jni.h"
#include "media/base/media_switches.h"

namespace media {

// static
bool NdkVideoEncodeAcceleratorSvcApi::IsTemporalLayerEncodingEnabled() {
  static bool enabled = []() {
    JNIEnv* env = base::android::AttachCurrentThread();
    return Java_VideoAcceleratorUtil_isTemporalLayerEncodingEnabled(env);
  }();
  return enabled;
}

// static
const NdkVideoEncodeAcceleratorSvcApi* NdkVideoEncodeAcceleratorSvcApi::Get() {
  static const base::NoDestructor<NdkVideoEncodeAcceleratorSvcApi> api;
  return &*api;
}

NdkVideoEncodeAcceleratorSvcApi::NdkVideoEncodeAcceleratorSvcApi() {
  if (__builtin_available(android 37, *)) {
    base::NativeLibraryLoadError error;
    base::NativeLibrary lib =
        base::LoadNativeLibrary(base::FilePath("libmediandk.so"), &error);
    if (!lib) {
      DVLOG(1) << "Failed to load libmediandk.so: " << error.ToString();
      return;
    }

    AMediaCodecStore_getCodecInfo =
        reinterpret_cast<AMediaCodecStore_getCodecInfo_Type>(
            base::GetFunctionPointerFromNativeLibrary(
                lib, "AMediaCodecStore_getCodecInfo"));
    AMediaCodecInfo_getEncoderCapabilities =
        reinterpret_cast<AMediaCodecInfo_getEncoderCapabilities_Type>(
            base::GetFunctionPointerFromNativeLibrary(
                lib, "AMediaCodecInfo_getEncoderCapabilities"));
    ACodecEncoderCapabilities_getSupportedLayeringSchemas = reinterpret_cast<
        ACodecEncoderCapabilities_getSupportedLayeringSchemas_Type>(
        base::GetFunctionPointerFromNativeLibrary(
            lib, "ACodecEncoderCapabilities_getSupportedLayeringSchemas"));

    if (!AMediaCodecStore_getCodecInfo ||
        !AMediaCodecInfo_getEncoderCapabilities ||
        !ACodecEncoderCapabilities_getSupportedLayeringSchemas) {
      DVLOG(1)
          << "New NDK APIs for layering schemas not found (symbols missing).";
      AMediaCodecStore_getCodecInfo = nullptr;
      AMediaCodecInfo_getEncoderCapabilities = nullptr;
      ACodecEncoderCapabilities_getSupportedLayeringSchemas = nullptr;
    }
  }
}

// static
bool NdkVideoEncodeAcceleratorSvcApi::IsTemporalLayerIdSupported() {
  if (__builtin_available(android 37, *)) {
    return base::FeatureList::IsEnabled(
               media::kNdkVideoEncodeAcceleratorNativeSvc) &&
           IsTemporalLayerEncodingEnabled();
  }

  return false;
}

// static
bool NdkVideoEncodeAcceleratorSvcApi::IsBitrateLayeringSupported() {
  if (__builtin_available(android 37, *)) {
    return base::FeatureList::IsEnabled(
               media::kNdkVideoEncodeAcceleratorBitrateLayering) &&
           IsTemporalLayerEncodingEnabled();
  }

  return false;
}

}  // namespace media

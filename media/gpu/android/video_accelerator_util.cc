// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_accelerator_util.h"

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/VideoAcceleratorUtil_jni.h"

namespace media {

const std::vector<MediaCodecEncoderInfo>& GetEncoderInfoCache() {
  static const base::NoDestructor<std::vector<MediaCodecEncoderInfo>> infos([] {
    // Sadly the NDK doesn't provide a mechanism for accessing the equivalent of
    // the SDK's MediaCodecList, so we must call into Java to enumerate support.
    JNIEnv* env = jni_zero::AttachCurrentThread();
    CHECK(env);
    auto java_profiles =
        Java_VideoAcceleratorUtil_getSupportedEncoderProfiles(env);

    std::vector<MediaCodecEncoderInfo> cpp_infos;
    if (!java_profiles) {
      // Per histograms this happens ~0% of the time, so no need for fallback.
      return cpp_infos;
    }

    for (auto java_profile : java_profiles.ReadElements<jobject>()) {
      MediaCodecEncoderInfo info;
      info.profile.profile = static_cast<VideoCodecProfile>(
          Java_SupportedProfileAdapter_getProfile(env, java_profile));
      info.profile.min_resolution = gfx::Size(
          Java_SupportedProfileAdapter_getMinWidth(env, java_profile),
          Java_SupportedProfileAdapter_getMinHeight(env, java_profile));
      info.profile.max_resolution = gfx::Size(
          Java_SupportedProfileAdapter_getMaxWidth(env, java_profile),
          Java_SupportedProfileAdapter_getMaxHeight(env, java_profile));
      info.profile.max_framerate_numerator =
          Java_SupportedProfileAdapter_getMaxFramerateNumerator(env,
                                                                java_profile);
      info.profile.max_framerate_denominator =
          Java_SupportedProfileAdapter_getMaxFramerateDenominator(env,
                                                                  java_profile);
      if (Java_SupportedProfileAdapter_supportsCbr(env, java_profile)) {
        info.profile.rate_control_modes |=
            VideoEncodeAccelerator::kConstantMode;
      }
      if (Java_SupportedProfileAdapter_supportsVbr(env, java_profile)) {
        info.profile.rate_control_modes |=
            VideoEncodeAccelerator::kVariableMode;
      }
      info.profile.is_software_codec =
          Java_SupportedProfileAdapter_isSoftwareCodec(env, java_profile);

      int num_temporal_layers =
          Java_SupportedProfileAdapter_getMaxNumberOfTemporalLayers(
              env, java_profile);

      info.profile.scalability_modes.push_back(SVCScalabilityMode::kL1T1);
      if (num_temporal_layers >= 2) {
        info.profile.scalability_modes.push_back(SVCScalabilityMode::kL1T2);
      }
      if (num_temporal_layers >= 3) {
        info.profile.scalability_modes.push_back(SVCScalabilityMode::kL1T3);
      }
      info.name = base::android::ConvertJavaStringToUTF8(
          Java_SupportedProfileAdapter_getName(env, java_profile));
      cpp_infos.push_back(info);
    }

    // Use a stable sort since codec information is returned in a rank order
    // specified by the OEM.
    std::stable_sort(
        cpp_infos.begin(), cpp_infos.end(),
        [](const MediaCodecEncoderInfo& a, const MediaCodecEncoderInfo& b) {
          return a.profile.profile < b.profile.profile;
        });
    return cpp_infos;
  }());
  return *infos;
}

const std::vector<MediaCodecDecoderInfo>& GetDecoderInfoCache() {
  static const base::NoDestructor<std::vector<MediaCodecDecoderInfo>> infos([] {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    CHECK(env);
    auto java_profiles =
        Java_VideoAcceleratorUtil_getSupportedDecoderProfiles(env);

    std::vector<MediaCodecDecoderInfo> cpp_infos;
    if (!java_profiles) {
      // Per histograms this happens ~0% of the time, so no need for fallback.
      return cpp_infos;
    }

    for (auto java_profile : java_profiles.ReadElements<jobject>()) {
      MediaCodecDecoderInfo info;
      info.profile = static_cast<VideoCodecProfile>(
          Java_SupportedProfileAdapter_getProfile(env, java_profile));
      info.level = Java_SupportedProfileAdapter_getLevel(env, java_profile);
      info.coded_size_min = gfx::Size(
          Java_SupportedProfileAdapter_getMinWidth(env, java_profile),
          Java_SupportedProfileAdapter_getMinHeight(env, java_profile));
      info.coded_size_max = gfx::Size(
          Java_SupportedProfileAdapter_getMaxWidth(env, java_profile),
          Java_SupportedProfileAdapter_getMaxHeight(env, java_profile));
      info.is_software_codec =
          Java_SupportedProfileAdapter_isSoftwareCodec(env, java_profile);
      bool supports_secure_playback =
          Java_SupportedProfileAdapter_supportsSecurePlayback(env,
                                                              java_profile);
      bool requires_secure_playback =
          Java_SupportedProfileAdapter_requiresSecurePlayback(env,
                                                              java_profile);
      // If the decoder requires secure playback, it must support secure
      // playback.
      DCHECK(!requires_secure_playback || supports_secure_playback);
      info.secure_codec_capability =
          requires_secure_playback
              ? SecureCodecCapability::kEncrypted
              : (supports_secure_playback ? SecureCodecCapability::kAny
                                          : SecureCodecCapability::kClear);
      info.name = base::android::ConvertJavaStringToUTF8(
          Java_SupportedProfileAdapter_getName(env, java_profile));
      cpp_infos.push_back(info);
    }

    // Use a stable sort since codec information is returned in a rank order
    // specified by the OEM.
    std::stable_sort(
        cpp_infos.begin(), cpp_infos.end(),
        [](const MediaCodecDecoderInfo& a, const MediaCodecDecoderInfo& b) {
          return a.profile < b.profile;
        });
    return cpp_infos;
  }());
  return *infos;
}

}  // namespace media

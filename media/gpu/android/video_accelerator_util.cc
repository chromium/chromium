// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/video_accelerator_util.h"

#include "base/android/build_info.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "media/base/android/media_jni_headers/VideoAcceleratorUtil_jni.h"

namespace media {

const std::vector<MediaCodecEncoderInfo>& GetEncoderInfoCache() {
  static const base::NoDestructor<std::vector<MediaCodecEncoderInfo>> infos([] {
    // Sadly the NDK doesn't provide a mechanism for accessing the equivalent of
    // the SDK's MediaCodecList, so we must call into Java to enumerate support.
    JNIEnv* env = base::android::AttachCurrentThread();
    CHECK(env);
    auto java_profiles =
        Java_VideoAcceleratorUtil_getSupportedEncoderProfiles(env);

    constexpr char kHasMediaCodecEncoderInfo[] =
        "Media.Android.MediaCodecInfo.HasEncoderInfo";
    std::vector<MediaCodecEncoderInfo> cpp_infos;
    if (!java_profiles) {
      base::UmaHistogramBoolean(kHasMediaCodecEncoderInfo, false);
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

      info.name = base::android::ConvertJavaStringToUTF8(
          Java_SupportedProfileAdapter_getName(env, java_profile));
      cpp_infos.push_back(info);
    }
    std::sort(
        cpp_infos.begin(), cpp_infos.end(),
        [](const MediaCodecEncoderInfo& a, const MediaCodecEncoderInfo& b) {
          return a.profile.profile < b.profile.profile;
        });
    base::UmaHistogramBoolean(kHasMediaCodecEncoderInfo, !cpp_infos.empty());
    return cpp_infos;
  }());
  return *infos;
}

const std::vector<MediaCodecDecoderInfo>& GetDecoderInfoCache() {
  static const base::NoDestructor<std::vector<MediaCodecDecoderInfo>> infos([] {
    JNIEnv* env = base::android::AttachCurrentThread();
    CHECK(env);
    auto java_profiles =
        Java_VideoAcceleratorUtil_getSupportedDecoderProfiles(env);
    constexpr char kHasMediaCodecDecoderInfo[] =
        "Media.Android.MediaCodecInfo.HasDecoderInfo";
    if (!java_profiles) {
      // TODO(crbug.com/1413887): Can we remove default profiles?
      base::UmaHistogramBoolean(kHasMediaCodecDecoderInfo, false);

      LOG(ERROR)
          << "Unable to retreive MediaCodecInfo, assuming default support.";

      // Since we don't bundle a software decoder for H.264, H.265, to avoid
      // breaking all video unnecessarily, inject what is likely supported.
      constexpr auto kDefaultSize = gfx::Size(4096, 4096);
      constexpr auto kSoftwareCodec = true;
      constexpr auto kHardwareCodec = false;
      return std::vector<MediaCodecDecoderInfo>({
          {H264PROFILE_BASELINE, gfx::Size(), kDefaultSize, kHardwareCodec},
          {H264PROFILE_MAIN, gfx::Size(), kDefaultSize, kHardwareCodec},
          {H264PROFILE_HIGH, gfx::Size(), kDefaultSize, kHardwareCodec},
          {HEVCPROFILE_MAIN, gfx::Size(), kDefaultSize, kHardwareCodec},

          // Report codecs as software where we have a bundled decoder.
          {VP8PROFILE_ANY, gfx::Size(), kDefaultSize, kSoftwareCodec},
          {VP9PROFILE_PROFILE0, gfx::Size(), kDefaultSize, kSoftwareCodec},
      });
    }

    std::vector<MediaCodecDecoderInfo> cpp_infos;
    for (auto java_profile : java_profiles.ReadElements<jobject>()) {
      MediaCodecDecoderInfo info;
      info.profile = static_cast<VideoCodecProfile>(
          Java_SupportedProfileAdapter_getProfile(env, java_profile));
      info.coded_size_min = gfx::Size(
          Java_SupportedProfileAdapter_getMinWidth(env, java_profile),
          Java_SupportedProfileAdapter_getMinHeight(env, java_profile));
      info.coded_size_max = gfx::Size(
          Java_SupportedProfileAdapter_getMaxWidth(env, java_profile),
          Java_SupportedProfileAdapter_getMaxHeight(env, java_profile));
      info.is_software_codec =
          Java_SupportedProfileAdapter_isSoftwareCodec(env, java_profile);
      cpp_infos.push_back(info);
    }
    std::sort(
        cpp_infos.begin(), cpp_infos.end(),
        [](const MediaCodecDecoderInfo& a, const MediaCodecDecoderInfo& b) {
          return a.profile < b.profile;
        });

    base::UmaHistogramBoolean(kHasMediaCodecDecoderInfo, !cpp_infos.empty());
    return cpp_infos;
  }());
  return *infos;
}

}  // namespace media

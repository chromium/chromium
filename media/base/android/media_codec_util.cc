// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_codec_util.h"

#include <stddef.h>

#include <vector>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/video_codecs.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "media/base/android/media_jni_headers/CodecProfileLevelList_jni.h"
#include "media/base/android/media_jni_headers/MediaCodecUtil_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::android_info::SDK_VERSION_P;

namespace media {

namespace {
const char kMp3MimeType[] = "audio/mpeg";
const char kAacMimeType[] = "audio/mp4a-latm";
const char kOpusMimeType[] = "audio/opus";
const char kVorbisMimeType[] = "audio/vorbis";
const char kFLACMimeType[] = "audio/flac";
const char kAc3MimeType[] = "audio/ac3";
const char kEac3MimeType[] = "audio/eac3";
const char kBitstreamAudioMimeType[] = "audio/raw";
const char kAvcMimeType[] = "video/avc";
const char kDolbyVisionMimeType[] = "video/dolby-vision";
const char kHevcMimeType[] = "video/hevc";
const char kVp8MimeType[] = "video/x-vnd.on2.vp8";
const char kVp9MimeType[] = "video/x-vnd.on2.vp9";
const char kAv1MimeType[] = "video/av01";
const char kDtsMimeType[] = "audio/vnd.dts";
const char kDtseMimeType[] = "audio/vnd.dts;profile=lbr";
const char kDtsxP2MimeType[] = "audio/vnd.dts.uhd;profile=p2";
}  // namespace

static CodecProfileLevel MediaCodecProfileLevelToChromiumProfileLevel(
    JNIEnv* env,
    const JavaRef<jobject>& j_codec_profile_level) {
  VideoCodec codec = static_cast<VideoCodec>(
      Java_CodecProfileLevelAdapter_getCodec(env, j_codec_profile_level));
  VideoCodecProfile profile = static_cast<VideoCodecProfile>(
      Java_CodecProfileLevelAdapter_getProfile(env, j_codec_profile_level));
  auto level = static_cast<VideoCodecLevel>(
      Java_CodecProfileLevelAdapter_getLevel(env, j_codec_profile_level));
  return {codec, profile, level};
}

static bool IsDecoderSupportedByDevice(const std::string& android_mime_type) {
  if (android_mime_type == kVp8MimeType) {
    std::string hardware = base::SysInfo::GetAndroidBuildID();
    // MediaTek decoders do not work properly on vp8. See
    // http://crbug.com/446974 and http://crbug.com/597836.
    if (hardware.starts_with("mt")) {
      if (base::android::android_info::sdk_int() <
          base::android::android_info::SDK_VERSION_P) {
        return false;
      }
      // MediaTek chipsets after 'Android T' are compatible with vp8.
      if (base::android::android_info::sdk_int() <
          base::android::android_info::SDK_VERSION_T) {
        // The following chipsets have been confirmed by MediaTek to work on P+
        return hardware.starts_with("mt5599") ||
               hardware.starts_with("mt5895") ||
               hardware.starts_with("mt8768") ||
               hardware.starts_with("mt8696") || hardware.starts_with("mt5887");
      }
    }
  } else if (android_mime_type == kVp9MimeType) {
    // Nexus Player VP9 decoder performs poorly at >= 1080p resolution.
    if (base::SysInfo::HardwareModelName() == "Nexus Player") {
      return false;
    }
  } else if (android_mime_type == kAv1MimeType) {
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_Q) {
      return false;
    }
  }
  return true;
}

static jboolean JNI_MediaCodecUtil_IsDecoderSupportedForDevice(
    JNIEnv* env,
    std::string& mime_type) {
  return IsDecoderSupportedByDevice(mime_type);
}

static bool CanDecodeInternal(const std::string& mime, bool is_secure) {
  if (mime.empty())
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_mime = ConvertUTF8ToJavaString(env, mime);
  return Java_MediaCodecUtil_canDecode(env, j_mime, is_secure);
}

static bool HasVp9Profile23Decoder() {
  // Support for VP9.2, VP9.3 was added in Nougat but it requires hardware
  // support which we can't check from the renderer process. Since Android P+
  // has a software decoder available for VP9.2, VP9.3 content and usage is nil
  // on Android, just gate support on P+.
  return base::android::android_info::sdk_int() >=
         base::android::android_info::SDK_VERSION_P;
}

// static
std::string MediaCodecUtil::CodecToAndroidMimeType(AudioCodec codec) {
  return CodecToAndroidMimeType(codec, kUnknownSampleFormat);
}

// static
std::string MediaCodecUtil::CodecToAndroidMimeType(AudioCodec codec,
                                                   SampleFormat sample_format) {
  // Passthrough is possible for some bitstream formats.
  if (sample_format == kSampleFormatDts ||
      sample_format == kSampleFormatDtsxP2 ||
      sample_format == kSampleFormatAc3 || sample_format == kSampleFormatEac3 ||
      sample_format == kSampleFormatMpegHAudio) {
    return kBitstreamAudioMimeType;
  }

  switch (codec) {
    case AudioCodec::kMP3:
      return kMp3MimeType;
    case AudioCodec::kVorbis:
      return kVorbisMimeType;
    case AudioCodec::kFLAC:
      return kFLACMimeType;
    case AudioCodec::kOpus:
      return kOpusMimeType;
    case AudioCodec::kAAC:
      return kAacMimeType;
    case AudioCodec::kAC3:
      return kAc3MimeType;
    case AudioCodec::kEAC3:
      return kEac3MimeType;
    case AudioCodec::kDTS:
      return kDtsMimeType;
    case AudioCodec::kDTSE:
      return kDtseMimeType;
    case AudioCodec::kDTSXP2:
      return kDtsxP2MimeType;
    default:
      return std::string();
  }
}

// static
std::string MediaCodecUtil::CodecToAndroidMimeType(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return kAvcMimeType;
    case VideoCodec::kHEVC:
      return kHevcMimeType;
    case VideoCodec::kVP8:
      return kVp8MimeType;
    case VideoCodec::kVP9:
      return kVp9MimeType;
    case VideoCodec::kDolbyVision:
      return kDolbyVisionMimeType;
    case VideoCodec::kAV1:
      return kAv1MimeType;
    default:
      return std::string();
  }
}

// static
bool MediaCodecUtil::IsVp8DecoderAvailable() {
  return IsDecoderSupportedByDevice(kVp8MimeType);
}

// static
bool MediaCodecUtil::IsVp9DecoderAvailable() {
  return IsDecoderSupportedByDevice(kVp9MimeType);
}

// static
bool MediaCodecUtil::IsVp9Profile2DecoderAvailable() {
  return IsVp9DecoderAvailable() && HasVp9Profile23Decoder();
}

// static
bool MediaCodecUtil::IsVp9Profile3DecoderAvailable() {
  return IsVp9DecoderAvailable() && HasVp9Profile23Decoder();
}

// static
bool MediaCodecUtil::IsOpusDecoderAvailable() {
  return IsDecoderSupportedByDevice(kOpusMimeType);
}

// static
bool MediaCodecUtil::IsAv1DecoderAvailable() {
  return IsDecoderSupportedByDevice(kAv1MimeType);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
// static
bool MediaCodecUtil::IsHEVCDecoderAvailable() {
  return IsDecoderSupportedByDevice(kHevcMimeType);
}
#endif

// static
bool MediaCodecUtil::IsAACEncoderAvailable() {
  // We only support AAC encoding on android Q+, due to our use of the NDK.
  return base::android::android_info::sdk_int() >=
         base::android::android_info::SDK_VERSION_Q;
}

// static
std::optional<gfx::Size> MediaCodecUtil::LookupCodedSizeAlignment(
    std::string_view name,
    std::optional<int> host_sdk_int) {
  // Below we build a map of codec names to coded size alignments. We do this on
  // a best effort basis to avoid glitches during a coded size change.
  //
  // A codec name may have multiple entries, if so they must be in descending
  // order by SDK version since the array is scanned from front to back.
  //
  // When testing codec names, we don't require an exact match, just that the
  // name we're looking up starts with `name_prefix` since many codecs have
  // multiple variants with various suffixes appended.
  //
  // New alignments can be added by inspecting logcat or
  // chrome://media-internals after running the test page at
  // https://crbug.com/1456427#c69.
  struct CodecAlignment {
    const char* name_regex;
    gfx::Size alignment;
    int sdk_int = base::android::android_info::SDK_VERSION_NOUGAT;
  };
  using base::android::android_info::SDK_VERSION_Q;
  using base::android::android_info::SDK_VERSION_R;
  using base::android::android_info::SDK_VERSION_Sv2;
  using base::android::android_info::SDK_VERSION_U;
  constexpr CodecAlignment kCodecAlignmentMap[] = {
      // Codec2 software decoders.
      {"c2.android.avc", gfx::Size(128, 2), SDK_VERSION_Sv2},
      {"c2.android.avc", gfx::Size(32, 2), SDK_VERSION_R},
      {"c2.android.avc", gfx::Size(64, 2)},
      {"c2.android.hevc", gfx::Size(128, 2), SDK_VERSION_Sv2},
      {"c2.android.hevc", gfx::Size(32, 2), SDK_VERSION_R},
      {"c2.android.hevc", gfx::Size(64, 2)},
      {"c2.android.(vp8|vp9|av1)", gfx::Size(16, 2)},

      // Codec1 software decoders.
      {"omx.google.(h264|hevc|vp8|vp9)", gfx::Size(2, 2)},

      // Google AV1 hardware decoder.
      {"c2.google.av1", gfx::Size(64, 16), SDK_VERSION_U},
      {"c2.google.av1", gfx::Size(64, 8)},

      // Qualcomm
      {"c2.qti.(avc|vp8)", gfx::Size(16, 16)},
      {"c2.qti.(hevc|vp9)", gfx::Size(8, 8)},
      {"omx.qcom.video.decoder.avc", gfx::Size(16, 16), SDK_VERSION_Q},
      {"omx.qcom.video.decoder.avc", gfx::Size(1, 1)},
      {"omx.qcom.video.decoder.hevc", gfx::Size(8, 8), SDK_VERSION_Q},
      {"omx.qcom.video.decoder.hevc", gfx::Size(1, 1)},
      {"omx.qcom.video.decoder.vp8", gfx::Size(16, 16), SDK_VERSION_R},
      {"omx.qcom.video.decoder.vp8", gfx::Size(1, 1)},
      {"omx.qcom.video.decoder.vp9", gfx::Size(8, 8), SDK_VERSION_R},
      {"omx.qcom.video.decoder.vp9", gfx::Size(1, 1)},

      // Samsung
      {"(omx|c2).exynos.h264", gfx::Size(16, 16)},
      {"(omx|c2).exynos.hevc", gfx::Size(8, 8)},
      {"(omx|c2).exynos.(vp8|vp9)", gfx::Size(1, 1)},

      // Unisoc
      {"omx.sprd.(h264|vpx)", gfx::Size(16, 16)},
      {"omx.sprd.(hevc|vp9)", gfx::Size(64, 64)},
  };

  const auto lower_name = base::ToLowerASCII(name);

  const auto sdk_int =
      host_sdk_int.value_or(base::android::android_info::sdk_int());
  for (const auto& entry : kCodecAlignmentMap) {
    if (sdk_int >= entry.sdk_int &&
        RE2::PartialMatch(lower_name, entry.name_regex)) {
      return entry.alignment;
    }
  }

  return std::nullopt;
}

// static
bool MediaCodecUtil::CanDecode(VideoCodec codec, bool is_secure) {
  return CanDecodeInternal(CodecToAndroidMimeType(codec), is_secure);
}

// static
bool MediaCodecUtil::CanDecode(AudioCodec codec) {
  return CanDecodeInternal(CodecToAndroidMimeType(codec), false);
}

// static
void MediaCodecUtil::AddSupportedCodecProfileLevels(
    std::vector<CodecProfileLevel>* result) {
  DCHECK(result);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_codec_profile_levels(
      Java_MediaCodecUtil_getSupportedCodecProfileLevels(env));
  for (auto java_codec_profile_level :
       j_codec_profile_levels.ReadElements<jobject>()) {
    result->push_back(MediaCodecProfileLevelToChromiumProfileLevel(
        env, java_codec_profile_level));
  }
}

// static
bool MediaCodecUtil::IsKnownUnaccelerated(VideoCodec codec,
                                          MediaCodecDirection direction) {
  auto* env = AttachCurrentThread();
  auto j_mime = ConvertUTF8ToJavaString(env, CodecToAndroidMimeType(codec));
  auto j_codec_name = Java_MediaCodecUtil_getDefaultCodecName(
      env, j_mime, static_cast<int>(direction), /*requireSoftwareCodec=*/false,
      /*requireHardwareCodec=*/true);

  auto codec_name = ConvertJavaStringToUTF8(env, j_codec_name.obj());
  DVLOG(1) << __func__ << "Default hardware codec for " << GetCodecName(codec)
           << " : " << codec_name
           << ", direction: " << static_cast<int>(direction);
  if (codec_name.empty())
    return true;

  // MediaTek hardware vp8 is known slower than the software implementation.
  if (base::StartsWith(codec_name, "OMX.MTK.") && codec == VideoCodec::kVP8) {
    // We may still reject VP8 hardware decoding later on certain chipsets,
    // see IsDecoderSupportedByDevice(). We don't have the the chipset ID
    // here to check now though.
    return base::android::android_info::sdk_int() < SDK_VERSION_P;
  }

  return false;
}

}  // namespace media

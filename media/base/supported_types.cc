// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "ui/gfx/hdr_metadata.h"

#if BUILDFLAG(ENABLE_LIBVPX)
// TODO(dalecurtis): This technically should not be allowed in media/base. See
// TODO below about moving outside of base.
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"      // nogncheck
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"

// TODO(dalecurtis): This include is not allowed by media/base since
// media/base/android is technically a different component. We should move
// supported_types*.{cc,h} out of media/base to fix this.
#include "media/base/android/media_codec_util.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace media {

namespace {

template <typename T>
class SupplementalProfileCache {
 public:
  void UpdateCache(const base::flat_set<T>& profiles) {
    base::AutoLock lock(profiles_lock_);
    profiles_ = profiles;
  }
  bool IsProfileSupported(T profile) {
    base::AutoLock lock(profiles_lock_);
    return profiles_.find(profile) != profiles_.end();
  }

 private:
  base::Lock profiles_lock_;
  base::flat_set<T> profiles_ GUARDED_BY(profiles_lock_);
};

SupplementalProfileCache<VideoCodecProfile>*
GetSupplementalDecoderVideoProfileCache() {
  static base::NoDestructor<SupplementalProfileCache<VideoCodecProfile>> cache;
  return cache.get();
}

SupplementalProfileCache<AudioType>* GetSupplementalDecoderAudioTypeCache() {
  static base::NoDestructor<SupplementalProfileCache<AudioType>> cache;
  return cache.get();
}

SupplementalProfileCache<VideoCodecProfile>*
GetSupplementalEncoderVideoProfileCache() {
  static base::NoDestructor<SupplementalProfileCache<VideoCodecProfile>> cache;
  return cache.get();
}

bool IsDecoderSupportedHdrMetadata(const VideoType& type) {
  switch (type.hdr_metadata_type) {
    case gfx::HdrMetadataType::kNone:
      return true;

    case gfx::HdrMetadataType::kSmpteSt2086:
      // HDR metadata is currently only used with the PQ transfer function.
      // See gfx::ColorTransform for more details.
      return type.color_space.transfer ==
             VideoColorSpace::TransferID::SMPTEST2084;

    // 2094-10 SEI metadata is not the same as Dolby Vision RPU metadata, Dolby
    // Vision decoders on each platform only support Dolby Vision RPU metadata.
    case gfx::HdrMetadataType::kSmpteSt2094_10:
    case gfx::HdrMetadataType::kSmpteSt2094_40:
      return false;
  }
}

bool IsDecoderColorSpaceSupported(const VideoColorSpace& color_space) {
  switch (color_space.primaries) {
    // Transfers supported before color management.
    case VideoColorSpace::PrimaryID::BT709:
    case VideoColorSpace::PrimaryID::UNSPECIFIED:
    case VideoColorSpace::PrimaryID::BT470M:
    case VideoColorSpace::PrimaryID::BT470BG:
    case VideoColorSpace::PrimaryID::SMPTE170M:
      break;

    // Supported with color management.
    case VideoColorSpace::PrimaryID::SMPTE240M:
    case VideoColorSpace::PrimaryID::FILM:
    case VideoColorSpace::PrimaryID::BT2020:
    case VideoColorSpace::PrimaryID::SMPTEST428_1:
    case VideoColorSpace::PrimaryID::SMPTEST431_2:
    case VideoColorSpace::PrimaryID::SMPTEST432_1:
    case VideoColorSpace::PrimaryID::EBU_3213_E:
      break;

    // Never supported.
    case VideoColorSpace::PrimaryID::INVALID:
      return false;
  }

  switch (color_space.transfer) {
    // Transfers supported before color management.
    case VideoColorSpace::TransferID::UNSPECIFIED:
    case VideoColorSpace::TransferID::GAMMA22:
    case VideoColorSpace::TransferID::BT709:
    case VideoColorSpace::TransferID::SMPTE170M:
    case VideoColorSpace::TransferID::BT2020_10:
    case VideoColorSpace::TransferID::BT2020_12:
    case VideoColorSpace::TransferID::IEC61966_2_1:
      break;

    // Supported with color management.
    case VideoColorSpace::TransferID::GAMMA28:
    case VideoColorSpace::TransferID::SMPTE240M:
    case VideoColorSpace::TransferID::LINEAR:
    case VideoColorSpace::TransferID::LOG:
    case VideoColorSpace::TransferID::LOG_SQRT:
    case VideoColorSpace::TransferID::BT1361_ECG:
    case VideoColorSpace::TransferID::SMPTEST2084:
    case VideoColorSpace::TransferID::IEC61966_2_4:
    case VideoColorSpace::TransferID::SMPTEST428_1:
    case VideoColorSpace::TransferID::ARIB_STD_B67:
      break;

    // Never supported.
    case VideoColorSpace::TransferID::INVALID:
      return false;
  }

  switch (color_space.matrix) {
    // Supported before color management.
    case VideoColorSpace::MatrixID::BT709:
    case VideoColorSpace::MatrixID::UNSPECIFIED:
    case VideoColorSpace::MatrixID::BT470BG:
    case VideoColorSpace::MatrixID::SMPTE170M:
    case VideoColorSpace::MatrixID::BT2020_NCL:
      break;

    // Supported with color management.
    case VideoColorSpace::MatrixID::RGB:
    case VideoColorSpace::MatrixID::FCC:
    case VideoColorSpace::MatrixID::SMPTE240M:
    case VideoColorSpace::MatrixID::YCOCG:
    case VideoColorSpace::MatrixID::YDZDX:
    case VideoColorSpace::MatrixID::BT2020_CL:
      break;

    // Never supported.
    case VideoColorSpace::MatrixID::INVALID:
      return false;
  }

  if (color_space.range == gfx::ColorSpace::RangeID::INVALID)
    return false;

  return true;
}

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
bool IsVideoCodecProprietary(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kVC1:
    case VideoCodec::kH264:
    case VideoCodec::kMPEG2:
    case VideoCodec::kMPEG4:
    case VideoCodec::kHEVC:
    case VideoCodec::kDolbyVision:
      return true;
    case VideoCodec::kUnknown:
    case VideoCodec::kTheora:
    case VideoCodec::kVP8:
    case VideoCodec::kVP9:
    case VideoCodec::kAV1:
      return false;
  }
}

bool IsAudioCodecProprietary(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kAAC:
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kGSM_MS:
    case AudioCodec::kALAC:
    case AudioCodec::kMpegHAudio:
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
    case AudioCodec::kDTSE:
    case AudioCodec::kAC4:
      return true;

    case AudioCodec::kFLAC:
    case AudioCodec::kIAMF:
    case AudioCodec::kMP3:
    case AudioCodec::kOpus:
    case AudioCodec::kVorbis:
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kUnknown:
      return false;
  }
}
#endif  // !BUILDFLAG(USE_PROPRIETARY_CODECS)

bool IsDecoderHevcProfileSupported(const VideoType& type) {
  if (!IsDecoderColorSpaceSupported(type.color_space)) {
    return false;
  }

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/171813538): For Lacros, the supplemental profile cache will be
  // asking lacros-gpu, but we will be doing decoding in ash-gpu. Until the
  // codec detection is plumbed through to ash-gpu we can do this extra check
  // for HEVC support.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLacrosEnablePlatformHevc)) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return GetSupplementalDecoderVideoProfileCache()->IsProfileSupported(
      type.profile);
#else
  return true;
#endif  // BUIDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT)
#else
  return false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
}

bool IsDecoderVp9ProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_LIBVPX)
  // High bit depth capabilities may be toggled via LibVPX config flags.
  static const bool vpx_supports_hbd = (vpx_codec_get_caps(vpx_codec_vp9_dx()) &
                                        VPX_CODEC_CAP_HIGHBITDEPTH) != 0;

  // Color management required for HDR to not look terrible.
  if (!IsDecoderColorSpaceSupported(type.color_space)) {
    return false;
  }

  switch (type.profile) {
    // LibVPX always supports Profiles 0 and 1.
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
      return true;
#if BUILDFLAG(IS_ANDROID)
    case VP9PROFILE_PROFILE2:
      return vpx_supports_hbd ||
             MediaCodecUtil::IsVp9Profile2DecoderAvailable();
    case VP9PROFILE_PROFILE3:
      return vpx_supports_hbd ||
             MediaCodecUtil::IsVp9Profile3DecoderAvailable();
#else
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return vpx_supports_hbd;
#endif  // BUILDFLAG(IS_ANDROID)
    default:
      NOTREACHED();
  }
#else
  return false;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

bool IsDecoderAV1Supported(const VideoType& type) {
  // If the AV1 decoder is enabled, or if we're on Q or later, yes.
#if BUILDFLAG(ENABLE_AV1_DECODER)
  return IsDecoderColorSpaceSupported(type.color_space);
#elif BUILDFLAG(IS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_Q &&
         IsDecoderColorSpaceSupported(type.color_space);
#else
  return false;
#endif
}

bool IsDecoderAACSupported(const AudioType& type) {
  if (type.profile != AudioCodecProfile::kXHE_AAC) {
    return true;
  }
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN))
  return GetSupplementalDecoderAudioTypeCache()->IsProfileSupported(type);
#else
  return false;
#endif
}

bool IsDecoderDolbyVisionProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) &&                      \
    BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_DECODE_SUPPORT) && \
    BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  return GetSupplementalDecoderVideoProfileCache()->IsProfileSupported(
      type.profile);
#else
  return false;
#endif
}

bool IsDecoderDolbyAc3Eac3Supported(const AudioType& type) {
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
#if BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
  return GetSupplementalDecoderAudioTypeCache()->IsProfileSupported(type);
#else
  // Keep 'true' for other platforms as old code snippet.
  return true;
#endif  // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && (BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_MAC))
#else
  return false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
}

bool IsDecoderDolbyAc4Supported(const AudioType& type) {
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO) && \
    BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && BUILDFLAG(IS_WIN)
  return GetSupplementalDecoderAudioTypeCache()->IsProfileSupported(type);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO) &&
        // BUILDFLAG(ENABLE_MOJO_AUDIO_DECODER) && BUILDFLAG(IS_WIN)
}

bool IsEncoderH264ProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_OPENH264)
  switch (type.profile) {
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_HIGH:
    case H264PROFILE_EXTENDED:
      return true;
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      // Although some of these profiles are supported by openH264, but we don't
      // wire them for now.
      return false;
    default:
      NOTREACHED();
  }
#elif BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Android and iOS won't bundle OpenH264, query hardware encoder support
  // instead.
  return GetSupplementalEncoderVideoProfileCache()->IsProfileSupported(
      type.profile);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_OPENH264)
}

bool IsEncoderVp8ProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_LIBVPX)
  return true;
#else
  return false;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

bool IsEncoderHevcProfileSupported(const VideoType& type) {
#if BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
  return GetSupplementalEncoderVideoProfileCache()->IsProfileSupported(
      type.profile);
#else
  return false;
#endif  // BUILDFLAG(PLATFORM_HAS_OPTIONAL_HEVC_ENCODE_SUPPORT)
}

bool IsEncoderVp9ProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_LIBVPX)
  // High bit depth capabilities may be toggled via LibVPX config flags.
  static const bool vpx_supports_hbd = (vpx_codec_get_caps(vpx_codec_vp9_dx()) &
                                        VPX_CODEC_CAP_HIGHBITDEPTH) != 0;

  switch (type.profile) {
    // LibVPX always supports Profiles 0 and 1.
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
      return true;
    case VP9PROFILE_PROFILE2:
    case VP9PROFILE_PROFILE3:
      return vpx_supports_hbd;
    default:
      NOTREACHED();
  }
#else
  return false;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
}

bool IsEncoderAv1ProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_LIBAOM)
  switch (type.profile) {
    case AV1PROFILE_PROFILE_MAIN:
    case AV1PROFILE_PROFILE_HIGH:
      return true;
    case AV1PROFILE_PROFILE_PRO:
      // We don't build libaom with high bit depth support.
      return false;
    default:
      NOTREACHED();
  }
#elif BUILDFLAG(IS_ANDROID)
  // Android won't bundle libaom, query hardware encoder support instead.
  return GetSupplementalEncoderVideoProfileCache()->IsProfileSupported(
      type.profile);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_LIBAOM)
}

}  // namespace

bool IsDecoderSupportedAudioType(const AudioType& type) {
  if (auto* media_client = GetMediaClient())
    return media_client->IsDecoderSupportedAudioType(type);
  return IsDefaultDecoderSupportedAudioType(type);
}

bool IsDecoderSupportedVideoType(const VideoType& type) {
  if (auto* media_client = GetMediaClient())
    return media_client->IsDecoderSupportedVideoType(type);
  return IsDefaultDecoderSupportedVideoType(type);
}

bool IsEncoderSupportedVideoType(const VideoType& type) {
  if (auto* media_client = GetMediaClient()) {
    return media_client->IsEncoderSupportedVideoType(type);
  }
  return IsDefaultEncoderSupportedVideoType(type);
}

// TODO(chcunningham): Add platform specific logic for Android (move from
// MimeUtilInternal).
bool IsDefaultDecoderSupportedVideoType(const VideoType& type) {
  if (!IsDecoderSupportedHdrMetadata(type)) {
    return false;
  }

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsVideoCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case VideoCodec::kTheora:
      return IsDecoderBuiltInVideoCodec(type.codec);
    case VideoCodec::kH264:
      return true;
    case VideoCodec::kVP8:
      return IsDecoderBuiltInVideoCodec(type.codec)
                 ? true
                 : GetSupplementalDecoderVideoProfileCache()
                       ->IsProfileSupported(type.profile);
    case VideoCodec::kAV1:
      return IsDecoderAV1Supported(type);
    case VideoCodec::kVP9:
      return IsDecoderVp9ProfileSupported(type);
    case VideoCodec::kHEVC:
      return IsDecoderHevcProfileSupported(type);
    case VideoCodec::kDolbyVision:
      return IsDecoderDolbyVisionProfileSupported(type);
    case VideoCodec::kUnknown:
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kMPEG4:
      return false;
  }
}

bool IsDefaultDecoderSupportedAudioType(const AudioType& type) {
  if (type.spatial_rendering)
    return false;

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsAudioCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case AudioCodec::kAAC:
      return IsDecoderAACSupported(type);
    case AudioCodec::kFLAC:
    case AudioCodec::kMP3:
    case AudioCodec::kOpus:
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kVorbis:
      return true;
    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kGSM_MS:
    case AudioCodec::kALAC:
    case AudioCodec::kMpegHAudio:
    case AudioCodec::kIAMF:
    case AudioCodec::kUnknown:
      return false;
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
    case AudioCodec::kDTSE:
      return BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO);
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
      return IsDecoderDolbyAc3Eac3Supported(type);
    case AudioCodec::kAC4:
      return IsDecoderDolbyAc4Supported(type);
  }
}

bool IsDefaultEncoderSupportedVideoType(const VideoType& type) {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsVideoCodecProprietary(type.codec)) {
    return false;
  }
#endif

  switch (type.codec) {
    case VideoCodec::kH264:
      return IsEncoderH264ProfileSupported(type);
    case VideoCodec::kVP8:
      return IsEncoderVp8ProfileSupported(type);
    case VideoCodec::kAV1:
      return IsEncoderAv1ProfileSupported(type);
    case VideoCodec::kVP9:
      return IsEncoderVp9ProfileSupported(type);
    case VideoCodec::kHEVC:
      return IsEncoderHevcProfileSupported(type);
    case VideoCodec::kTheora:
    case VideoCodec::kDolbyVision:
    case VideoCodec::kUnknown:
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kMPEG4:
      return false;
  }
}

bool IsDecoderBuiltInVideoCodec(VideoCodec codec) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codec == VideoCodec::kH264 &&
      base::FeatureList::IsEnabled(kBuiltInH264Decoder)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) &&
        // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_LIBVPX)
#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (codec == VideoCodec::kAV1)
    return true;
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
  return false;
}

bool IsEncoderBuiltInVideoCodec(VideoCodec codec) {
#if BUILDFLAG(ENABLE_OPENH264) && BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codec == VideoCodec::kH264) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_OPENH264) && BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_LIBVPX)
#if BUILDFLAG(ENABLE_LIBAOM)
  if (codec == VideoCodec::kAV1) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_LIBAOM)
  return false;
}

bool MayHaveAndAllowSelectOSSoftwareEncoder(VideoCodec codec) {
  // Allow OS software encoding when we don't have an equivalent
  // software encoder.
  constexpr bool kHasBundledH264Encoder = BUILDFLAG(ENABLE_OPENH264);
  constexpr bool kHasOSSoftwareH264Encoder =
      BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID);
  constexpr bool kHasOSSoftwareHEVCEncoder =
      BUILDFLAG(IS_MAC) && BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER);

  switch (codec) {
    case VideoCodec::kH264:
      // Prefer the bundled encoder, if present.
      return kHasOSSoftwareH264Encoder && !kHasBundledH264Encoder;
    case VideoCodec::kHEVC:
      return kHasOSSoftwareHEVCEncoder;
    default:
      return false;
  }
}

void UpdateDefaultDecoderSupportedVideoProfiles(
    const base::flat_set<media::VideoCodecProfile>& profiles) {
  GetSupplementalDecoderVideoProfileCache()->UpdateCache(profiles);
}

void UpdateDefaultDecoderSupportedAudioTypes(
    const base::flat_set<AudioType>& types) {
  GetSupplementalDecoderAudioTypeCache()->UpdateCache(types);
}

void UpdateDefaultEncoderSupportedVideoProfiles(
    const base::flat_set<media::VideoCodecProfile>& profiles) {
  GetSupplementalEncoderVideoProfileCache()->UpdateCache(profiles);
}

}  // namespace media

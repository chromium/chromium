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
#include "ui/display/display_switches.h"

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

namespace media {

namespace {

class SupplementalProfileCache {
 public:
  void UpdateCache(const base::flat_set<media::VideoCodecProfile>& profiles) {
    base::AutoLock lock(profiles_lock_);
    profiles_ = profiles;
  }
  bool IsProfileSupported(media::VideoCodecProfile profile) {
    base::AutoLock lock(profiles_lock_);
    return profiles_.find(profile) != profiles_.end();
  }

 private:
  base::Lock profiles_lock_;
  base::flat_set<media::VideoCodecProfile> profiles_ GUARDED_BY(profiles_lock_);
};

SupplementalProfileCache* GetSupplementalProfileCache() {
  static base::NoDestructor<SupplementalProfileCache> cache;
  return cache.get();
}

bool IsSupportedHdrMetadata(const gfx::HdrMetadataType& hdr_metadata_type) {
  switch (hdr_metadata_type) {
    case gfx::HdrMetadataType::kNone:
      return true;

    case gfx::HdrMetadataType::kSmpteSt2086:
    case gfx::HdrMetadataType::kSmpteSt2094_10:
    case gfx::HdrMetadataType::kSmpteSt2094_40:
      return false;
  }
}

bool IsColorSpaceSupported(const VideoColorSpace& color_space) {
  switch (color_space.primaries) {
    case VideoColorSpace::PrimaryID::EBU_3213_E:
    case VideoColorSpace::PrimaryID::INVALID:
      return false;

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
      break;
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
      return true;

    case AudioCodec::kFLAC:
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

bool IsHevcProfileSupported(const VideoType& type) {
  if (!IsColorSpaceSupported(type.color_space))
    return false;

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_MAC)
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
  return GetSupplementalProfileCache()->IsProfileSupported(type.profile);
#elif BUILDFLAG(IS_ANDROID)
  // Technically android 5.0 mandates support for only HEVC main profile,
  // however some platforms (like chromecast) have had more profiles supported
  // so we'll see what happens if we just enable them all.
  return base::FeatureList::IsEnabled(kPlatformHEVCDecoderSupport);
#else
  return true;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_MAC)
#else
  return false;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
}

bool IsVp9ProfileSupported(const VideoType& type) {
#if BUILDFLAG(ENABLE_LIBVPX)
  // High bit depth capabilities may be toggled via LibVPX config flags.
  static const bool vpx_supports_hbd = (vpx_codec_get_caps(vpx_codec_vp9_dx()) &
                                        VPX_CODEC_CAP_HIGHBITDEPTH) != 0;

  // Color management required for HDR to not look terrible.
  if (!IsColorSpaceSupported(type.color_space))
    return false;

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
#endif  // BUILDFLAG(ENABLE_LIBVPX)
  return false;
}

bool IsAV1Supported(const VideoType& type) {
  // If the AV1 decoder is enabled, or if we're on Q or later, yes.
#if BUILDFLAG(ENABLE_AV1_DECODER)
  return IsColorSpaceSupported(type.color_space);
#elif BUILDFLAG(IS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_Q &&
         IsColorSpaceSupported(type.color_space);
#else
  return false;
#endif
}

bool IsMPEG4Supported() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#else
  return false;
#endif
}

bool IsAACSupported(const AudioType& type) {
  if (type.profile != AudioCodecProfile::kXHE_AAC)
    return true;
#if BUILDFLAG(IS_ANDROID)
  return base::android::BuildInfo::GetInstance()->sdk_int() >=
         base::android::SDK_VERSION_P;
#elif BUILDFLAG(IS_MAC)
  if (__builtin_available(macOS 10.15, *))
    return true;
  return false;
#else
  return false;
#endif
}

}  // namespace

bool IsSupportedAudioType(const AudioType& type) {
  if (auto* media_client = GetMediaClient())
    return media_client->IsSupportedAudioType(type);
  return IsDefaultSupportedAudioType(type);
}

bool IsSupportedVideoType(const VideoType& type) {
  if (auto* media_client = GetMediaClient())
    return media_client->IsSupportedVideoType(type);
  return IsDefaultSupportedVideoType(type);
}

// TODO(chcunningham): Add platform specific logic for Android (move from
// MimeUtilInternal).
bool IsDefaultSupportedVideoType(const VideoType& type) {
  if (!IsSupportedHdrMetadata(type.hdr_metadata_type))
    return false;

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsVideoCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case VideoCodec::kH264:
    case VideoCodec::kVP8:
    case VideoCodec::kTheora:
      return true;
    case VideoCodec::kAV1:
      return IsAV1Supported(type);
    case VideoCodec::kVP9:
      return IsVp9ProfileSupported(type);
    case VideoCodec::kHEVC:
      return IsHevcProfileSupported(type);
    case VideoCodec::kMPEG4:
      return IsMPEG4Supported();
    case VideoCodec::kUnknown:
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kDolbyVision:
      return false;
  }
}

bool IsDefaultSupportedAudioType(const AudioType& type) {
  if (type.spatial_rendering)
    return false;

#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsAudioCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case AudioCodec::kAAC:
      return IsAACSupported(type);
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
    case AudioCodec::kEAC3:
    case AudioCodec::kALAC:
    case AudioCodec::kAC3:
    case AudioCodec::kMpegHAudio:
    case AudioCodec::kUnknown:
      return false;
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
      return true;
#else
      return false;
#endif
  }
}

bool IsBuiltInVideoCodec(VideoCodec codec) {
#if BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
  if (codec == VideoCodec::kTheora)
    return true;
  if (codec == VideoCodec::kVP8)
    return true;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codec == VideoCodec::kH264)
    return true;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#endif  // BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
#if BUILDFLAG(ENABLE_LIBVPX)
  if (codec == VideoCodec::kVP8 || codec == VideoCodec::kVP9)
    return true;
#endif  // BUILDFLAG(ENABLE_LIBVPX)
#if BUILDFLAG(ENABLE_AV1_DECODER)
  if (codec == VideoCodec::kAV1)
    return true;
#endif  // BUILDFLAG(ENABLE_AV1_DECODER)
  return false;
}

void UpdateDefaultSupportedVideoProfiles(
    const base::flat_set<media::VideoCodecProfile>& profiles) {
  GetSupplementalProfileCache()->UpdateCache(profiles);
}

}  // namespace media

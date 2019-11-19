// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/supported_types.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_client.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "ui/display/display_switches.h"

#if BUILDFLAG(ENABLE_LIBVPX)
// TODO(dalecurtis): This technically should not be allowed in media/base. See
// TODO below about moving outside of base.
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"

// TODO(dalecurtis): This include is not allowed by media/base since
// media/base/android is technically a different component. We should move
// supported_types*.{cc,h} out of media/base to fix this.
#include "media/base/android/media_codec_util.h"  // nogncheck
#endif

namespace media {

bool IsSupportedAudioType(const AudioType& type) {
  MediaClient* media_client = GetMediaClient();
  if (media_client)
    return media_client->IsSupportedAudioType(type);

  return IsDefaultSupportedAudioType(type);
}

bool IsSupportedVideoType(const VideoType& type) {
  MediaClient* media_client = GetMediaClient();
  if (media_client)
    return media_client->IsSupportedVideoType(type);

  return IsDefaultSupportedVideoType(type);
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

bool IsVp9ProfileSupported(VideoCodecProfile profile) {
#if BUILDFLAG(ENABLE_LIBVPX)
  // High bit depth capabilities may be toggled via LibVPX config flags.
  static const bool vpx_supports_hbd = (vpx_codec_get_caps(vpx_codec_vp9_dx()) &
                                        VPX_CODEC_CAP_HIGHBITDEPTH) != 0;
  switch (profile) {
    // LibVPX always supports Profiles 0 and 1.
    case VP9PROFILE_PROFILE0:
    case VP9PROFILE_PROFILE1:
      return true;
#if defined(OS_ANDROID)
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
#endif
    default:
      NOTREACHED();
  }
#endif
  return false;
}

bool IsAudioCodecProprietary(AudioCodec codec) {
  switch (codec) {
    case kCodecAAC:
    case kCodecAC3:
    case kCodecEAC3:
    case kCodecAMR_NB:
    case kCodecAMR_WB:
    case kCodecGSM_MS:
    case kCodecALAC:
    case kCodecMpegHAudio:
      return true;

    case kCodecFLAC:
    case kCodecMP3:
    case kCodecOpus:
    case kCodecVorbis:
    case kCodecPCM:
    case kCodecPCM_MULAW:
    case kCodecPCM_S16BE:
    case kCodecPCM_S24BE:
    case kCodecPCM_ALAW:
    case kUnknownAudioCodec:
      return false;
  }
}

bool IsDefaultSupportedAudioType(const AudioType& type) {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsAudioCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case kCodecAAC:
    case kCodecFLAC:
    case kCodecMP3:
    case kCodecOpus:
    case kCodecPCM:
    case kCodecPCM_MULAW:
    case kCodecPCM_S16BE:
    case kCodecPCM_S24BE:
    case kCodecPCM_ALAW:
    case kCodecVorbis:
      return true;

    case kCodecAMR_NB:
    case kCodecAMR_WB:
    case kCodecGSM_MS:
#if defined(OS_CHROMEOS)
      return true;
#else
      return false;
#endif

    case kCodecEAC3:
    case kCodecALAC:
    case kCodecAC3:
    case kCodecMpegHAudio:
    case kUnknownAudioCodec:
      return false;
  }

  NOTREACHED();
  return false;
}

bool IsVideoCodecProprietary(VideoCodec codec) {
  switch (codec) {
    case kCodecVC1:
    case kCodecH264:
    case kCodecMPEG2:
    case kCodecMPEG4:
    case kCodecHEVC:
    case kCodecDolbyVision:
      return true;
    case kUnknownVideoCodec:
    case kCodecTheora:
    case kCodecVP8:
    case kCodecVP9:
    case kCodecAV1:
      return false;
  }
}

// TODO(chcunningham): Add platform specific logic for Android (move from
// MimeUtilIntenral).
bool IsDefaultSupportedVideoType(const VideoType& type) {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (IsVideoCodecProprietary(type.codec))
    return false;
#endif

  switch (type.codec) {
    case kCodecAV1:
      // If the AV1 decoder is enabled, or if we're on Q or later, yes.
#if BUILDFLAG(ENABLE_AV1_DECODER)
      return IsColorSpaceSupported(type.color_space);
#elif defined(OS_ANDROID)
      if (base::android::BuildInfo::GetInstance()->is_at_least_q() &&
          IsColorSpaceSupported(type.color_space)) {
        return true;
      }
#endif
      return false;

    case kCodecVP9:
      // Color management required for HDR to not look terrible.
      return IsColorSpaceSupported(type.color_space) &&
             IsVp9ProfileSupported(type.profile);
    case kCodecH264:
    case kCodecVP8:
    case kCodecTheora:
      return true;

    case kUnknownVideoCodec:
    case kCodecVC1:
    case kCodecMPEG2:
    case kCodecHEVC:
    case kCodecDolbyVision:
      return false;

    case kCodecMPEG4:
#if defined(OS_CHROMEOS)
      return true;
#else
      return false;
#endif
  }

  NOTREACHED();
  return false;
}

}  // namespace media

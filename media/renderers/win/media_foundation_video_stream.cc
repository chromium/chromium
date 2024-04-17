// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_video_stream.h"

#include <initguid.h>

#include <mfapi.h>
#include <mferror.h>
#include <wrl.h>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/win/mf_helpers.h"
#include "media/media_buildflags.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// MF_MT_MIN_MASTERING_LUMINANCE values are in 1/10000th of a nit (0.0001 nit).
// https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10
constexpr int kMasteringDispLuminanceScale = 10000;

MFVideoRotationFormat VideoRotationToMF(VideoRotation rotation) {
  DVLOG(2) << __func__ << ": rotation=" << rotation;

  switch (rotation) {
    case VideoRotation::VIDEO_ROTATION_0:
      return MFVideoRotationFormat_0;
    case VideoRotation::VIDEO_ROTATION_90:
      return MFVideoRotationFormat_90;
    case VideoRotation::VIDEO_ROTATION_180:
      return MFVideoRotationFormat_180;
    case VideoRotation::VIDEO_ROTATION_270:
      return MFVideoRotationFormat_270;
  }
}

MFVideoPrimaries VideoPrimariesToMF(
    media::VideoColorSpace::PrimaryID primary_id) {
  DVLOG(2) << __func__ << ": primary_id=" << static_cast<int>(primary_id);

  switch (primary_id) {
    case VideoColorSpace::PrimaryID::INVALID:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::BT709:
      return MFVideoPrimaries_BT709;
    case VideoColorSpace::PrimaryID::UNSPECIFIED:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::BT470M:
      return MFVideoPrimaries_BT470_2_SysM;
    case VideoColorSpace::PrimaryID::BT470BG:
      return MFVideoPrimaries_BT470_2_SysBG;
    case VideoColorSpace::PrimaryID::SMPTE170M:
      return MFVideoPrimaries_SMPTE170M;
    case VideoColorSpace::PrimaryID::SMPTE240M:
      return MFVideoPrimaries_SMPTE240M;
    case VideoColorSpace::PrimaryID::FILM:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::BT2020:
      return MFVideoPrimaries_BT2020;
    case VideoColorSpace::PrimaryID::SMPTEST428_1:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::SMPTEST431_2:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::SMPTEST432_1:
      return MFVideoPrimaries_Unknown;
    case VideoColorSpace::PrimaryID::EBU_3213_E:
      return MFVideoPrimaries_EBU3213;
    default:
      DLOG(ERROR) << "VideoPrimariesToMF failed due to invalid Video Primary.";
  }

  return MFVideoPrimaries_Unknown;
}

MFVideoTransferFunction VideoTransferFunctionToMF(
    media::VideoColorSpace::TransferID transfer_id) {
  DVLOG(2) << __func__ << ": transfer_id=" << static_cast<int>(transfer_id);

  switch (transfer_id) {
    case VideoColorSpace::TransferID::INVALID:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::BT709:
      return MFVideoTransFunc_709;
    case VideoColorSpace::TransferID::UNSPECIFIED:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::GAMMA22:
      return MFVideoTransFunc_22;
    case VideoColorSpace::TransferID::GAMMA28:
      return MFVideoTransFunc_28;
    case VideoColorSpace::TransferID::SMPTE170M:
      return MFVideoTransFunc_709;
    case VideoColorSpace::TransferID::SMPTE240M:
      return MFVideoTransFunc_240M;
    case VideoColorSpace::TransferID::LINEAR:
      return MFVideoTransFunc_10;
    case VideoColorSpace::TransferID::LOG:
      return MFVideoTransFunc_Log_100;
    case VideoColorSpace::TransferID::LOG_SQRT:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::IEC61966_2_4:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::BT1361_ECG:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::IEC61966_2_1:
      return MFVideoTransFunc_sRGB;
    case VideoColorSpace::TransferID::BT2020_10:
      return MFVideoTransFunc_2020;
    case VideoColorSpace::TransferID::BT2020_12:
      return MFVideoTransFunc_2020;
    case VideoColorSpace::TransferID::SMPTEST2084:
      return MFVideoTransFunc_2084;
    case VideoColorSpace::TransferID::SMPTEST428_1:
      return MFVideoTransFunc_Unknown;
    case VideoColorSpace::TransferID::ARIB_STD_B67:
      return MFVideoTransFunc_HLG;
    default:
      DLOG(ERROR) << "VideoTransferFunctionToMF failed due to invalid Transfer "
                     "Function.";
  }

  return MFVideoTransFunc_Unknown;
}

MT_CUSTOM_VIDEO_PRIMARIES CustomVideoPrimaryToMF(
    const gfx::HdrMetadataSmpteSt2086& smpte_st_2086) {
  // MT_CUSTOM_VIDEO_PRIMARIES stores value in float no scaling factor needed
  // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/ns-mfapi-mt_custom_video_primaries
  MT_CUSTOM_VIDEO_PRIMARIES primaries = {0};
  primaries.fRx = smpte_st_2086.primaries.fRX;
  primaries.fRy = smpte_st_2086.primaries.fRY;
  primaries.fGx = smpte_st_2086.primaries.fGX;
  primaries.fGy = smpte_st_2086.primaries.fGY;
  primaries.fBx = smpte_st_2086.primaries.fBX;
  primaries.fBy = smpte_st_2086.primaries.fBY;
  primaries.fWx = smpte_st_2086.primaries.fWX;
  primaries.fWy = smpte_st_2086.primaries.fWY;
  return primaries;
}

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
// To MediaFoundation, DolbyVision renderer profile strings are always 7
// characters. For HEVC based profiles, it's in the format "dvhe.xx". For AVC
// based profiles, it's in the format "dvav.xx". In both cases, "xx" is the
// two-digit profile number. Note the DolbyVision level is ignored.
bool GetDolbyVisionConfigurations(
    VideoCodecProfile profile,
    std::wstring& dolby_vision_profile,
    unsigned& dolby_vision_configuration_nalu_type) {
  DVLOG(2) << __func__ << ": profile=" << profile;
  switch (profile) {
    case VideoCodecProfile::DOLBYVISION_PROFILE0:
      DLOG(ERROR) << __func__ << ": Profile 0 unsupported by Media Foundation";
      return false;
    case VideoCodecProfile::DOLBYVISION_PROFILE5:
      dolby_vision_profile = L"dvhe.05";
      dolby_vision_configuration_nalu_type = 62;
      break;
    case VideoCodecProfile::DOLBYVISION_PROFILE7:
      dolby_vision_profile = L"dvhe.07";
      dolby_vision_configuration_nalu_type = 62;
      break;
    case VideoCodecProfile::DOLBYVISION_PROFILE8:
      dolby_vision_profile = L"dvhe.08";
      dolby_vision_configuration_nalu_type = 62;
      break;
    case VideoCodecProfile::DOLBYVISION_PROFILE9:
      dolby_vision_profile = L"dvav.09";
      dolby_vision_configuration_nalu_type = 28;
      break;
    default:
      DLOG(ERROR) << __func__ << ": Invalid profile (" << profile << ")";
      return false;
  }

  return true;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)

// https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/ns-mfobjects-mfoffset
// The value of the MFOffset number is value + (fract / 65536.0f).
MFOffset MakeOffset(float value) {
  MFOffset offset;
  offset.value = base::checked_cast<short>(value);
  offset.fract = base::checked_cast<WORD>(65536 * (value - offset.value));
  return offset;
}

// TODO(frankli): investigate if VideoCodecProfile is needed by MF.
HRESULT GetVideoType(const VideoDecoderConfig& config,
                     IMFMediaType** media_type_out) {
  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

  GUID mf_subtype = VideoCodecToMFSubtype(config.codec(), config.profile());
  if (mf_subtype == GUID_NULL) {
    DLOG(ERROR) << "Unsupported codec type: " << config.codec();
    return MF_E_TOPO_CODEC_NOT_FOUND;
  }
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_SUBTYPE, mf_subtype));

  UINT32 width = config.visible_rect().width();
  UINT32 height = config.visible_rect().height();
  RETURN_IF_FAILED(
      MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, width, height));

  UINT32 natural_width = config.natural_size().width();
  UINT32 natural_height = config.natural_size().height();
  RETURN_IF_FAILED(
      MFSetAttributeRatio(media_type.Get(), MF_MT_PIXEL_ASPECT_RATIO,
                          height * natural_width, width * natural_height));

  MFVideoArea area;
  area.OffsetX = MakeOffset(static_cast<float>(config.visible_rect().x()));
  area.OffsetY = MakeOffset(static_cast<float>(config.visible_rect().y()));
  area.Area = config.natural_size().ToSIZE();
  RETURN_IF_FAILED(media_type->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area,
                                       sizeof(area)));

  if (config.video_transformation().rotation !=
      VideoRotation::VIDEO_ROTATION_0) {
    MFVideoRotationFormat mf_rotation =
        VideoRotationToMF(config.video_transformation().rotation);
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_VIDEO_ROTATION, mf_rotation));
  }

  MFVideoTransferFunction mf_transfer_function =
      VideoTransferFunctionToMF(config.color_space_info().transfer);
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_TRANSFER_FUNCTION, mf_transfer_function));

  MFVideoPrimaries mf_video_primary =
      VideoPrimariesToMF(config.color_space_info().primaries);
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_VIDEO_PRIMARIES, mf_video_primary));

  UINT32 video_nominal_range =
      config.color_space_info().range == gfx::ColorSpace::RangeID::FULL
          ? MFNominalRange_0_255
          : MFNominalRange_16_235;
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, video_nominal_range));

  {
    const auto hdr_metadata = gfx::HDRMetadata::PopulateUnspecifiedWithDefaults(
        config.hdr_metadata());
    UINT32 max_display_mastering_luminance =
        hdr_metadata.smpte_st_2086->luminance_max;
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_MAX_MASTERING_LUMINANCE,
                                           max_display_mastering_luminance));

    UINT32 min_display_mastering_luminance =
        hdr_metadata.smpte_st_2086->luminance_min *
        kMasteringDispLuminanceScale;
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_MIN_MASTERING_LUMINANCE,
                                           min_display_mastering_luminance));

    MT_CUSTOM_VIDEO_PRIMARIES primaries =
        CustomVideoPrimaryToMF(hdr_metadata.smpte_st_2086.value());
    RETURN_IF_FAILED(media_type->SetBlob(MF_MT_CUSTOM_VIDEO_PRIMARIES,
                                         reinterpret_cast<UINT8*>(&primaries),
                                         sizeof(MT_CUSTOM_VIDEO_PRIMARIES)));

    if (hdr_metadata.cta_861_3.has_value()) {
      UINT32 max_luminance_level =
          hdr_metadata.cta_861_3->max_content_light_level;
      RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_MAX_LUMINANCE_LEVEL,
                                             max_luminance_level));

      UINT32 max_frame_average_luminance_level =
          hdr_metadata.cta_861_3->max_frame_average_light_level;
      RETURN_IF_FAILED(
          media_type->SetUINT32(MF_MT_MAX_FRAME_AVERAGE_LUMINANCE_LEVEL,
                                max_frame_average_luminance_level));
    }
  }

#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
  if (config.codec() == VideoCodec::kDolbyVision) {
    std::wstring dolby_vision_profile;
    // This value should be 28 for AVC and 62 for HEVC.
    unsigned dolby_vision_configuration_nalu_type = 0;
    if (!GetDolbyVisionConfigurations(config.profile(), dolby_vision_profile,
                                      dolby_vision_configuration_nalu_type)) {
      return MF_E_TOPO_CODEC_NOT_FOUND;
    }

    media_type->SetString(MF_MT_VIDEO_RENDERER_EXTENSION_PROFILE,
                          dolby_vision_profile.c_str());
    media_type->SetUINT32(MF_MT_FORWARD_CUSTOM_NALU,
                          dolby_vision_configuration_nalu_type);
    media_type->SetUINT32(MF_DECODER_FWD_CUSTOM_SEI_DECODE_ORDER, TRUE);
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)

  *media_type_out = media_type.Detach();
  return S_OK;
}

}  // namespace

/*static*/
HRESULT MediaFoundationVideoStream::Create(
    int stream_id,
    IMFMediaSource* parent_source,
    DemuxerStream* demuxer_stream,
    std::unique_ptr<MediaLog> media_log,
    MediaFoundationStreamWrapper** stream_out) {
  DVLOG(1) << __func__ << ": stream_id=" << stream_id;

  ComPtr<IMFMediaStream> video_stream;
  VideoCodec codec = demuxer_stream->video_decoder_config().codec();
  switch (codec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case VideoCodec::kH264:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationH264VideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case VideoCodec::kHEVC:
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    case VideoCodec::kDolbyVision:
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) || BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationHEVCVideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) ||
        // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    default:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationVideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
  }

  *stream_out =
      static_cast<MediaFoundationStreamWrapper*>(video_stream.Detach());
  return S_OK;
}

bool MediaFoundationVideoStream::IsEncrypted() const {
  VideoDecoderConfig decoder_config = demuxer_stream_->video_decoder_config();
  return decoder_config.is_encrypted();
}

HRESULT MediaFoundationVideoStream::GetMediaType(
    IMFMediaType** media_type_out) {
  VideoDecoderConfig decoder_config = demuxer_stream_->video_decoder_config();
  return GetVideoType(decoder_config, media_type_out);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
HRESULT MediaFoundationH264VideoStream::GetMediaType(
    IMFMediaType** media_type_out) {
  VideoDecoderConfig decoder_config = demuxer_stream_->video_decoder_config();
  RETURN_IF_FAILED(GetVideoType(decoder_config, media_type_out));
  // Enable conversion to Annex-B
  demuxer_stream_->EnableBitstreamConverter();
  return S_OK;
}

bool MediaFoundationH264VideoStream::AreFormatChangesEnabled() {
  // Disable explicit format change event for H264 to allow switching to the
  // new stream without a full re-create, which will be much faster. This is
  // also due to the fact that the MFT decoder can handle some format changes
  // without a format change event. For format changes that the MFT decoder
  // cannot support (e.g. codec change), the playback will fail later with
  // MF_E_INVALIDMEDIATYPE (0xC00D36B4).
  return false;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
HRESULT MediaFoundationHEVCVideoStream::GetMediaType(
    IMFMediaType** media_type_out) {
  RETURN_IF_FAILED(MediaFoundationVideoStream::GetMediaType(media_type_out));
  // Enable conversion to Annex-B
  demuxer_stream_->EnableBitstreamConverter();
  return S_OK;
}

bool MediaFoundationHEVCVideoStream::AreFormatChangesEnabled() {
  // Disable explicit format change event for HEVC to allow switching to the
  // new stream without a full re-create, which will be much faster. This is
  // also due to the fact that the MFT decoder can handle some format changes
  // without a format change event. For format changes that the MFT decoder
  // cannot support (e.g. codec change), the playback will fail later with
  // MF_E_INVALIDMEDIATYPE (0xC00D36B4).
  return false;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

}  // namespace media

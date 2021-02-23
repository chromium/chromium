// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_foundation_video_stream.h"

#include <initguid.h>  // NOLINT(build/include_order)
#include <mferror.h>   // NOLINT(build/include_order)
#include <wrl.h>       // NOLINT(build/include_order)

#include "base/numerics/safe_conversions.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// This is supported by Media Foundation.
DEFINE_MEDIATYPE_GUID(MFVideoFormat_THEORA, FCC('theo'))

GUID VideoCodecToMFSubtype(VideoCodec codec) {
  switch (codec) {
    case kCodecH264:
      return MFVideoFormat_H264;
    case kCodecVC1:
      return MFVideoFormat_WVC1;
    case kCodecMPEG2:
      return MFVideoFormat_MPEG2;
    case kCodecMPEG4:
      return MFVideoFormat_MP4V;
    case kCodecTheora:
      return MFVideoFormat_THEORA;
    case kCodecVP8:
      return MFVideoFormat_VP80;
    case kCodecVP9:
      return MFVideoFormat_VP90;
    case kCodecHEVC:
      return MFVideoFormat_HEVC;
    case kCodecDolbyVision:
      // TODO(frankli): DolbyVision also supports H264 when the profile ID is 9
      // (DOLBYVISION_PROFILE9). Will it be fine to use HEVC?
      return MFVideoFormat_HEVC;
    case kCodecAV1:
      return MFVideoFormat_AV1;
    default:
      return GUID_NULL;
  }
}

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

//
// https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/ns-mfobjects-mfoffset
// The value of the MFOffset number is value + (fract / 65536.0f).
MFOffset MakeOffset(float value) {
  MFOffset offset;
  offset.value = base::checked_cast<short>(value);
  offset.fract = base::checked_cast<WORD>(65536 * (value - offset.value));
  return offset;
}

// TODO(frankli): investigate if VideoCodecProfile is needed by MF.
HRESULT GetVideoType(const VideoDecoderConfig& decoder_config,
                     IMFMediaType** media_type_out) {
  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));

  GUID mf_subtype = VideoCodecToMFSubtype(decoder_config.codec());
  if (mf_subtype == GUID_NULL) {
    DLOG(ERROR) << "Unsupported codec type: " << decoder_config.codec();
    return MF_E_TOPO_CODEC_NOT_FOUND;
  }
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_SUBTYPE, mf_subtype));

  UINT32 width = decoder_config.visible_rect().width();
  UINT32 height = decoder_config.visible_rect().height();
  RETURN_IF_FAILED(
      MFSetAttributeSize(media_type.Get(), MF_MT_FRAME_SIZE, width, height));

  UINT32 natural_width = decoder_config.natural_size().width();
  UINT32 natural_height = decoder_config.natural_size().height();
  RETURN_IF_FAILED(
      MFSetAttributeRatio(media_type.Get(), MF_MT_PIXEL_ASPECT_RATIO,
                          height * natural_width, width * natural_height));

  MFVideoArea area;
  area.OffsetX =
      MakeOffset(static_cast<float>(decoder_config.visible_rect().x()));
  area.OffsetY =
      MakeOffset(static_cast<float>(decoder_config.visible_rect().y()));
  area.Area = decoder_config.natural_size().ToSIZE();
  RETURN_IF_FAILED(media_type->SetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area,
                                       sizeof(area)));

  if (decoder_config.video_transformation().rotation !=
      VideoRotation::VIDEO_ROTATION_0) {
    MFVideoRotationFormat mf_rotation =
        VideoRotationToMF(decoder_config.video_transformation().rotation);
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_VIDEO_ROTATION, mf_rotation));
  }

  *media_type_out = media_type.Detach();
  return S_OK;
}

}  // namespace

/*static*/
HRESULT MediaFoundationVideoStream::Create(
    int stream_id,
    IMFMediaSource* parent_source,
    DemuxerStream* demuxer_stream,
    MediaFoundationStreamWrapper** stream_out) {
  DVLOG(1) << __func__ << ": stream_id=" << stream_id;

  ComPtr<IMFMediaStream> video_stream;
  VideoCodec codec = demuxer_stream->video_decoder_config().codec();
  switch (codec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case kCodecH264:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationH264VideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream));
      break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
    case kCodecHEVC:
#endif
#if BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    case kCodecDolbyVision:
#endif
#if BUILDFLAG(ENABLE_PLATFORM_HEVC) || BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationHEVCVideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream));
      break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC) ||
        // BUILDFLAG(ENABLE_PLATFORM_DOLBY_VISION)
    default:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationVideoStream>(
          &video_stream, stream_id, parent_source, demuxer_stream));
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
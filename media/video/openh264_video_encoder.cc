// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/openh264_video_encoder.h"

#include <algorithm>
#include <limits>
#include <numeric>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/video_encoder_info.h"

namespace media {

namespace {

EProfileIdc ToOpenH264Profile(VideoCodecProfile profile) {
  switch (profile) {
    case media::H264PROFILE_BASELINE:
      return PRO_BASELINE;
    case media::H264PROFILE_MAIN:
      return PRO_MAIN;
    case media::H264PROFILE_HIGH:
      return PRO_HIGH;
    default:
      return PRO_UNKNOWN;
  }
}

void SetUpOpenH264Params(VideoCodecProfile profile,
                         const VideoEncoder::Options& options,
                         const VideoColorSpace& itu_cs,
                         SEncParamExt* params) {
  int threads = GetNumberOfThreadsForSoftwareEncoding(options.frame_size);
  params->bEnableFrameSkip =
      base::FeatureList::IsEnabled(kWebCodecsVideoEncoderFrameDrop) &&
      options.latency_mode == VideoEncoder::LatencyMode::Realtime;
  params->iPaddingFlag = 0;
  params->iComplexityMode = MEDIUM_COMPLEXITY;
  params->iUsageType = options.content_hint == VideoEncoder::ContentHint::Screen
                           ? SCREEN_CONTENT_REAL_TIME
                           : CAMERA_VIDEO_REAL_TIME;
  params->bEnableDenoise = false;
  params->eSpsPpsIdStrategy = SPS_LISTING;
  params->iMultipleThreadIdc = threads;
  if (options.framerate.has_value())
    params->fMaxFrameRate = options.framerate.value();
  params->iPicHeight = options.frame_size.height();
  params->iPicWidth = options.frame_size.width();

  if (options.keyframe_interval.has_value())
    params->uiIntraPeriod = options.keyframe_interval.value();

  if (options.bitrate.has_value()) {
    auto& bitrate = options.bitrate.value();
    params->iRCMode = RC_BITRATE_MODE;
    if (bitrate.target_bps() != 0) {
      params->iTargetBitrate = base::saturated_cast<int>(bitrate.target_bps());
    } else {
      params->iTargetBitrate =
          base::saturated_cast<int>(GetDefaultVideoEncodeBitrate(
              options.frame_size, options.framerate.value_or(30)));
    }
  } else {
    params->iRCMode = RC_OFF_MODE;
  }

  int num_temporal_layers = 1;
  if (options.scalability_mode) {
    switch (options.scalability_mode.value()) {
      case SVCScalabilityMode::kL1T1:
        // Nothing to do
        break;
      case SVCScalabilityMode::kL1T2:
        num_temporal_layers = 2;
        break;
      case SVCScalabilityMode::kL1T3:
        num_temporal_layers = 3;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Unsupported SVC: "
            << GetScalabilityModeName(options.scalability_mode.value());
    }
  }

  if (options.content_hint == VideoEncoder::ContentHint::Screen &&
      num_temporal_layers > 2) {
    // Currently, OpenH264 only supports up to 2 temporal layers for screen
    // content. Otherwise,
    // https://github.com/cisco/openh264/blob/6a6cb82eef8e83bc52bafab21e996f5d3e211eb4/codec/encoder/core/src/ref_list_mgr_svc.cpp#L650
    // triggers an assert.
    // See https://bugs.webrtc.org/15582 for more details.
    LOG(ERROR) << "Screen content only supports up to 2 temporal layers.";
    params->iUsageType = CAMERA_VIDEO_REAL_TIME;
  }

  params->iTemporalLayerNum = num_temporal_layers;
  params->iSpatialLayerNum = 1;
  auto& layer = params->sSpatialLayers[0];
  layer.fFrameRate = params->fMaxFrameRate;
  layer.uiProfileIdc = ToOpenH264Profile(profile);
  layer.iMaxSpatialBitrate = params->iTargetBitrate;
  layer.iSpatialBitrate = params->iTargetBitrate;
  layer.iVideoHeight = params->iPicHeight;
  layer.iVideoWidth = params->iPicWidth;
  if (threads > 1) {
    layer.sSliceArgument.uiSliceMode = SM_FIXEDSLCNUM_SLICE;
    layer.sSliceArgument.uiSliceNum = threads;
  } else {
    layer.sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
  }

  if (!itu_cs.IsSpecified())
    return;

  layer.bVideoSignalTypePresent = true;
  layer.bColorDescriptionPresent = true;

  if (itu_cs.primaries != VideoColorSpace::PrimaryID::INVALID &&
      itu_cs.primaries != VideoColorSpace::PrimaryID::UNSPECIFIED) {
    layer.uiColorPrimaries = static_cast<unsigned char>(itu_cs.primaries);
  }
  if (itu_cs.transfer != VideoColorSpace::TransferID::INVALID &&
      itu_cs.transfer != VideoColorSpace::TransferID::UNSPECIFIED) {
    layer.uiTransferCharacteristics =
        static_cast<unsigned char>(itu_cs.transfer);
  }
  if (itu_cs.matrix != VideoColorSpace::MatrixID::INVALID &&
      itu_cs.matrix != VideoColorSpace::MatrixID::UNSPECIFIED) {
    layer.uiColorMatrix = static_cast<unsigned char>(itu_cs.matrix);
  }
  if (itu_cs.range == gfx::ColorSpace::RangeID::FULL ||
      itu_cs.range == gfx::ColorSpace::RangeID::LIMITED) {
    layer.bFullRange = itu_cs.range == gfx::ColorSpace::RangeID::FULL;
  }
}

}  // namespace

OpenH264VideoEncoder::ISVCEncoderDeleter::ISVCEncoderDeleter() = default;
OpenH264VideoEncoder::ISVCEncoderDeleter::ISVCEncoderDeleter(
    const ISVCEncoderDeleter&) = default;
OpenH264VideoEncoder::ISVCEncoderDeleter&
OpenH264VideoEncoder::ISVCEncoderDeleter::operator=(const ISVCEncoderDeleter&) =
    default;
void OpenH264VideoEncoder::ISVCEncoderDeleter::operator()(ISVCEncoder* codec) {
  if (codec) {
    if (initialized_) {
      auto result = codec->Uninitialize();
      DCHECK_EQ(cmResultSuccess, result);
    }
    WelsDestroySVCEncoder(codec);
  }
}
void OpenH264VideoEncoder::ISVCEncoderDeleter::MarkInitialized() {
  initialized_ = true;
}

OpenH264VideoEncoder::OpenH264VideoEncoder() : codec_() {}
OpenH264VideoEncoder::~OpenH264VideoEncoder() = default;

void OpenH264VideoEncoder::Initialize(VideoCodecProfile profile,
                                      const Options& options,
                                      EncoderInfoCB info_cb,
                                      OutputCB output_cb,
                                      EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (codec_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  if (options.subsampling.value_or(VideoChromaSampling::k420) !=
          VideoChromaSampling::k420 ||
      options.bit_depth.value_or(8) != 8) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Unsupported subsampling or bit depth"));
    return;
  }

  if (ToOpenH264Profile(profile) == PRO_UNKNOWN) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedProfile,
                      "Unsupported profile: " + GetProfileName(profile)));
    return;
  }
  profile_ = profile;

  if (options.bitrate.has_value() &&
      options.bitrate->mode() == Bitrate::Mode::kExternal) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Unsupported bitrate mode"));
    return;
  }

  ISVCEncoder* raw_codec = nullptr;
  if (WelsCreateSVCEncoder(&raw_codec) != 0) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to create OpenH264 encoder."));
    return;
  }
  svc_encoder_unique_ptr codec(raw_codec, ISVCEncoderDeleter());
  raw_codec = nullptr;

  SEncParamExt params = {};
  if (int err = codec->GetDefaultParams(&params)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to get default params.")
            .WithData("error", err));
    return;
  }

  if (options.frame_size.height() < 16 || options.frame_size.width() < 16) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedConfig,
                      "Unsupported frame size which is less than 16"));
    return;
  }
  SetUpOpenH264Params(
      profile_, options,
      VideoColorSpace::FromGfxColorSpace(last_frame_color_space_), &params);

  if (int err = codec->InitializeExt(&params)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to initialize OpenH264 encoder.")
            .WithData("error", err));
    return;
  }
  codec.get_deleter().MarkInitialized();

  int video_format = EVideoFormatType::videoFormatI420;
  if (int err = codec->SetOption(ENCODER_OPTION_DATAFORMAT, &video_format)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to set data format for OpenH264 encoder")
            .WithData("error", err));
    return;
  }

  if (!options.avc.produce_annexb)
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();

  options_ = options;
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  codec_ = std::move(codec);

  if (info_cb) {
    VideoEncoderInfo info;
    info.implementation_name = "OpenH264VideoEncoder";
    info.is_hardware_accelerated = false;
    BindCallbackToCurrentLoopIfNeeded(std::move(info_cb)).Run(info);
  }

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

EncoderStatus OpenH264VideoEncoder::DrainOutputs(const SFrameBSInfo& frame_info,
                                                 base::TimeDelta timestamp,
                                                 gfx::ColorSpace color_space) {
  VideoEncoderOutput result;
  result.timestamp = timestamp;

  const size_t total_chunk_size = frame_info.iFrameSizeInBytes;
  if (total_chunk_size == 0) {
    // Drop frame.
    output_cb_.Run(std::move(result), {});
    return EncoderStatus::Codes::kOk;
  }

  result.key_frame = (frame_info.eFrameType == videoFrameTypeIDR);
  result.color_space = color_space;
  result.data = base::HeapArray<uint8_t>::Uninit(total_chunk_size);
  auto gather_buffer = result.data.as_span();

  if (h264_converter_) {
    // Copy data to a temporary buffer instead.
    conversion_buffer_.resize(total_chunk_size);
    gather_buffer = base::span(conversion_buffer_);
  }

  result.temporal_id = -1;
  size_t written_size = 0;
  auto frame_layer_info = base::span(frame_info.sLayerInfo);
  for (int layer_idx = 0; layer_idx < frame_info.iLayerNum; ++layer_idx) {
    const SLayerBSInfo& layer_info = frame_layer_info[layer_idx];

    // All layers in the same frame must have the same temporal_id.
    if (result.temporal_id == -1) {
      result.temporal_id = layer_info.uiTemporalId;
    } else if (result.temporal_id != layer_info.uiTemporalId) {
      return EncoderStatus::Codes::kEncoderFailedEncode;
    }

    // SAFETY: OpenH264 documents that layer_info.pNalLengthInByte has
    // layer_info.iNalCount elements.
    UNSAFE_BUFFERS(
        base::span nal_len_bytes(layer_info.pNalLengthInByte,
                                 static_cast<size_t>(layer_info.iNalCount)));
    size_t layer_len =
        std::accumulate(nal_len_bytes.begin(), nal_len_bytes.end(), 0);
    if (written_size + layer_len > total_chunk_size) {
      return EncoderStatus::Codes::kEncoderFailedEncode;
    }

    // SAFETY: The whole buffer size is equal to the size of all NALs combined.
    UNSAFE_BUFFERS(base::span layer_data(layer_info.pBsBuf, layer_len));
    gather_buffer.subspan(written_size, layer_len).copy_from(layer_data);
    written_size += layer_len;
  }
  DCHECK_EQ(written_size, total_chunk_size);

  if (!h264_converter_) {
    output_cb_.Run(std::move(result), std::optional<CodecDescription>());
    return EncoderStatus::Codes::kOk;
  }

  size_t converted_output_size = 0;
  bool config_changed = false;
  MP4Status status = OkStatus();
  do {
    status =
        h264_converter_->ConvertChunk(conversion_buffer_, result.data,
                                      &config_changed, &converted_output_size);
    if (status.code() == MP4Status::Codes::kBufferTooSmall) {
      result.data = base::HeapArray<uint8_t>::Uninit(converted_output_size);
      continue;
    } else if (!status.is_ok()) {
      return EncoderStatus(EncoderStatus::Codes::kBitstreamConversionError)
          .AddCause(std::move(status));
    }
  } while (!status.is_ok());

  result.data = std::move(result.data).take_first(converted_output_size);

  std::optional<CodecDescription> desc;
  if (config_changed) {
    const auto& config = h264_converter_->GetCurrentConfig();
    desc = CodecDescription();
    if (!config.Serialize(desc.value())) {
      return EncoderStatus(EncoderStatus::Codes::kBitstreamConversionError,
                           "Failed to serialize AVC decoder config");
    }
  }

  output_cb_.Run(std::move(result), std::move(desc));
  return EncoderStatus::Codes::kOk;
}

void OpenH264VideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                                  const EncodeOptions& encode_options,
                                  EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  if (!frame) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kInvalidInputFrame,
                      "No frame provided for encoding."));
    return;
  }
  const bool supported_format = frame->format() == PIXEL_FORMAT_NV12 ||
                                frame->format() == PIXEL_FORMAT_I420 ||
                                frame->format() == PIXEL_FORMAT_XBGR ||
                                frame->format() == PIXEL_FORMAT_XRGB ||
                                frame->format() == PIXEL_FORMAT_ABGR ||
                                frame->format() == PIXEL_FORMAT_ARGB;
  if ((!frame->IsMappable() && !frame->HasMappableGpuBuffer()) ||
      !supported_format) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kUnsupportedFrameFormat,
                      "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("storage type", frame->storage_type())
            .WithData("format", frame->format()));
    return;
  }

  if (frame->format() == PIXEL_FORMAT_NV12 && frame->HasMappableGpuBuffer()) {
    frame = ConvertToMemoryMappedFrame(frame);
    if (!frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kSystemAPICallError,
                        "Convert GMB frame to MemoryMappedFrame failed."));
      return;
    }
  }

  if (frame->format() != PIXEL_FORMAT_I420) {
    // OpenH264 can resize frame automatically, but since we're converting
    // pixel format anyway we can do resize as well.
    auto i420_frame = frame_pool_.CreateFrame(
        PIXEL_FORMAT_I420, options_.frame_size, gfx::Rect(options_.frame_size),
        options_.frame_size, frame->timestamp());
    if (!i420_frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kOutOfMemoryError,
                        "Can't allocate an I420 frame."));
      return;
    }
    auto status = frame_converter_.ConvertAndScale(*frame, *i420_frame);
    if (!status.is_ok()) {
      std::move(done_cb).Run(status);
      return;
    }
    frame = std::move(i420_frame);
  }

  bool key_frame = encode_options.key_frame;
  if (last_frame_color_space_ != frame->ColorSpace()) {
    last_frame_color_space_ = frame->ColorSpace();
    key_frame = true;
    UpdateEncoderColorSpace();
  }

  SSourcePicture picture = {};
  picture.iPicWidth = frame->visible_rect().width();
  picture.iPicHeight = frame->visible_rect().height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp = frame->timestamp().InMilliseconds();
  auto picture_data = base::span(picture.pData);
  picture_data[0] =
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kY));
  picture_data[1] =
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kU));
  picture_data[2] =
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::Plane::kV));
  auto picture_stride = base::span(picture.iStride);
  picture_stride[0] = frame->stride(VideoFrame::Plane::kY);
  picture_stride[1] = frame->stride(VideoFrame::Plane::kU);
  picture_stride[2] = frame->stride(VideoFrame::Plane::kV);

  if (key_frame) {
    if (int err = codec_->ForceIntraFrame(true)) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Can't make keyframe.")
              .WithData("error", err));
      return;
    }
  }

  SFrameBSInfo frame_info = {};
  TRACE_EVENT1("media", "OpenH264::EncodeFrame", "timestamp",
               frame->timestamp());
  if (int err = codec_->EncodeFrame(&picture, &frame_info)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                      "Failed to encode using OpenH264.")
            .WithData("error", err));
    return;
  }

  std::move(done_cb).Run(
      DrainOutputs(frame_info, frame->timestamp(), frame->ColorSpace()));
}

void OpenH264VideoEncoder::ChangeOptions(const Options& options,
                                         OutputCB output_cb,
                                         EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  SEncParamExt params = {};
  if (int err = codec_->GetDefaultParams(&params)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Failed to get default params.")
            .WithData("error", err));
    return;
  }

  SetUpOpenH264Params(
      profile_, options,
      VideoColorSpace::FromGfxColorSpace(last_frame_color_space_), &params);

  if (int err =
          codec_->SetOption(ENCODER_OPTION_SVC_ENCODE_PARAM_EXT, &params)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "OpenH264 encoder failed to set new SEncParamExt.")
            .WithData("error", err));
    return;
  }

  if (options.avc.produce_annexb) {
    h264_converter_.reset();
  } else if (!h264_converter_) {
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
  }

  options_ = options;
  if (!output_cb.is_null())
    output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void OpenH264VideoEncoder::Flush(EncoderStatusCB done_cb) {
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  // Nothing to do really.
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void OpenH264VideoEncoder::UpdateEncoderColorSpace() {
  auto itu_cs = VideoColorSpace::FromGfxColorSpace(last_frame_color_space_);
  if (!itu_cs.IsSpecified())
    return;

  SEncParamExt params = {};
  if (int err = codec_->GetDefaultParams(&params)) {
    DLOG(ERROR) << "Failed to GetDefaultParams to set color space: " << err;
    return;
  }

  SetUpOpenH264Params(profile_, options_, itu_cs, &params);

  // It'd be nice if SetOption(ENCODER_OPTION_SVC_ENCODE_PARAM_EXT) worked, but
  // alas it doesn't seem to, so we must reinitialize.
  if (int err = codec_->InitializeExt(&params))
    DLOG(ERROR) << "Failed to reinitialize codec to set color space: " << err;
}

}  // namespace media

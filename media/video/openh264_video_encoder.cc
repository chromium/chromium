// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/openh264_video_encoder.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/video_encoder_info.h"

namespace media {

namespace {

void SetUpOpenH264Params(const VideoEncoder::Options& options,
                         const VideoColorSpace& itu_cs,
                         SEncParamExt* params) {
  int threads = GetNumberOfThreadsForSoftwareEncoding(options.frame_size);
  params->bEnableFrameSkip = false;
  params->iPaddingFlag = 0;
  params->iComplexityMode = MEDIUM_COMPLEXITY;
  params->iUsageType = CAMERA_VIDEO_REAL_TIME;
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
    params->iTargetBitrate = base::saturated_cast<int>(bitrate.target_bps());
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
        NOTREACHED() << "Unsupported SVC: "
                     << GetScalabilityModeName(
                            options.scalability_mode.value());
    }
  }

  params->iTemporalLayerNum = num_temporal_layers;
  params->iSpatialLayerNum = 1;
  auto& layer = params->sSpatialLayers[0];
  layer.fFrameRate = params->fMaxFrameRate;
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

  profile_ = profile;
  if (profile != H264PROFILE_BASELINE) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderUnsupportedProfile,
                      "Unsupported profile"));
    return;
  }

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
      options, VideoColorSpace::FromGfxColorSpace(last_frame_color_space_),
      &params);

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

  VideoEncoderInfo info;
  info.implementation_name = "OpenH264VideoEncoder";
  info.is_hardware_accelerated = false;
  BindCallbackToCurrentLoopIfNeeded(std::move(info_cb)).Run(info);

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

EncoderStatus OpenH264VideoEncoder::DrainOutputs(const SFrameBSInfo& frame_info,
                                                 base::TimeDelta timestamp,
                                                 gfx::ColorSpace color_space) {
  VideoEncoderOutput result;
  result.key_frame = (frame_info.eFrameType == videoFrameTypeIDR);
  result.timestamp = timestamp;
  result.color_space = color_space;

  DCHECK_GT(frame_info.iFrameSizeInBytes, 0);
  size_t total_chunk_size = frame_info.iFrameSizeInBytes;
  result.data = std::make_unique<uint8_t[]>(total_chunk_size);
  auto* gather_buffer = result.data.get();

  if (h264_converter_) {
    // Copy data to a temporary buffer instead.
    conversion_buffer_.resize(total_chunk_size);
    gather_buffer = conversion_buffer_.data();
  }

  result.temporal_id = -1;
  size_t written_size = 0;
  for (int layer_idx = 0; layer_idx < frame_info.iLayerNum; ++layer_idx) {
    const SLayerBSInfo& layer_info = frame_info.sLayerInfo[layer_idx];

    // All layers in the same frame must have the same temporal_id.
    if (result.temporal_id == -1)
      result.temporal_id = layer_info.uiTemporalId;
    else if (result.temporal_id != layer_info.uiTemporalId)
      return EncoderStatus::Codes::kEncoderFailedEncode;

    size_t layer_len = 0;
    for (int nal_idx = 0; nal_idx < layer_info.iNalCount; ++nal_idx)
      layer_len += layer_info.pNalLengthInByte[nal_idx];
    if (written_size + layer_len > total_chunk_size)
      return EncoderStatus::Codes::kEncoderFailedEncode;

    memcpy(gather_buffer + written_size, layer_info.pBsBuf, layer_len);
    written_size += layer_len;
  }
  DCHECK_EQ(written_size, total_chunk_size);

  if (!h264_converter_) {
    result.size = total_chunk_size;

    output_cb_.Run(std::move(result), absl::optional<CodecDescription>());
    return EncoderStatus::Codes::kOk;
  }

  size_t converted_output_size = 0;
  bool config_changed = false;
  auto status = h264_converter_->ConvertChunk(
      conversion_buffer_,
      base::span<uint8_t>(result.data.get(), total_chunk_size), &config_changed,
      &converted_output_size);

  if (!status.is_ok())
    return EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode)
        .AddCause(std::move(status));

  result.size = converted_output_size;

  absl::optional<CodecDescription> desc;
  if (config_changed) {
    const auto& config = h264_converter_->GetCurrentConfig();
    desc = CodecDescription();
    if (!config.Serialize(desc.value())) {
      return EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
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
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                      "No frame provided for encoding."));
    return;
  }
  const bool supported_format = frame->format() == PIXEL_FORMAT_NV12 ||
                                frame->format() == PIXEL_FORMAT_I420 ||
                                frame->format() == PIXEL_FORMAT_XBGR ||
                                frame->format() == PIXEL_FORMAT_XRGB ||
                                frame->format() == PIXEL_FORMAT_ABGR ||
                                frame->format() == PIXEL_FORMAT_ARGB;
  if ((!frame->IsMappable() && !frame->HasGpuMemoryBuffer()) ||
      !supported_format) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                      "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("format", frame->format()));
    return;
  }

  if (frame->format() == PIXEL_FORMAT_NV12 && frame->HasGpuMemoryBuffer()) {
    frame = ConvertToMemoryMappedFrame(frame);
    if (!frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Convert GMB frame to MemoryMappedFrame failed."));
      return;
    }
  }

  if (frame->format() != PIXEL_FORMAT_I420) {
    // OpenH264 can resize frame automatically, but since we're converting
    // pixel fromat anyway we can do resize as well.
    auto i420_frame = frame_pool_.CreateFrame(
        PIXEL_FORMAT_I420, options_.frame_size, gfx::Rect(options_.frame_size),
        options_.frame_size, frame->timestamp());
    if (!i420_frame) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                        "Can't allocate an I420 frame."));
      return;
    }
    auto status = ConvertAndScaleFrame(*frame, *i420_frame, conversion_buffer_);
    if (!status.is_ok()) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode)
              .AddCause(std::move(status)));
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
  picture.pData[0] = frame->GetWritableVisibleData(VideoFrame::kYPlane);
  picture.pData[1] = frame->GetWritableVisibleData(VideoFrame::kUPlane);
  picture.pData[2] = frame->GetWritableVisibleData(VideoFrame::kVPlane);
  picture.iStride[0] = frame->stride(VideoFrame::kYPlane);
  picture.iStride[1] = frame->stride(VideoFrame::kUPlane);
  picture.iStride[2] = frame->stride(VideoFrame::kVPlane);

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
      options, VideoColorSpace::FromGfxColorSpace(last_frame_color_space_),
      &params);

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

  SetUpOpenH264Params(options_, itu_cs, &params);

  // It'd be nice if SetOption(ENCODER_OPTION_SVC_ENCODE_PARAM_EXT) worked, but
  // alas it doesn't seem to, so we must reinitialize.
  if (int err = codec_->InitializeExt(&params))
    DLOG(ERROR) << "Failed to reinitialize codec to set color space: " << err;
}

}  // namespace media

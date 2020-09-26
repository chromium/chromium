// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/openh264_video_encoder.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

Status SetUpOpenH264Params(const VideoEncoder::Options& options,
                           SEncParamExt* params) {
  params->bEnableFrameSkip = false;
  params->iPaddingFlag = 0;
  params->iComplexityMode = MEDIUM_COMPLEXITY;
  params->iUsageType = CAMERA_VIDEO_REAL_TIME;
  params->bEnableDenoise = false;
  // Set to 1 due to https://crbug.com/583348
  params->iMultipleThreadIdc = 1;
  params->fMaxFrameRate = options.framerate;
  params->iPicHeight = options.height;
  params->iPicWidth = options.width;

  if (options.keyframe_interval.has_value())
    params->uiIntraPeriod = options.keyframe_interval.value();

  if (options.bitrate.has_value()) {
    params->iRCMode = RC_BITRATE_MODE;
    params->iTargetBitrate = int{std::min(
        options.bitrate.value(), uint64_t{std::numeric_limits<int>::max()})};
  } else {
    params->iRCMode = RC_OFF_MODE;
  }

  params->iSpatialLayerNum = 1;
  params->sSpatialLayers[0].fFrameRate = params->fMaxFrameRate;
  params->sSpatialLayers[0].iMaxSpatialBitrate = params->iTargetBitrate;
  params->sSpatialLayers[0].iSpatialBitrate = params->iTargetBitrate;
  params->sSpatialLayers[0].iVideoHeight = params->iPicHeight;
  params->sSpatialLayers[0].iVideoWidth = params->iPicWidth;
  params->sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;

  return Status();
}
}  // namespace

OpenH264VideoEncoder::ISVCEncoderDeleter::ISVCEncoderDeleter() = default;
OpenH264VideoEncoder::ISVCEncoderDeleter::ISVCEncoderDeleter(
    const ISVCEncoderDeleter&) = default;
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
                                      OutputCB output_cb,
                                      StatusCB done_cb) {
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeTwice);
    return;
  }

  profile_ = profile;

  ISVCEncoder* raw_codec = nullptr;
  if (WelsCreateSVCEncoder(&raw_codec) != 0) {
    auto status = Status(StatusCode::kEncoderInitializationError,
                         "Failed to create OpenH264 encoder.");
    std::move(done_cb).Run(status);
    return;
  }
  svc_encoder_unique_ptr codec(raw_codec, ISVCEncoderDeleter());
  raw_codec = nullptr;

  Status status;

  SEncParamExt params = {};
  if (int err = codec->GetDefaultParams(&params)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to get default params.")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }

  status = SetUpOpenH264Params(options, &params);
  if (!status.is_ok()) {
    std::move(done_cb).Run(status);
    return;
  }

  if (int err = codec->InitializeExt(&params)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to initialize OpenH264 encoder.")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }
  codec.get_deleter().MarkInitialized();

  int video_format = EVideoFormatType::videoFormatI420;
  if (int err = codec->SetOption(ENCODER_OPTION_DATAFORMAT, &video_format)) {
    status = Status(StatusCode::kEncoderInitializationError,
                    "Failed to set data format for OpenH264 encoder")
                 .WithData("error", err);
    std::move(done_cb).Run(status);
    return;
  }

  options_ = options;
  output_cb_ = media::BindToCurrentLoop(std::move(output_cb));
  codec_ = std::move(codec);
  std::move(done_cb).Run(Status());
}

void OpenH264VideoEncoder::Encode(scoped_refptr<VideoFrame> frame,
                                  bool key_frame,
                                  StatusCB done_cb) {
  Status status;
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  if (!frame) {
    std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                  "No frame provided for encoding."));
    return;
  }
  if (!frame->IsMappable() || frame->format() != media::PIXEL_FORMAT_I420) {
    status =
        Status(StatusCode::kEncoderFailedEncode, "Unexpected frame format.")
            .WithData("IsMappable", frame->IsMappable())
            .WithData("format", frame->format());
    std::move(done_cb).Run(std::move(status));
    return;
  }

  SSourcePicture picture = {};
  picture.iPicWidth = frame->visible_rect().width();
  picture.iPicHeight = frame->visible_rect().height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp = frame->timestamp().InMilliseconds();
  picture.pData[0] = frame->visible_data(VideoFrame::kYPlane);
  picture.pData[1] = frame->visible_data(VideoFrame::kUPlane);
  picture.pData[2] = frame->visible_data(VideoFrame::kVPlane);
  picture.iStride[0] = frame->stride(VideoFrame::kYPlane);
  picture.iStride[1] = frame->stride(VideoFrame::kUPlane);
  picture.iStride[2] = frame->stride(VideoFrame::kVPlane);

  if (key_frame) {
    if (int err = codec_->ForceIntraFrame(true)) {
      std::move(done_cb).Run(
          Status(StatusCode::kEncoderFailedEncode, "Can't make keyframe.")
              .WithData("error", err));
      return;
    }
  }

  SFrameBSInfo frame_info = {};
  if (int err = codec_->EncodeFrame(&picture, &frame_info)) {
    std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                  "Failed to encode using OpenH264.")
                               .WithData("error", err));
    return;
  }

  VideoEncoderOutput result;
  result.key_frame = (frame_info.eFrameType == videoFrameTypeIDR);
  result.timestamp = frame->timestamp();

  DCHECK_GT(frame_info.iFrameSizeInBytes, 0);
  size_t total_chunk_size = frame_info.iFrameSizeInBytes;
  conversion_buffer_.resize(total_chunk_size);

  size_t written_size = 0;
  for (int layer_idx = 0; layer_idx < frame_info.iLayerNum; ++layer_idx) {
    SLayerBSInfo& layer_info = frame_info.sLayerInfo[layer_idx];
    size_t layer_len = 0;
    for (int nal_idx = 0; nal_idx < layer_info.iNalCount; ++nal_idx)
      layer_len += layer_info.pNalLengthInByte[nal_idx];
    if (written_size + layer_len > total_chunk_size) {
      std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                    "Inconsistent size of the encoded frame."));
      return;
    }

    memcpy(conversion_buffer_.data() + written_size, layer_info.pBsBuf,
           layer_len);
    written_size += layer_len;
  }
  DCHECK_EQ(written_size, total_chunk_size);

  size_t converted_output_size = 0;
  bool config_changed = false;
  result.data.reset(new uint8_t[total_chunk_size]);
  status = h264_converter_.ConvertChunk(
      conversion_buffer_,
      base::span<uint8_t>(result.data.get(), total_chunk_size), &config_changed,
      &converted_output_size);

  if (!status.is_ok()) {
    std::move(done_cb).Run(std::move(status).AddHere(FROM_HERE));
    return;
  }
  result.size = converted_output_size;

  base::Optional<CodecDescription> desc;
  if (config_changed) {
    const auto& config = h264_converter_.GetCurrentConfig();
    desc = CodecDescription();
    if (!config.Serialize(desc.value())) {
      std::move(done_cb).Run(Status(StatusCode::kEncoderFailedEncode,
                                    "Failed to serialize AVC decoder config"));
      return;
    }
  }

  output_cb_.Run(std::move(result), std::move(desc));
  std::move(done_cb).Run(Status());
}

void OpenH264VideoEncoder::ChangeOptions(const Options& options,
                                         StatusCB done_cb) {
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  // TODO(eugene): Not implemented yet.

  std::move(done_cb).Run(Status());
}

void OpenH264VideoEncoder::Flush(StatusCB done_cb) {
  done_cb = media::BindToCurrentLoop(std::move(done_cb));
  if (!codec_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  // Nothing to do really.
  std::move(done_cb).Run(Status());
}

}  // namespace media

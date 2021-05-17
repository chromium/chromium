// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_wrapper.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/media/base/vp9_profile.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

#if defined(USE_H264_ENCODER)
#include "remoting/codec/webrtc_video_encoder_gpu.h"
#endif

namespace remoting {
namespace protocol {

namespace {

constexpr base::TimeDelta kTargetFrameInterval =
    base::TimeDelta::FromMilliseconds(1000 / kTargetFrameRate);

// Maximum quantizer at which to encode frames. Lowering this value will
// improve image quality (in cases of low-bandwidth or large frames) at the
// cost of latency. Increasing the value will improve latency (in these cases)
// at the cost of image quality, resulting in longer top-off times.
const int kMaxQuantizer = 50;

// Minimum quantizer at which to encode frames. The value is chosen such that
// sending higher-quality (lower quantizer) frames would use up bandwidth
// without any appreciable gain in image quality.
const int kMinQuantizer = 10;

std::string EncodeResultToString(WebrtcVideoEncoder::EncodeResult result) {
  using EncodeResult = WebrtcVideoEncoder::EncodeResult;

  switch (result) {
    case EncodeResult::SUCCEEDED:
      return "Succeeded";
    case EncodeResult::FRAME_SIZE_EXCEEDS_CAPABILITY:
      return "Frame size exceeds capability";
    case EncodeResult::UNKNOWN_ERROR:
      return "Unknown error";
  }
  NOTREACHED();
  return "";
}

}  // namespace

WebrtcVideoEncoderWrapper::WebrtcVideoEncoderWrapper(
    const webrtc::SdpVideoFormat& format,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer)
    : main_task_runner_(main_task_runner),
      video_channel_state_observer_(video_channel_state_observer) {
  codec_type_ = webrtc::PayloadStringToCodecType(format.name);
  switch (codec_type_) {
    case webrtc::kVideoCodecVP8:
      VLOG(0) << "Creating VP8 encoder.";
      encoder_ = WebrtcVideoEncoderVpx::CreateForVP8();
      break;
    case webrtc::kVideoCodecVP9: {
      const auto iter = format.parameters.find(webrtc::kVP9FmtpProfileId);
      bool lossless_color =
          iter != format.parameters.end() && iter->second == "1";
      VLOG(0) << "Creating VP9 encoder, lossless_color="
              << (lossless_color ? "true" : "false");
      encoder_ = WebrtcVideoEncoderVpx::CreateForVP9();
      encoder_->SetLosslessColor(lossless_color);
      break;
    }
    case webrtc::kVideoCodecH264:
#if defined(USE_H264_ENCODER)
      VLOG(0) << "Creating H264 encoder.";
      encoder_ = WebrtcVideoEncoderGpu::CreateForH264();
#else
      NOTIMPLEMENTED();
#endif
      break;
    default:
      LOG(FATAL) << "Unknown codec type: " << codec_type_;
  }
}

WebrtcVideoEncoderWrapper::~WebrtcVideoEncoderWrapper() = default;

int32_t WebrtcVideoEncoderWrapper::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    const webrtc::VideoEncoder::Settings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(codec_settings);
  DCHECK_EQ(codec_settings->codecType, codec_type_);

  // Validate request is to support a single stream.
  DCHECK_EQ(1, codec_settings->numberOfSimulcastStreams);

  if (codec_type_ == webrtc::kVideoCodecVP9) {
    // SVC is not supported.
    DCHECK_EQ(1, codec_settings->VP9().numberOfSpatialLayers);
  }

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoderWrapper::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoded_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoderWrapper::Release() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  encoded_callback_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcVideoEncoderWrapper::Encode(
    const webrtc::VideoFrame& frame,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Frames of type kNative are expected to have the adapter that was used to
  // wrap the DesktopFrame, so the downcast should be safe.
  if (frame.video_frame_buffer()->type() !=
      webrtc::VideoFrameBuffer::Type::kNative) {
    LOG(ERROR) << "Only kNative frames are supported.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  auto* video_frame_adapter =
      static_cast<WebrtcVideoFrameAdapter*>(frame.video_frame_buffer().get());

  // Store timestamp so it can be added to the EncodedImage when encoding is
  // complete.
  rtp_timestamp_ = frame.timestamp();

  // TODO(crbug.com/1192865): Implement large-frame detection for VP8, and
  // ensure VP9 is configured to do this automatically. If the frame has a
  // large update-region, it should be encoded at a lower quality to keep
  // latency down, and topped off later.
  WebrtcVideoEncoder::FrameParams frame_params;

  // SetRates() must be called prior to Encode(), with a non-zero bitrate.
  DCHECK_NE(0, bitrate_kbps_);
  frame_params.bitrate_kbps = bitrate_kbps_;
  frame_params.duration = kTargetFrameInterval;

  // TODO(crbug.com/1192865): Copy the FPS estimator from the scheduler,
  // instead of hard-coding this value here.
  frame_params.fps = kTargetFrameRate;

  frame_params.vpx_min_quantizer = kMinQuantizer;
  frame_params.vpx_max_quantizer = kMaxQuantizer;
  frame_params.clear_active_map = !top_off_active_;

  // Simulcast is unsupported, so only the first vector element is needed.
  frame_params.key_frame =
      (frame_types && !frame_types->empty() &&
       ((*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey));

  // Just in case the encoder runs the callback on an arbitrary thread,
  // BindPostTask() is used here to trampoline onto the correct thread.
  // This object is bound via a WeakPtr which must only be dereferenced
  // on the current thread.
  auto encode_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&WebrtcVideoEncoderWrapper::OnFrameEncoded,
                     weak_factory_.GetWeakPtr()));
  encoder_->Encode(video_frame_adapter->TakeDesktopFrame(), frame_params,
                   std::move(encode_callback));
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcVideoEncoderWrapper::SetRates(
    const RateControlParameters& parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int bitrate_kbps = parameters.bitrate.get_sum_kbps();
  if (bitrate_kbps_ != bitrate_kbps) {
    bitrate_kbps_ = bitrate_kbps;
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoChannelStateObserver::OnTargetBitrateChanged,
                       video_channel_state_observer_, bitrate_kbps));
  }
}

void WebrtcVideoEncoderWrapper::OnRttUpdate(int64_t rtt_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoChannelStateObserver::OnRttUpdate,
                                video_channel_state_observer_,
                                base::TimeDelta::FromMilliseconds(rtt_ms)));
}

webrtc::VideoEncoder::EncoderInfo WebrtcVideoEncoderWrapper::GetEncoderInfo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return EncoderInfo();
}

webrtc::EncodedImageCallback::Result
WebrtcVideoEncoderWrapper::ReturnEncodedFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const uint8_t* buffer =
      reinterpret_cast<const uint8_t*>(base::data(frame.data));
  size_t buffer_size = frame.data.size();

  // TODO(crbug.com/1208215): Avoid copying/allocating frame data here, by
  // implementing EncodedImageBufferInterface.
  webrtc::EncodedImage encoded_image;
  encoded_image.SetEncodedData(
      webrtc::EncodedImageBuffer::Create(buffer, buffer_size));
  encoded_image._encodedWidth = frame.size.width();
  encoded_image._encodedHeight = frame.size.height();
  encoded_image._frameType = frame.key_frame
                                 ? webrtc::VideoFrameType::kVideoFrameKey
                                 : webrtc::VideoFrameType::kVideoFrameDelta;
  encoded_image.SetTimestamp(rtp_timestamp_);
  encoded_image.playout_delay_.min_ms = 0;
  encoded_image.playout_delay_.max_ms = 0;
  encoded_image.content_type_ = webrtc::VideoContentType::SCREENSHARE;

  webrtc::CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = frame.codec;

  if (frame.codec == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfoVP8* vp8_info =
        &codec_specific_info.codecSpecific.VP8;
    vp8_info->temporalIdx = webrtc::kNoTemporalIdx;
  } else if (frame.codec == webrtc::kVideoCodecVP9) {
    webrtc::CodecSpecificInfoVP9* vp9_info =
        &codec_specific_info.codecSpecific.VP9;
    vp9_info->inter_pic_predicted = !frame.key_frame;
    vp9_info->ss_data_available = frame.key_frame;
    vp9_info->spatial_layer_resolution_present = frame.key_frame;
    if (frame.key_frame) {
      vp9_info->width[0] = frame.size.width();
      vp9_info->height[0] = frame.size.height();
    }
    vp9_info->num_spatial_layers = 1;
    vp9_info->gof_idx = webrtc::kNoGofIdx;
    vp9_info->temporal_idx = webrtc::kNoTemporalIdx;
    vp9_info->flexible_mode = false;
    vp9_info->temporal_up_switch = true;
    vp9_info->inter_layer_predicted = false;
    vp9_info->first_frame_in_picture = true;
    vp9_info->end_of_picture = true;
    vp9_info->spatial_layer_resolution_present = false;
  } else if (frame.codec == webrtc::kVideoCodecH264) {
#if defined(USE_H264_ENCODER)
    webrtc::CodecSpecificInfoH264* h264_info =
        &codec_specific_info.codecSpecific.H264;
    h264_info->packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;
#else
    NOTREACHED();
#endif
  } else {
    NOTREACHED();
  }

  DCHECK(encoded_callback_);
  return encoded_callback_->OnEncodedImage(encoded_image, &codec_specific_info);
}

void WebrtcVideoEncoderWrapper::OnFrameEncoded(
    WebrtcVideoEncoder::EncodeResult encode_result,
    std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> encoded_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Keep |encoded_frame| alive until frame-encoded/frame-sent notifications
  // have executed on |main_task_runner_|.
  std::unique_ptr<WebrtcVideoEncoder::EncodedFrame, base::OnTaskRunnerDeleter>
      frame(encoded_frame.release(),
            base::OnTaskRunnerDeleter(main_task_runner_));

  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoChannelStateObserver::OnFrameEncoded,
                                video_channel_state_observer_, encode_result,
                                frame.get()));

  if (encode_result != WebrtcVideoEncoder::EncodeResult::SUCCEEDED) {
    // TODO(crbug.com/1192865): Store this error and communicate it to WebRTC
    // via the next call to Encode(). The VPX encoders are never expected to
    // return any error, but hardware-decoders such as H264 may fail.
    LOG(ERROR) << "Video encoder returned error "
               << EncodeResultToString(encode_result);
    return;
  }

  if (!frame || frame->data.empty()) {
    SetTopOffActive(false);
    return;
  }

  // Top-off until the best quantizer value is reached.
  SetTopOffActive(frame->quantizer > kMinQuantizer);

  // Non-null, because WebRTC registers a callback before calling Encode().
  DCHECK(encoded_callback_);

  webrtc::EncodedImageCallback::Result send_result = ReturnEncodedFrame(*frame);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoChannelStateObserver::OnEncodedFrameSent,
                     video_channel_state_observer_, send_result, *frame));
}

void WebrtcVideoEncoderWrapper::SetTopOffActive(bool active) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (top_off_active_ != active) {
    top_off_active_ = active;
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoChannelStateObserver::OnTopOffActive,
                                  video_channel_state_observer_, active));
  }
}

}  // namespace protocol
}  // namespace remoting

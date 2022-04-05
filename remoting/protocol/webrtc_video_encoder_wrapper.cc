// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_encoder_wrapper.h"

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "remoting/protocol/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
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
    base::Milliseconds(1000 / kTargetFrameRate);

// Maximum quantizer at which to encode frames. Lowering this value will
// improve image quality (in cases of low-bandwidth or large frames) at the
// cost of latency. Increasing the value will improve latency (in these cases)
// at the cost of image quality, resulting in longer top-off times.
const int kMaxQuantizer = 50;

// Minimum quantizer at which to encode frames. The value is chosen such that
// sending higher-quality (lower quantizer) frames would use up bandwidth
// without any appreciable gain in image quality.
const int kMinQuantizer = 10;

const int64_t kPixelsPerMegapixel = 1000000;

// Threshold in number of updated pixels used to detect "big" frames. These
// frames update significant portion of the screen compared to the preceding
// frames. For these frames min quantizer may need to be adjusted in order to
// ensure that they get delivered to the client as soon as possible, in exchange
// for lower-quality image.
const int kBigFrameThresholdPixels = 300000;

// Estimated size (in bytes per megapixel) of encoded frame at target quantizer
// value (see kTargetQuantizerForTopOff). Compression ratio varies depending
// on the image, so this is just a rough estimate. It's used to predict when
// encoded "big" frame may be too large to be delivered to the client quickly.
const int kEstimatedBytesPerMegapixel = 100000;

// Minimum interval between frames needed to keep the connection alive. The
// client will request a key-frame if it does not receive any frames for a
// 3-second period. This is effectively a minimum frame-rate, so the value
// should not be too small, otherwise the client may waste CPU cycles on
// processing and rendering lots of identical frames.
constexpr base::TimeDelta kKeepAliveInterval = base::Seconds(2);

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

WebrtcVideoEncoderWrapper::~WebrtcVideoEncoderWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebrtcVideoEncoderWrapper::SetEncoderForTest(
    std::unique_ptr<WebrtcVideoEncoder> encoder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  encoder_ = std::move(encoder);
}

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

  auto now = base::TimeTicks::Now();

  // Simulcast is unsupported, so only the first vector element is needed.
  bool key_frame_requested =
      (frame_types && !frame_types->empty() &&
       ((*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey));
  if (key_frame_requested) {
    pending_key_frame_request_ = true;
  }

  bool webrtc_dropped_frame = false;
  if (next_frame_id_ != frame.id()) {
    webrtc_dropped_frame = true;
    next_frame_id_ = frame.id();
  }
  next_frame_id_++;

  if (encode_pending_) {
    accumulated_update_rect_.Union(frame.update_rect());
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebrtcVideoEncoderWrapper::NotifyFrameDropped,
                       weak_factory_.GetWeakPtr()));
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Frames of type kNative are expected to have the adapter that was used to
  // wrap the DesktopFrame, so the downcast should be safe.
  if (frame.video_frame_buffer()->type() !=
      webrtc::VideoFrameBuffer::Type::kNative) {
    LOG(ERROR) << "Only kNative frames are supported.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  auto* video_frame_adapter =
      static_cast<WebrtcVideoFrameAdapter*>(frame.video_frame_buffer().get());

  // Store RTP timestamp and FrameStats so they can be added to the
  // EncodedImage and EncodedFrame when encoding is complete.
  rtp_timestamp_ = frame.timestamp();
  frame_stats_ = video_frame_adapter->TakeFrameStats();
  if (!frame_stats_) {
    // This could happen if WebRTC tried to encode the same frame twice.
    // Taking the frame-stats twice from the same frame-adapter would return
    // nullptr the second time.
    LOG(ERROR) << "Frame provided with missing frame-stats.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  frame_stats_->encode_started_time = now;

  auto desktop_frame = video_frame_adapter->TakeDesktopFrame();

  // If any frames were dropped by WebRTC or by this class, the
  // original DesktopFrame's updated-region should not be used as-is
  // (because that region is the difference between this frame and the
  // previous frame, which the encoder has not seen because it was dropped).
  // In this case, the DesktopFrame's update-region should be set to the
  // union of all the dropped frames' update-rectangles.
  bool this_class_dropped_frame = !accumulated_update_rect_.IsEmpty();
  if (webrtc_dropped_frame || this_class_dropped_frame) {
    // Get the update-rect that WebRTC provides, which will include any
    // accumulated updates from frames that WebRTC dropped.
    auto update_rect = frame.update_rect();

    // Combine it with any updates from frames dropped by this class.
    update_rect.Union(accumulated_update_rect_);

    // In case the new frame has a different resolution, ensure the update-rect
    // is constrained by the frame's bounds. On the first frame with a new
    // resolution, WebRTC sets the update-rect to the full area of the frame, so
    // this line will give the correct result in that case. If the resolution
    // did not change (for this frame or any prior dropped frames), the
    // update-region will already be constrained by the resolution, so this line
    // will be a no-op.
    update_rect.Intersect(
        webrtc::VideoFrame::UpdateRect{0, 0, frame.width(), frame.height()});

    desktop_frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeXYWH(update_rect.offset_x,
                                      update_rect.offset_y, update_rect.width,
                                      update_rect.height));

    // The update-region has now been applied to the desktop_frame which is
    // being sent to the encoder, so empty it here.
    accumulated_update_rect_.MakeEmptyUpdate();
  }

  // Limit the encoding and sending of empty frames to |kKeepAliveInterval|.
  // This is done to save on network bandwidth and CPU usage.
  if (desktop_frame->updated_region().is_empty() && !top_off_active_ &&
      !pending_key_frame_request_ &&
      (now - latest_frame_encode_start_time_ < kKeepAliveInterval)) {
    // Drop the frame. There is no need to track the update-rect as the
    // frame being dropped is empty.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebrtcVideoEncoderWrapper::NotifyFrameDropped,
                       weak_factory_.GetWeakPtr()));
    return WEBRTC_VIDEO_CODEC_OK;
  }
  latest_frame_encode_start_time_ = now;

  WebrtcVideoEncoder::FrameParams frame_params;

  // SetRates() must be called prior to Encode(), with a non-zero bitrate.
  DCHECK_NE(0, bitrate_kbps_);
  frame_params.bitrate_kbps = bitrate_kbps_;
  frame_params.duration = kTargetFrameInterval;

  // TODO(crbug.com/1192865): Copy the FPS estimator from the scheduler,
  // instead of hard-coding this value here.
  frame_params.fps = kTargetFrameRate;

  frame_params.vpx_min_quantizer =
      ShouldDropQualityForLargeFrame(*desktop_frame) ? kMaxQuantizer
                                                     : kMinQuantizer;
  frame_params.vpx_max_quantizer = kMaxQuantizer;
  frame_params.clear_active_map = !top_off_active_;

  frame_params.key_frame = pending_key_frame_request_;
  pending_key_frame_request_ = false;

  encode_pending_ = true;

  // Just in case the encoder runs the callback on an arbitrary thread,
  // BindPostTask() is used here to trampoline onto the correct thread.
  // This object is bound via a WeakPtr which must only be dereferenced
  // on the current thread.
  auto encode_callback = base::BindPostTask(
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&WebrtcVideoEncoderWrapper::OnFrameEncoded,
                     weak_factory_.GetWeakPtr()));
  encoder_->Encode(std::move(desktop_frame), frame_params,
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

  rtt_estimate_ = base::Milliseconds(rtt_ms);
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
      reinterpret_cast<const uint8_t*>(std::data(frame.data));
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

  DCHECK(encode_pending_);
  encode_pending_ = false;

  if (frame) {
    // This is non-null because the |encode_pending_| flag ensures that
    // frame-encodings are serialized. So there cannot be 2 consecutive calls to
    // this method without an intervening call to Encode() which sets
    // |frame_stats_| to non-null.
    DCHECK(frame_stats_);
    frame_stats_->encode_ended_time = base::TimeTicks::Now();
    frame_stats_->rtt_estimate = rtt_estimate_;
    frame_stats_->bandwidth_estimate_kbps = bitrate_kbps_;
    frame->stats = std::move(frame_stats_);
  }

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
    NotifyFrameDropped();
    return;
  }

  if (!frame || frame->data.empty()) {
    top_off_active_ = false;
    NotifyFrameDropped();
    return;
  }

  // Top-off until the best quantizer value is reached.
  top_off_active_ = (frame->quantizer > kMinQuantizer);

  // Non-null, because WebRTC registers a callback before calling Encode().
  DCHECK(encoded_callback_);

  webrtc::EncodedImageCallback::Result send_result = ReturnEncodedFrame(*frame);

  // std::ref() is used here because base::BindOnce() would otherwise try to
  // copy the referenced frame object, which is move-only. This is safe because
  // base::OnTaskRunnerDeleter posts the frame-deleter task to run after this
  // task has executed.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoChannelStateObserver::OnEncodedFrameSent,
                                video_channel_state_observer_, send_result,
                                std::ref(*frame)));
}

void WebrtcVideoEncoderWrapper::NotifyFrameDropped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(encoded_callback_);
  encoded_callback_->OnDroppedFrame(
      webrtc::EncodedImageCallback::DropReason::kDroppedByEncoder);
}

bool WebrtcVideoEncoderWrapper::ShouldDropQualityForLargeFrame(
    const webrtc::DesktopFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (codec_type_ != webrtc::kVideoCodecVP8) {
    return false;
  }

  int64_t updated_area = 0;
  for (webrtc::DesktopRegion::Iterator r(frame.updated_region()); !r.IsAtEnd();
       r.Advance()) {
    updated_area += r.rect().width() * r.rect().height();
  }

  bool should_drop_quality = false;
  if (updated_area - updated_region_area_.Max() > kBigFrameThresholdPixels) {
    int expected_frame_size =
        updated_area * kEstimatedBytesPerMegapixel / kPixelsPerMegapixel;
    base::TimeDelta expected_send_delay =
        base::Seconds(expected_frame_size * 8 / (bitrate_kbps_ * 1000.0));
    if (expected_send_delay > kTargetFrameInterval) {
      should_drop_quality = true;
    }
  }

  updated_region_area_.Record(updated_area);
  return should_drop_quality;
}

}  // namespace protocol
}  // namespace remoting
